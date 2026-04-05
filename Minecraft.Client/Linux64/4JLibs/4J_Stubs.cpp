/*
 * 4J_Stubs.cpp
 *
 * Stub / no-op implementations for all 4J library classes used by
 * the Linux 64-bit port of Minecraft Console Edition.
 *
 * Every public method declared in the four 4J headers is implemented
 * here so that the game links without pulling in platform-specific
 * libraries.  All functions are intentionally minimal: they return safe
 * default values and perform no real work.
 */

#include "stdafx.h"

#include <cstring>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <cmath>
#include <cstdint>
#include <atomic>
#include <limits>
#include <array>
#include <png.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "gl_api.h"
#include "vk_backend.h"
#include "vk_pipeline.h"
#include "vk_texture.h"
#include "../SessionLog.h"

/* ====================================================================
 * Global singleton instances
 * ==================================================================== */

C4JRender  RenderManager;
C_4JInput  InputManager;
C4JStorage StorageManager;
C_4JProfile ProfileManager;

extern int g_iScreenWidth;
extern int g_iScreenHeight;
extern float g_iAspectRatio;

/* ********************************************************************
 *
 *  C4JRender  --  Render Manager stubs
 *
 * ********************************************************************/

namespace
{
struct DrawCall
{
    C4JRender::ePrimitiveType primitive;
    int count;
    C4JRender::eVertexType vtype;
    std::vector<unsigned char> bytes;
    bool hasMatrices = false;
    bool useVertexColor = true; // false = use current GL color state, not per-vertex
    float modelview[16];
    float projection[16];
    int textureId = -1;
    float globalUV[2] = {0.0f, 0.0f};
};

struct TextureInfo
{
    unsigned int glId = 0;
};

GLFWwindow *g_window = nullptr;
std::atomic<bool> g_glReady{false};
std::thread::id g_renderThread;
float g_clearRGBA[4] = {0.0f, 0.0f, 0.0f, 1.0f};
int g_textureLevels = 1;
int g_currentTexture = -1;
bool g_textureEnabled = true; // tracks glEnable/glDisable(GL_TEXTURE_2D) state
int g_nextTextureId = 1;
int g_nextCBuffId = 1;
std::unordered_map<int, TextureInfo> g_textures;
std::unordered_map<int, std::vector<DrawCall>> g_cbuffs;
std::mutex g_cbuffMutex;
thread_local int tl_recordingCbuff = -1;
thread_local std::vector<DrawCall> tl_recordingCalls;
thread_local int tl_currentTexture = -1;
thread_local bool tl_nextDrawHasVertexColor = true; // set by Tesselator before DrawVertices
thread_local float tl_savedModelForRecording[16]; // saved MV at CBuffStart for restore at CBuffEnd

// Scroll callback for input (must be defined before ensureGLReady)
static float s_scrollAccum = 0;
static void inputScrollCallback(GLFWwindow*, double /*xoff*/, double yoff)
{
    s_scrollAccum += (float)yoff;
}

// ── Render state for Vulkan pipeline selection + push constants ──
struct RenderState {
    bool   blendEnable = false;
    int    blendSrc = 6; // VK_BLEND_FACTOR_SRC_ALPHA
    int    blendDst = 7; // VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA
    bool   depthTestEnable = true;
    bool   depthWriteEnable = true;
    bool   alphaTestEnable = false;
    float  alphaRef = 0.1f;
    bool   fogEnable = false;
    int    fogMode = 0; // 0=linear, 1=exp
    float  fogNear = 0.0f, fogFar = 256.0f, fogDensity = 1.0f;
    float  fogColor[3] = {0,0,0};
    float  globalColor[4] = {1,1,1,1};
    uint8_t colorWriteMask = 0xF; // RGBA
    float  depthBiasSlope = 0.0f, depthBiasConst = 0.0f;
};
static RenderState g_renderState;
thread_local float tl_globalUV[2] = {0.0f, 0.0f};
std::atomic<int> g_sharedTextureId(-1);
std::atomic<float> g_sharedUVU(0.0f);
std::atomic<float> g_sharedUVV(0.0f);
struct SoftMatrixState
{
    bool init = false;
    int mode = GL_MODELVIEW;
    float model[16];
    float proj[16];
    float tex[16];
    std::vector<std::array<float, 16>> modelStack;
    std::vector<std::array<float, 16>> projStack;
    std::vector<std::array<float, 16>> texStack;
};
thread_local SoftMatrixState tl_softMatrices;

static unsigned int mapPrimitive(C4JRender::ePrimitiveType primitive)
{
    switch (primitive)
    {
    case C4JRender::PRIMITIVE_TYPE_LINE_LIST: return 0x0001;       // GL_LINES
    case C4JRender::PRIMITIVE_TYPE_LINE_STRIP: return 0x0003;      // GL_LINE_STRIP
    case C4JRender::PRIMITIVE_TYPE_TRIANGLE_STRIP: return 0x0005;  // GL_TRIANGLE_STRIP
    case C4JRender::PRIMITIVE_TYPE_TRIANGLE_FAN: return 0x0006;    // GL_TRIANGLE_FAN
    case C4JRender::PRIMITIVE_TYPE_QUAD_LIST: return 0x0007;       // GL_QUADS
    case C4JRender::PRIMITIVE_TYPE_TRIANGLE_LIST:
    default: return 0x0004;                                         // GL_TRIANGLES
    }
}

static unsigned int mapMatrixMode(int mode)
{
    switch (mode)
    {
    case GL_PROJECTION: return 0x1701; // GL_PROJECTION
    case GL_TEXTURE: return 0x1702;    // GL_TEXTURE
    case GL_MODELVIEW:
    default: return 0x1700;            // GL_MODELVIEW
    }
}

static size_t strideForType(C4JRender::eVertexType type)
{
    return (type == C4JRender::VERTEX_TYPE_COMPRESSED) ? 16u : 32u;
}

static void decode565(unsigned short packed, unsigned char &r, unsigned char &g, unsigned char &b)
{
    r = (unsigned char)((((packed >> 11) & 0x1F) * 255 + 15) / 31);
    g = (unsigned char)((((packed >> 5) & 0x3F) * 255 + 31) / 63);
    b = (unsigned char)(((packed & 0x1F) * 255 + 15) / 31);
}

// Overworld brightness ramp: ramp[i] = (i/15) / (3*(1-i/15) + 1), ambientLight=0
// Maps light level 0-15 to brightness 0.0-1.0
static const float s_brightnessRamp[16] = {
    0.0f,      0.01754f, 0.03704f, 0.05882f,
    0.08333f,  0.11111f, 0.14286f, 0.17949f,
    0.22222f,  0.27273f, 0.33333f, 0.40741f,
    0.50000f,  0.61905f, 0.77778f, 1.00000f
};

static void matIdentity(float m[16])
{
    memset(m, 0, sizeof(float) * 16);
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static void matCopy(float dst[16], const float src[16])
{
    memcpy(dst, src, sizeof(float) * 16);
}

static void matMul(float out[16], const float a[16], const float b[16])
{
    // Column-major 4x4 multiply: out = a * b
    for (int col = 0; col < 4; ++col)
    {
        for (int row = 0; row < 4; ++row)
        {
            out[col * 4 + row] =
                a[0 * 4 + row] * b[col * 4 + 0] +
                a[1 * 4 + row] * b[col * 4 + 1] +
                a[2 * 4 + row] * b[col * 4 + 2] +
                a[3 * 4 + row] * b[col * 4 + 3];
        }
    }
}

static void matPostMul(float io[16], const float rhs[16])
{
    float out[16];
    matMul(out, io, rhs);
    matCopy(io, out);
}

static void ensureSoftMatrices()
{
    if (tl_softMatrices.init)
        return;
    tl_softMatrices.init = true;
    tl_softMatrices.mode = GL_MODELVIEW;
    matIdentity(tl_softMatrices.model);
    matIdentity(tl_softMatrices.proj);
    matIdentity(tl_softMatrices.tex);
}

static float *currentSoftMatrix()
{
    ensureSoftMatrices();
    if (tl_softMatrices.mode == GL_PROJECTION || tl_softMatrices.mode == GL_PROJECTION_MATRIX)
        return tl_softMatrices.proj;
    if (tl_softMatrices.mode == GL_TEXTURE)
        return tl_softMatrices.tex;
    return tl_softMatrices.model;
}

static std::vector<std::array<float, 16>> &currentSoftStack()
{
    ensureSoftMatrices();
    if (tl_softMatrices.mode == GL_PROJECTION || tl_softMatrices.mode == GL_PROJECTION_MATRIX)
        return tl_softMatrices.projStack;
    if (tl_softMatrices.mode == GL_TEXTURE)
        return tl_softMatrices.texStack;
    return tl_softMatrices.modelStack;
}

static void softTranslate(float x, float y, float z)
{
    float t[16];
    matIdentity(t);
    t[12] = x;
    t[13] = y;
    t[14] = z;
    matPostMul(currentSoftMatrix(), t);
    // (debug translate logging removed)
}

static void softScale(float x, float y, float z)
{
    float s[16];
    matIdentity(s);
    s[0] = x;
    s[5] = y;
    s[10] = z;
    matPostMul(currentSoftMatrix(), s);
}

static void softRotateRadians(float angleRad, float x, float y, float z)
{
    float len = std::sqrt(x * x + y * y + z * z);
    if (len <= 1e-8f)
        return;
    x /= len; y /= len; z /= len;
    const float c = std::cos(angleRad);
    const float s = std::sin(angleRad);
    const float t = 1.0f - c;

    float r[16];
    matIdentity(r);
    r[0] = t * x * x + c;
    r[4] = t * x * y - s * z;
    r[8] = t * x * z + s * y;
    r[1] = t * x * y + s * z;
    r[5] = t * y * y + c;
    r[9] = t * y * z - s * x;
    r[2] = t * x * z - s * y;
    r[6] = t * y * z + s * x;
    r[10] = t * z * z + c;
    matPostMul(currentSoftMatrix(), r);
}

static void softOrtho(float left, float right, float bottom, float top, float zNear, float zFar)
{
    const float rl = right - left;
    const float tb = top - bottom;
    const float fn = zFar - zNear;
    if (std::fabs(rl) <= 1e-8f || std::fabs(tb) <= 1e-8f || std::fabs(fn) <= 1e-8f)
        return;

    // Right-handed (-Z forward) → Vulkan NDC: depth [0,1], Y-flipped
    float o[16];
    matIdentity(o);
    o[0]  =  2.0f / rl;
    o[5]  = -2.0f / tb;              // Y-flip for Vulkan
    o[10] = -1.0f / fn;              // RH: -Z forward, depth [0,1]
    o[12] = -(right + left) / rl;
    o[13] =  (top + bottom) / tb;    // Y-flip for Vulkan
    o[14] = -zNear / fn;             // depth [0,1]
    matPostMul(currentSoftMatrix(), o);
}

static void softFrustum(float left, float right, float bottom, float top, float zNear, float zFar)
{
    const float rl = right - left;
    const float tb = top - bottom;
    const float fn = zFar - zNear;
    if (std::fabs(rl) <= 1e-8f || std::fabs(tb) <= 1e-8f || std::fabs(fn) <= 1e-8f || zNear <= 0.0f || zFar <= 0.0f)
        return;

    // Right-handed (-Z forward) → Vulkan NDC: depth [0,1], Y-flipped
    // The game's modelview produces negative z_view for objects in front
    // of the camera (standard OpenGL RH convention: glRotatef(yaw+180,...)).
    float f[16];
    memset(f, 0, sizeof(f));
    f[0]  =  (2.0f * zNear) / rl;
    f[5]  = -(2.0f * zNear) / tb;    // Y-flip for Vulkan
    f[8]  =  (right + left) / rl;
    f[9]  = -(top + bottom) / tb;     // Y-flip for Vulkan
    f[10] = -zFar / fn;               // RH: -Z forward, depth [0,1]
    f[11] = -1.0f;                     // RH: clip_w = -z_view
    f[14] = -(zFar * zNear) / fn;     // depth [0,1]
    matPostMul(currentSoftMatrix(), f);
}

static void onGlfwError(int code, const char *desc)
{
    SessionLog_Printf("[linuxgl] GLFW error %d: %s\n", code, desc ? desc : "(null)");
}

static std::mutex g_initMutex;
static bool ensureGLReady()
{
    if (g_glReady.load(std::memory_order_acquire))
        return true;

    std::lock_guard<std::mutex> lock(g_initMutex);
    if (g_glReady.load(std::memory_order_relaxed))
        return true;

    static bool glfwInitDone = false;
    if (!glfwInitDone)
    {
        glfwSetErrorCallback(onGlfwError);
        if (!glfwInit())
        {
            SessionLog_Printf("[vk] glfwInit failed\n");
            return false;
        }
        glfwInitDone = true;
    }

    // Create window with NO API — Vulkan provides rendering.
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    int w = (g_iScreenWidth > 0) ? g_iScreenWidth : 1280;
    int h = (g_iScreenHeight > 0) ? g_iScreenHeight : 720;
    if (w < 640) w = 640;
    if (h < 480) h = 480;

    g_window = glfwCreateWindow(w, h, "Minecraft Console Edition", nullptr, nullptr);
    if (!g_window)
        g_window = glfwCreateWindow(1280, 720, "Minecraft Console Edition", nullptr, nullptr);
    if (!g_window)
        g_window = glfwCreateWindow(800, 600, "Minecraft Console Edition", nullptr, nullptr);
    if (!g_window)
    {
        SessionLog_Printf("[vk] failed to create GLFW window\n");
        return false;
    }

    glfwShowWindow(g_window);
    glfwGetFramebufferSize(g_window, &g_iScreenWidth, &g_iScreenHeight);
    if (g_iScreenWidth < 1) g_iScreenWidth = 1;
    if (g_iScreenHeight < 1) g_iScreenHeight = 1;
    g_iAspectRatio = (float)g_iScreenWidth / (float)g_iScreenHeight;

    // Initialize Vulkan backend
    if (!vkb_init(g_window))
    {
        SessionLog_Printf("[vk] Vulkan init failed\n");
        return false;
    }

    // Set up input callbacks now that window exists
    glfwSetScrollCallback(g_window, inputScrollCallback);

    g_renderThread = std::this_thread::get_id();
    g_glReady.store(true, std::memory_order_release);
    return true;
}

static void updateWindowSize()
{
    if (!g_window)
        return;
    int w = g_iScreenWidth;
    int h = g_iScreenHeight;
    glfwGetFramebufferSize(g_window, &w, &h);
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (w != g_iScreenWidth || h != g_iScreenHeight)
    {
        g_iScreenWidth = w;
        g_iScreenHeight = h;
        if (h > 0)
            g_iAspectRatio = (float)w / (float)h;
        mcglViewport(0, 0, w, h);
    }
}

// Helper: multiply two column-major 4x4 matrices: out = a * b
static void mat4Mul(float *out, const float *a, const float *b)
{
    for (int c = 0; c < 4; c++)
        for (int r = 0; r < 4; r++)
            out[c*4+r] = a[0*4+r]*b[c*4+0] + a[1*4+r]*b[c*4+1] + a[2*4+r]*b[c*4+2] + a[3*4+r]*b[c*4+3];
}

// Standard vertex layout for Vulkan (matches shader inputs at 32-byte stride)
struct VkStdVertex { float x, y, z, u, v; uint32_t color; uint32_t _pad0, _pad1; };

static int g_drawCallCount = 0;
static int g_cbuffCallCount = 0;
static int g_frameCount = 0;
static int g_drawsInFrustum = 0;
static int g_drawsOutFrustum = 0;
static int g_drawsWithFog = 0;
static int g_subUploadCount = 0; // texture sub-uploads this frame
static int g_fullUploadCount = 0; // full texture uploads this frame (blocking)

static int g_compressedDraws = 0, g_standardDraws = 0;
static int g_texturedDraws = 0, g_untexturedDraws = 0;

// Wall-clock diagnostic timer (logs every 2 seconds instead of every N frames)
static double g_diagLastTime = 0.0;
static int g_diagFrameAccum = 0;

// Per-frame timing (milliseconds)
static double g_timeFlush = 0, g_timeBeginFrame = 0, g_timeDraw = 0, g_timeEndFrame = 0;
static double g_startFrameEndTime = 0; // timestamp when StartFrame finished
static int g_pipelineCreates = 0; // pipeline cache misses this frame

static void executeDraw(const DrawCall &call)
{
    if (call.count <= 0 || call.bytes.empty())
        return;
    if (!g_vk.inRenderPass || !g_vk.device)
        return;
    if (call.vtype == C4JRender::VERTEX_TYPE_COMPRESSED) g_compressedDraws++;
    else g_standardDraws++;

    VkCommandBuffer cmd = vkb_cmd();
    if (!cmd || !g_vk.stagingMapped[g_vk.currentFrame])
        return;

    // ── Compute MVP ──
    ensureSoftMatrices();

    float mvp[16];
    float mv[16];
    if (call.hasMatrices)
    {
        // CBuffCall replay: vertices are in chunk-local coordinates.
        // The recorded modelview (call.modelview) contains the chunk-to-world
        // translation captured during display list compilation (translateToPos).
        // The current modelview has camera rotation + player translation.
        // Combined: MV = currentMV * recordedMV  →  transforms local→world→view.
        float combined[16];
        matMul(combined, tl_softMatrices.model, call.modelview);
        memcpy(mv, combined, sizeof(mv));
    }
    else
    {
        // Immediate draw: current thread's modelview is correct.
        memcpy(mv, tl_softMatrices.model, sizeof(mv));
    }
    mat4Mul(mvp, tl_softMatrices.proj, mv);

    // (per-draw matrix diagnostics removed — frame-level stats in frustum check below)

    // ── Push constants ──
    VkPushConstants pc = {};
    memcpy(pc.mvp, mvp, 64);
    pc.globalColor[0] = pc.globalColor[1] = pc.globalColor[2] = pc.globalColor[3] = 1.0f;
    // Resolve texture.  textureId semantics:
    //   -1 = unset (needs resolution from current GL state)
    //    0 = explicitly disabled (glDisable(GL_TEXTURE_2D))
    //   >0 = specific texture to use
    int effectiveTextureId = call.textureId;
    if (effectiveTextureId < 0)
    {
        if (g_currentTexture > 0) effectiveTextureId = g_currentTexture;
        else if (tl_currentTexture > 0) effectiveTextureId = tl_currentTexture;
        else effectiveTextureId = g_sharedTextureId.load(std::memory_order_relaxed);
    }
    // Only apply glColor state when the draw uses global color (no per-vertex colors).
    // Per-vertex color draws already have color baked into vertices — multiplying by
    // globalColor would double-tint them with whatever glColor4f was last set.
    if (!call.useVertexColor)
        memcpy(pc.globalColor, g_renderState.globalColor, sizeof(pc.globalColor));
    // Enable texturing whenever we have a valid texture — the g_textureEnabled flag
    // is unreliable across display list replay boundaries.
    pc.textureEnable   = (effectiveTextureId > 0) ? 1.0f : 0.0f;
    pc.fogEnable       = g_renderState.fogEnable ? 1.0f : 0.0f;
    if (g_renderState.fogEnable) g_drawsWithFog++;
    pc.fogColor[0] = g_renderState.fogColor[0];
    pc.fogColor[1] = g_renderState.fogColor[1];
    pc.fogColor[2] = g_renderState.fogColor[2];
    pc.fogColor[3]     = 1.0f;
    pc.fogParams[0]    = g_renderState.fogNear;
    pc.fogParams[1]    = g_renderState.fogFar;
    pc.fogParams[2]    = g_renderState.fogDensity;
    pc.fogParams[3]    = (float)g_renderState.fogMode;
    pc.alphaTestEnable = g_renderState.alphaTestEnable ? 1.0f : 0.0f;
    pc.alphaRef        = g_renderState.alphaRef;

    // ── Expand vertices ──
    const bool isQuadList = (call.primitive == C4JRender::PRIMITIVE_TYPE_QUAD_LIST);
    const bool isCompressed = (call.vtype == C4JRender::VERTEX_TYPE_COMPRESSED);

    // Calculate output vertex count
    int outCount = call.count;
    if (isQuadList)
        outCount = (call.count / 4) * 6; // 4 verts per quad → 6 verts (2 triangles)

    size_t outBytes = (size_t)outCount * 32;

    // Check staging buffer space
    VkDeviceSize alignedOffset = (g_vk.stagingOffset + 3) & ~(VkDeviceSize)3;
    if (alignedOffset + outBytes > VKB_STAGING_BUFFER_SIZE)
        return; // staging full, skip this draw

    uint8_t *dst = (uint8_t*)g_vk.stagingMapped[g_vk.currentFrame] + alignedOffset;

    // Write vertices to staging buffer
    auto writeStdVert = [](uint8_t *d, float x, float y, float z, float u, float v, uint32_t col) {
        float *f = (float*)d;
        f[0] = x; f[1] = y; f[2] = z; f[3] = u; f[4] = v;
        ((uint32_t*)d)[5] = col;
        ((uint32_t*)d)[6] = 0;
        ((uint32_t*)d)[7] = 0;
    };

    if (isCompressed)
    {
        const int16_t *v = reinterpret_cast<const int16_t *>(call.bytes.data());
        auto decodeVert = [&](int i, float &ox, float &oy, float &oz, float &ou, float &ov, uint32_t &oc) {
            const int16_t *s = v + (i * 8);
            ox = (float)s[0] / 1024.0f;
            oy = (float)s[1] / 1024.0f;
            oz = (float)s[2] / 1024.0f;
            ou = (float)s[4] / 8192.0f;
            ov = (float)s[5] / 8192.0f;
            unsigned short packed = (unsigned short)(((int)s[3] + 32768) & 0xFFFF);
            unsigned char r, g, b;
            decode565(packed, r, g, b);

            // Lightmap brightness from s[6]=blockLight*16, s[7]=skyLight*16
            int blockLight = (s[6] >> 4) & 0xF;
            int skyLight   = (s[7] >> 4) & 0xF;
            float skyBr   = s_brightnessRamp[skyLight];
            float blockBr = s_brightnessRamp[blockLight] * 1.5f;
            float bright  = skyBr + blockBr;
            if (bright > 1.0f) bright = 1.0f;
            bright = bright * 0.96f + 0.03f; // match lightmap post-processing

            r = (unsigned char)(r * bright);
            g = (unsigned char)(g * bright);
            b = (unsigned char)(b * bright);

            oc = ((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | 0xFF;
        };

        uint8_t *p = dst;
        if (isQuadList)
        {
            for (int i = 0; i + 3 < call.count; i += 4)
            {
                float x[4], y[4], z[4], u[4], vv[4]; uint32_t c[4];
                for (int j = 0; j < 4; j++) decodeVert(i+j, x[j], y[j], z[j], u[j], vv[j], c[j]);
                int idx[] = {0,1,2, 0,2,3};
                for (int j = 0; j < 6; j++) {
                    int k = idx[j];
                    writeStdVert(p, x[k], y[k], z[k], u[k], vv[k], c[k]);
                    p += 32;
                }
            }
        }
        else
        {
            for (int i = 0; i < call.count; i++) {
                float x, y, z, u, vv; uint32_t c;
                decodeVert(i, x, y, z, u, vv, c);
                writeStdVert(p, x, y, z, u, vv, c);
                p += 32;
            }
        }
    }
    else
    {
        // Standard format: already 32 bytes per vertex, just handle quad expansion
        const uint8_t *src = call.bytes.data();
        if (isQuadList)
        {
            uint8_t *p = dst;
            for (int i = 0; i + 3 < call.count; i += 4)
            {
                int idx[] = {0,1,2, 0,2,3};
                for (int j = 0; j < 6; j++) {
                    memcpy(p, src + (i + idx[j]) * 32, 32);
                    p += 32;
                }
            }
        }
        else
        {
            memcpy(dst, src, (size_t)call.count * 32);
        }
    }

    // If no per-vertex color, bake white (0xFFFFFFFF) into color slot
    if (!call.useVertexColor)
    {
        uint32_t white = 0xFFFFFFFF;
        for (int i = 0; i < outCount; i++)
            memcpy(dst + i * 32 + 20, &white, 4);
    }

    // Apply lightmap brightness from _tex2 field (byte offset 28) for standard format.
    // _tex2 = skyLight << 20 | blockLight << 4.  Value 0xfe00fe00 means "no lightmap".
    if (!isCompressed && call.useVertexColor)
    {
        for (int i = 0; i < outCount; i++)
        {
            uint32_t tex2;
            memcpy(&tex2, dst + i * 32 + 28, 4);
            if (tex2 == 0xfe00fe00u) continue; // no lightmap data

            int skyLight   = (tex2 >> 20) & 0xF;
            int blockLight = (tex2 >> 4) & 0xF;
            float skyBr   = s_brightnessRamp[skyLight];
            float blockBr = s_brightnessRamp[blockLight] * 1.5f;
            float bright  = skyBr + blockBr;
            if (bright > 1.0f) bright = 1.0f;
            bright = bright * 0.96f + 0.03f;

            uint32_t col;
            memcpy(&col, dst + i * 32 + 20, 4);
            unsigned char r = (col >> 24) & 0xFF;
            unsigned char g = (col >> 16) & 0xFF;
            unsigned char b = (col >> 8)  & 0xFF;
            unsigned char a = col & 0xFF;
            r = (unsigned char)(r * bright);
            g = (unsigned char)(g * bright);
            b = (unsigned char)(b * bright);
            col = ((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | a;
            memcpy(dst + i * 32 + 20, &col, 4);
        }
    }

    // ── Bind pipeline ──
    VkPipelineKey key = vkp_default_key();
    key.blendEnable      = g_renderState.blendEnable ? 1 : 0;
    key.srcBlend         = g_renderState.blendSrc & 0xF;
    key.dstBlend         = g_renderState.blendDst & 0xF;
    key.colorWriteMask   = g_renderState.colorWriteMask & 0xF;
    key.depthTestEnable  = g_renderState.depthTestEnable ? 1 : 0;
    key.depthWriteEnable = g_renderState.depthWriteEnable ? 1 : 0;
    if (call.primitive == C4JRender::PRIMITIVE_TYPE_LINE_LIST)
        key.topology = 1;
    else if (call.primitive == C4JRender::PRIMITIVE_TYPE_LINE_STRIP)
        key.topology = 2;
    else if (call.primitive == C4JRender::PRIMITIVE_TYPE_TRIANGLE_STRIP)
        key.topology = 3;
    else if (call.primitive == C4JRender::PRIMITIVE_TYPE_TRIANGLE_FAN)
        key.topology = 4;

    VkPipeline pipeline = vkp_get_pipeline(key);
    if (!pipeline) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdPushConstants(cmd, g_pipe.pipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0, sizeof(VkPushConstants), &pc);

    // Bind texture descriptor
    VkDescriptorSet texDesc = (effectiveTextureId > 0)
        ? vkt_get_descriptor(effectiveTextureId)
        : g_pipe.whiteTexDescSet;
    if (effectiveTextureId > 0) g_texturedDraws++;
    else g_untexturedDraws++;
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        g_pipe.pipelineLayout, 0, 1, &texDesc, 0, nullptr);

    // ── Bind vertex buffer and draw ──
    VkBuffer vb = g_vk.stagingBuffer[g_vk.currentFrame];
    VkDeviceSize offset = alignedOffset;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &offset);
    vkCmdDraw(cmd, (uint32_t)outCount, 1, 0, 0);

    g_vk.stagingOffset = alignedOffset + outBytes;
    g_drawCallCount++;

    // Frustum check: test first vertex of each draw
    {
        float *v0 = (float*)dst;
        float tw = mvp[3]*v0[0] + mvp[7]*v0[1] + mvp[11]*v0[2] + mvp[15];
        if (tw > 0.001f) {
            float tx = mvp[0]*v0[0] + mvp[4]*v0[1] + mvp[8]*v0[2]  + mvp[12];
            float ty = mvp[1]*v0[0] + mvp[5]*v0[1] + mvp[9]*v0[2]  + mvp[13];
            float tz = mvp[2]*v0[0] + mvp[6]*v0[1] + mvp[10]*v0[2] + mvp[14];
            float nx = tx/tw, ny = ty/tw, nz = tz/tw;
            if (nx >= -1 && nx <= 1 && ny >= -1 && ny <= 1 && nz >= 0 && nz <= 1)
                g_drawsInFrustum++;
            else
                g_drawsOutFrustum++;
        } else {
            g_drawsOutFrustum++;
        }
    }

}

} // namespace

// --- Core -----------------------------------------------------------

void C4JRender::Initialise(void * /*pDevice*/, void * /*pSwapChain*/) { ensureGLReady(); }
void C4JRender::InitialiseContext() { ensureGLReady(); }
void C4JRender::Tick()
{
    if (!ensureGLReady())
        return;
    glfwPollEvents();
    if (glfwWindowShouldClose(g_window))
    {
        app.m_bShutdown = true;
        return;
    }
    updateWindowSize();
}
void C4JRender::UpdateGamma(unsigned short /*usGamma*/) {}
void C4JRender::StartFrame()
{
    if (!ensureGLReady()) return;
    // Adopt whichever thread actually calls StartFrame as the render thread.
    // (The GLFW/Vulkan init may happen on a different thread than rendering.)
    g_renderThread = std::this_thread::get_id();
    g_drawCallCount = 0;
    g_cbuffCallCount = 0;
    g_subUploadCount = 0;
    g_fullUploadCount = 0;
    g_timeDraw = 0;

    // Flush any pending texture sub-uploads (from animated textures last frame)
    // BEFORE beginning the render pass, so the GPU does all transfers in one batch.
    double t0 = glfwGetTime();
    vkt_flush_uploads();
    g_timeFlush = (glfwGetTime() - t0) * 1000.0;

    double t1 = glfwGetTime();
    if (!vkb_begin_frame())
    {
        // Swapchain out of date or minimized — skip this frame
        g_vk.inRenderPass = false;
    }
    g_timeBeginFrame = (glfwGetTime() - t1) * 1000.0;
    g_startFrameEndTime = glfwGetTime();
}
void C4JRender::Present()
{
    if (!ensureGLReady())
        return;
    g_timeDraw = (glfwGetTime() - g_startFrameEndTime) * 1000.0;
    g_frameCount++;
    g_diagFrameAccum++;

    // Log diagnostics every 2 seconds by wall clock (not frame count).
    // This ensures we see output even at very low FPS.
    double now = glfwGetTime();
    if (g_diagLastTime == 0.0) g_diagLastTime = now;
    if (now - g_diagLastTime >= 2.0)
    {
        double elapsed = now - g_diagLastTime;
        int fps = (elapsed > 0.0) ? (int)(g_diagFrameAccum / elapsed + 0.5) : 0;
        SessionLog_Printf("[vk-diag] frame %d: %d draws (%d in/%d out) cbuf=%d tex=%d/%d texUp=%d fullUp=%d FPS=%d  flush=%.1fms begin=%.1fms draw=%.1fms end=%.1fms\n",
            g_frameCount, g_drawCallCount, g_drawsInFrustum, g_drawsOutFrustum,
            g_cbuffCallCount,
            g_texturedDraws, g_untexturedDraws,
            g_subUploadCount, g_fullUploadCount, fps,
            g_timeFlush, g_timeBeginFrame, g_timeDraw, g_timeEndFrame);
        g_diagLastTime = now;
        g_diagFrameAccum = 0;
    }

    g_drawsInFrustum = 0;
    g_drawsOutFrustum = 0;
    g_drawsWithFog = 0;
    g_compressedDraws = 0;
    g_standardDraws = 0;
    g_texturedDraws = 0;
    g_untexturedDraws = 0;
    if (g_vk.inRenderPass) {
        double t2 = glfwGetTime();
        vkb_end_frame();
        g_timeEndFrame = (glfwGetTime() - t2) * 1000.0;
    }
}
void C4JRender::Clear(int flags, D3D11_RECT *pRect)
{
    if (!ensureGLReady())
        return;

    unsigned int mask = 0;
    if (flags & GL_COLOR_BUFFER_BIT) mask |= 0x00004000; // GL_COLOR_BUFFER_BIT
    if (flags & GL_DEPTH_BUFFER_BIT) mask |= 0x00000100; // GL_DEPTH_BUFFER_BIT
    if (mask == 0) return;

    if (pRect)
    {
        mcglEnable(0x0C11); // GL_SCISSOR_TEST
        mcglScissor((int)pRect->left, g_iScreenHeight - (int)pRect->bottom,
                  (int)(pRect->right - pRect->left), (int)(pRect->bottom - pRect->top));
        mcglClear(mask);
        mcglDisable(0x0C11); // GL_SCISSOR_TEST
        return;
    }
    mcglClear(mask);
}
void C4JRender::SetClearColour(const float colourRGBA[4])
{
    if (!colourRGBA) return;
    g_clearRGBA[0] = colourRGBA[0];
    g_clearRGBA[1] = colourRGBA[1];
    g_clearRGBA[2] = colourRGBA[2];
    g_clearRGBA[3] = colourRGBA[3];
    g_vk.clearColor[0] = colourRGBA[0];
    g_vk.clearColor[1] = colourRGBA[1];
    g_vk.clearColor[2] = colourRGBA[2];
    g_vk.clearColor[3] = colourRGBA[3];
}
void C4JRender::DoScreenGrabOnNextPresent() {}
bool C4JRender::IsWidescreen() { return g_iAspectRatio >= 1.5f; }
bool C4JRender::IsHiDef()      { return g_iScreenWidth >= 1280 && g_iScreenHeight >= 720; }

void C4JRender::CaptureThumbnail(ImageFileBuffer * /*pngOut*/) {}
void C4JRender::CaptureScreen(ImageFileBuffer * /*jpgOut*/, XSOCIAL_PREVIEWIMAGE * /*previewOut*/) {}

void C4JRender::BeginConditionalSurvey(int /*identifier*/) {}
void C4JRender::EndConditionalSurvey() {}
void C4JRender::BeginConditionalRendering(int /*identifier*/) {}
void C4JRender::EndConditionalRendering() {}

// --- Matrix stack ---------------------------------------------------

void C4JRender::MatrixMode(int type)
{
    ensureSoftMatrices();
    tl_softMatrices.mode = type;
    if (ensureGLReady() && std::this_thread::get_id() == g_renderThread) mcglMatrixMode(mapMatrixMode(type));
}
void C4JRender::MatrixSetIdentity()
{
    matIdentity(currentSoftMatrix());
    if (ensureGLReady() && std::this_thread::get_id() == g_renderThread) { mcglMatrixMode(mapMatrixMode(tl_softMatrices.mode)); mcglLoadIdentity(); }
}
void C4JRender::MatrixTranslate(float x, float y, float z)
{
    softTranslate(x, y, z);
    if (ensureGLReady() && std::this_thread::get_id() == g_renderThread) { mcglMatrixMode(mapMatrixMode(tl_softMatrices.mode)); mcglTranslatef(x, y, z); }
}
void C4JRender::MatrixRotate(float angle, float x, float y, float z)
{
    softRotateRadians(angle, x, y, z);
    if (ensureGLReady() && std::this_thread::get_id() == g_renderThread) { mcglMatrixMode(mapMatrixMode(tl_softMatrices.mode)); mcglRotatef(angle * (180.0f / 3.1415926535f), x, y, z); }
}
void C4JRender::MatrixScale(float x, float y, float z)
{
    softScale(x, y, z);
    if (ensureGLReady() && std::this_thread::get_id() == g_renderThread) { mcglMatrixMode(mapMatrixMode(tl_softMatrices.mode)); mcglScalef(x, y, z); }
}
void C4JRender::MatrixPerspective(float fovy, float aspect, float zNear, float zFar)
{
    const float fovyRadians = fovy * (3.1415926535f / 180.0f);
    const float top = zNear * std::tan(fovyRadians * 0.5f);
    const float bottom = -top;
    const float right = top * aspect;
    const float left = -right;
    softFrustum(left, right, bottom, top, zNear, zFar);

    if (!ensureGLReady()) return;
    mcglMatrixMode(mapMatrixMode(tl_softMatrices.mode));
    mcglFrustum((double)left, (double)right, (double)bottom, (double)top, (double)zNear, (double)zFar);
}
void C4JRender::MatrixOrthogonal(float left, float right, float bottom, float top, float zNear, float zFar)
{
    softOrtho(left, right, bottom, top, zNear, zFar);
    if (ensureGLReady() && std::this_thread::get_id() == g_renderThread) { mcglMatrixMode(mapMatrixMode(tl_softMatrices.mode)); mcglOrtho(left, right, bottom, top, zNear, zFar); }
}
void C4JRender::MatrixPop()
{
    std::vector<std::array<float, 16>> &stack = currentSoftStack();
    if (!stack.empty())
    {
        matCopy(currentSoftMatrix(), stack.back().data());
        stack.pop_back();
    }
    if (ensureGLReady() && std::this_thread::get_id() == g_renderThread) { mcglMatrixMode(mapMatrixMode(tl_softMatrices.mode)); mcglPopMatrix(); }
}
void C4JRender::MatrixPush()
{
    std::vector<std::array<float, 16>> &stack = currentSoftStack();
    std::array<float, 16> m = {};
    memcpy(m.data(), currentSoftMatrix(), sizeof(float) * 16);
    stack.push_back(m);
    if (ensureGLReady() && std::this_thread::get_id() == g_renderThread) { mcglMatrixMode(mapMatrixMode(tl_softMatrices.mode)); mcglPushMatrix(); }
}
void C4JRender::MatrixMult(float *mat)
{
    if (mat) matPostMul(currentSoftMatrix(), mat);
    if (mat && ensureGLReady() && std::this_thread::get_id() == g_renderThread) { mcglMatrixMode(mapMatrixMode(tl_softMatrices.mode)); mcglMultMatrixf(mat); }
}

const float *C4JRender::MatrixGet(int type)
{
    // Return our software matrices directly — mcglGetFloatv is a no-op in Vulkan.
    ensureSoftMatrices();
    if (type == GL_PROJECTION || type == GL_PROJECTION_MATRIX)
        return tl_softMatrices.proj;
    if (type == GL_TEXTURE)
        return tl_softMatrices.tex;
    return tl_softMatrices.model;
}

void C4JRender::Set_matrixDirty() {}

// --- Vertex data ----------------------------------------------------

void C4JRender::DrawVertices(ePrimitiveType PrimitiveType, int count, void *dataIn,
                             eVertexType vType, C4JRender::ePixelShaderType /*psType*/)
{
    if (!dataIn || count <= 0)
        return;

    DrawCall call = {};
    call.primitive = PrimitiveType;
    call.count = count;
    call.vtype = vType;
    const size_t bytes = strideForType(vType) * (size_t)count;
    call.bytes.resize(bytes);
    memcpy(call.bytes.data(), dataIn, bytes);
    ensureSoftMatrices();
    call.textureId = tl_currentTexture;
    call.useVertexColor = tl_nextDrawHasVertexColor;
    tl_nextDrawHasVertexColor = true; // reset for next draw
    call.globalUV[0] = g_sharedUVU.load(std::memory_order_relaxed);
    call.globalUV[1] = g_sharedUVV.load(std::memory_order_relaxed);

    if (tl_recordingCbuff >= 0)
    {
        // Recording into a command buffer — capture the matrix for later replay.
        call.hasMatrices = true;
        memcpy(call.modelview, tl_softMatrices.model, sizeof(call.modelview));
        memcpy(call.projection, tl_softMatrices.proj, sizeof(call.projection));
        tl_recordingCalls.push_back(std::move(call));
        return;
    }

    // Immediate draw
    call.hasMatrices = false;
    // If GL_TEXTURE_2D is disabled, force texture off (0 = explicitly disabled).
    if (!g_textureEnabled)
        call.textureId = 0;

    if (!ensureGLReady())
        return;
    executeDraw(call);
}

void C4JRender::DrawVertexBuffer(ePrimitiveType PrimitiveType, int count, void *buffer,
                                 C4JRender::eVertexType vType, C4JRender::ePixelShaderType psType)
{
    DrawVertices(PrimitiveType, count, buffer, vType, psType);
}

// --- Command buffers ------------------------------------------------

void C4JRender::CBuffLockStaticCreations() {}
int  C4JRender::CBuffCreate(int count)
{
    if (count <= 0) return 0;
    std::lock_guard<std::mutex> lock(g_cbuffMutex);
    int first = g_nextCBuffId;
    for (int i = 0; i < count; ++i) g_cbuffs[g_nextCBuffId++] = {};
    return first;
}
void C4JRender::CBuffDelete(int first, int count)
{
    if (count <= 0) return;
    std::lock_guard<std::mutex> lock(g_cbuffMutex);
    for (int i = 0; i < count; ++i) g_cbuffs.erase(first + i);
}
void C4JRender::CBuffStart(int index, bool /*full*/)
{
    tl_recordingCbuff = index;
    tl_recordingCalls.clear();
    // Save the current modelview and reset to identity.  In real OpenGL,
    // glNewList(GL_COMPILE) defers operations — they don't touch the current
    // matrix.  We emulate this by starting recording from identity so that
    // the captured MV reflects ONLY transforms within the display list
    // (e.g. chunk-to-world translation), not ambient thread state.
    ensureSoftMatrices();
    memcpy(tl_savedModelForRecording, tl_softMatrices.model, sizeof(tl_savedModelForRecording));
    matIdentity(tl_softMatrices.model);
    // Don't inherit texture from render thread — display list draws should
    // use whatever texture is current at replay time (set by the caller before
    // CBuffCall), matching OpenGL display list semantics where texture binds
    // are NOT recorded.  The old code captured g_currentTexture here, which
    // raced with the render thread and could lock in the wrong texture
    // (e.g. GUI icons) for all subsequent chunk compilations.
}
void C4JRender::CBuffClear(int index) { std::lock_guard<std::mutex> lock(g_cbuffMutex); g_cbuffs[index].clear(); }
int  C4JRender::CBuffSize(int index) { std::lock_guard<std::mutex> lock(g_cbuffMutex); return (int)g_cbuffs[index].size(); }
void C4JRender::CBuffEnd()
{
    if (tl_recordingCbuff < 0) return;
    std::lock_guard<std::mutex> lock(g_cbuffMutex);
    g_cbuffs[tl_recordingCbuff] = std::move(tl_recordingCalls);
    tl_recordingCalls.clear();
    tl_recordingCbuff = -1;
    // Restore modelview saved at CBuffStart (undo the identity reset)
    memcpy(tl_softMatrices.model, tl_savedModelForRecording, sizeof(tl_softMatrices.model));
}
bool C4JRender::CBuffCall(int index, bool /*full*/)
{
    if (!ensureGLReady())
        return false;
    std::vector<DrawCall> local;
    {
        std::lock_guard<std::mutex> lock(g_cbuffMutex);
        auto it = g_cbuffs.find(index);
        if (it == g_cbuffs.end()) return false;
        local = it->second;
    }
    g_cbuffCallCount++;
    int replayTextureId = g_currentTexture;
    if (replayTextureId <= 0)
    {
        if (tl_currentTexture > 0)
            replayTextureId = tl_currentTexture;
        else
            replayTextureId = g_sharedTextureId.load(std::memory_order_relaxed);
    }
    for (const DrawCall &src : local)
    {
        DrawCall c = src;
        if (g_textureEnabled)
        {
            // Textured replay: resolve texture from recorded or current state.
            if (c.textureId > 0)
                replayTextureId = c.textureId;
            else if (replayTextureId > 0)
                c.textureId = replayTextureId;
        }
        else
        {
            // Untextured replay (sky, lines): force texture off.
            // Use 0 (not -1) so executeDraw doesn't fall through to the
            // texture resolution chain that would re-assign a texture.
            c.textureId = 0;
        }
        executeDraw(c);
    }
    return true;
}
void C4JRender::CBuffTick() {}
void C4JRender::CBuffDeferredModeStart() {}
void C4JRender::CBuffDeferredModeEnd() {}

// --- Textures -------------------------------------------------------

int C4JRender::TextureCreate()
{
    if (!ensureGLReady()) return 0;
    int id = vkt_create();
    TextureBind(id);
    return id;
}

void C4JRender::TextureFree(int idx)
{
    if (!ensureGLReady()) return;
    vkt_free(idx);
}
void C4JRender::TextureBind(int idx)
{
    tl_currentTexture = (idx > 0) ? idx : -1;
    g_sharedTextureId.store(tl_currentTexture, std::memory_order_relaxed);
    if (!ensureGLReady()) return;
    if (idx <= 0) { g_currentTexture = -1; return; }
    g_currentTexture = idx;
    g_textureEnabled = true;
}
void C4JRender::TextureBindVertex(int idx) { TextureBind(idx); }
void C4JRender::TextureSetTextureLevels(int levels) { g_textureLevels = (levels > 0) ? levels : 1; }
int  C4JRender::TextureGetTextureLevels() { return g_textureLevels; }
void C4JRender::TextureData(int width, int height, void *data, int level, eTextureFormat /*format*/)
{
    if (!ensureGLReady()) return;
    if (g_currentTexture <= 0 || !data || width <= 0 || height <= 0) return;
    vkt_upload(g_currentTexture, width, height, data, level);
    g_fullUploadCount++;
}
void C4JRender::TextureDataUpdate(int xoffset, int yoffset, int width, int height, void *data, int level)
{
    if (!ensureGLReady()) return;
    if (g_currentTexture <= 0 || !data || width <= 0 || height <= 0) return;
    vkt_sub_upload(g_currentTexture, xoffset, yoffset, width, height, data, level);
    g_subUploadCount++;
}
void C4JRender::TextureSetParam(int param, int value)
{
    if (!ensureGLReady()) return;
    if (g_currentTexture <= 0) return;
    if (param == GL_TEXTURE_MIN_FILTER || param == GL_TEXTURE_MAG_FILTER)
    {
        vkt_set_filter(g_currentTexture, value == GL_LINEAR);
    }
}
void C4JRender::TextureDynamicUpdateStart() {}
void C4JRender::TextureDynamicUpdateEnd() {}

// ---- internal helper: decode raw libpng rows → new int[w*h] in ARGB format ----
static int *pngRowsToARGB(png_structp png, png_infop info, int width, int height)
{
    size_t row_bytes = png_get_rowbytes(png, info);
    png_bytep *rows = (png_bytep *)malloc(height * sizeof(png_bytep));
    if (!rows) return nullptr;
    for (int y = 0; y < height; y++) {
        rows[y] = (png_byte *)malloc(row_bytes);
        if (!rows[y]) {
            for (int j = 0; j < y; j++) free(rows[j]);
            free(rows);
            return nullptr;
        }
    }
    png_read_image(png, rows);

    int *pixels = new int[width * height];
    for (int y = 0; y < height; y++) {
        png_bytep row = rows[y];
        for (int x = 0; x < width; x++) {
            unsigned char r = row[x*4 + 0];
            unsigned char g = row[x*4 + 1];
            unsigned char b = row[x*4 + 2];
            unsigned char a = row[x*4 + 3];
            pixels[y * width + x] = ((int)a << 24) | ((int)r << 16) | ((int)g << 8) | (int)b;
        }
        free(rows[y]);
    }
    free(rows);
    return pixels;
}

// ---- shared PNG setup: normalise any PNG variant to RGBA 8-bit ----
static void pngSetupTransforms(png_structp png, png_infop info)
{
    png_byte color_type = png_get_color_type(png, info);
    png_byte bit_depth  = png_get_bit_depth(png, info);

    if (bit_depth == 16)
        png_set_strip_16(png);
    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);
    if (color_type == PNG_COLOR_TYPE_RGB ||
        color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);

    png_read_update_info(png, info);
}

HRESULT C4JRender::LoadTextureData(const char *szFilename, D3DXIMAGE_INFO *pSrcInfo, int **ppDataOut)
{
    if (pSrcInfo)  memset(pSrcInfo, 0, sizeof(D3DXIMAGE_INFO));
    if (!ppDataOut) return E_INVALIDARG;
    *ppDataOut = nullptr;
    if (!szFilename) return E_INVALIDARG;

    FILE *fp = fopen(szFilename, "rb");
    if (!fp) return E_FAIL;

    unsigned char sig[8];
    if (fread(sig, 1, 8, fp) != 8 || png_sig_cmp(sig, 0, 8)) {
        fclose(fp);
        return E_FAIL;
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png) { fclose(fp); return E_FAIL; }
    png_infop info = png_create_info_struct(png);
    if (!info) { png_destroy_read_struct(&png, nullptr, nullptr); fclose(fp); return E_FAIL; }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, nullptr);
        fclose(fp);
        return E_FAIL;
    }

    png_init_io(png, fp);
    png_set_sig_bytes(png, 8);
    png_read_info(png, info);

    int width  = (int)png_get_image_width(png, info);
    int height = (int)png_get_image_height(png, info);
    pngSetupTransforms(png, info);

    int *pixels = pngRowsToARGB(png, info, width, height);
    png_destroy_read_struct(&png, &info, nullptr);
    fclose(fp);

    if (!pixels) return E_OUTOFMEMORY;

    if (pSrcInfo) { pSrcInfo->Width = width; pSrcInfo->Height = height; }
    *ppDataOut = pixels;
    return S_OK;
}

// Memory-buffer read callback for libpng
struct PngMemState { const BYTE *data; DWORD size; DWORD pos; };
static void pngMemRead(png_structp png, png_bytep dst, png_size_t len)
{
    PngMemState *s = (PngMemState *)png_get_io_ptr(png);
    if (s->pos + (DWORD)len > s->size) png_error(png, "read past end");
    memcpy(dst, s->data + s->pos, len);
    s->pos += (DWORD)len;
}

HRESULT C4JRender::LoadTextureData(BYTE *pbData, DWORD dwBytes, D3DXIMAGE_INFO *pSrcInfo, int **ppDataOut)
{
    if (pSrcInfo)  memset(pSrcInfo, 0, sizeof(D3DXIMAGE_INFO));
    if (!ppDataOut) return E_INVALIDARG;
    *ppDataOut = nullptr;
    if (!pbData || dwBytes < 8) return E_INVALIDARG;

    if (png_sig_cmp(pbData, 0, 8)) return E_FAIL;

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png) return E_FAIL;
    png_infop info = png_create_info_struct(png);
    if (!info) { png_destroy_read_struct(&png, nullptr, nullptr); return E_FAIL; }

    PngMemState state = { pbData, dwBytes, 8 };
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, nullptr);
        return E_FAIL;
    }

    png_set_read_fn(png, &state, pngMemRead);
    png_set_sig_bytes(png, 8);
    png_read_info(png, info);

    int width  = (int)png_get_image_width(png, info);
    int height = (int)png_get_image_height(png, info);
    pngSetupTransforms(png, info);

    int *pixels = pngRowsToARGB(png, info, width, height);
    png_destroy_read_struct(&png, &info, nullptr);

    if (!pixels) return E_OUTOFMEMORY;

    if (pSrcInfo) { pSrcInfo->Width = width; pSrcInfo->Height = height; }
    *ppDataOut = pixels;
    return S_OK;
}

HRESULT C4JRender::SaveTextureData(const char * /*szFilename*/, D3DXIMAGE_INFO * /*pSrcInfo*/, int * /*ppDataOut*/)
{
    return S_OK;
}

HRESULT C4JRender::SaveTextureDataToMemory(void * /*pOutput*/, int /*outputCapacity*/, int *outputLength, int /*width*/, int /*height*/, int * /*ppDataIn*/)
{
    if (outputLength) *outputLength = 0;
    return S_OK;
}

void  C4JRender::TextureGetStats() {}
void *C4JRender::TextureGetTexture(int idx)
{
    auto it = g_textures.find(idx);
    if (it == g_textures.end())
        return nullptr;
    return (void *)(uintptr_t)it->second.glId;
}

// --- State control --------------------------------------------------

void C4JRender::StateSetColour(float r, float g, float b, float a)
{
    g_renderState.globalColor[0] = r; g_renderState.globalColor[1] = g;
    g_renderState.globalColor[2] = b; g_renderState.globalColor[3] = a;
}
void C4JRender::StateSetDepthMask(bool enable)
{
    g_renderState.depthWriteEnable = enable;
}
void C4JRender::StateSetBlendEnable(bool enable)
{
    g_renderState.blendEnable = enable;
}
void C4JRender::StateSetBlendFunc(int src, int dst)
{
    // Map engine blend constants to VkBlendFactor values
    auto mapToVk = [](int f) -> int {
        switch (f) {
        case GL_ZERO:                    return 0;   // VK_BLEND_FACTOR_ZERO
        case GL_ONE:                     return 1;   // VK_BLEND_FACTOR_ONE
        case GL_SRC_COLOR:               return 2;   // VK_BLEND_FACTOR_SRC_COLOR
        case GL_ONE_MINUS_SRC_COLOR:     return 3;   // VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR
        case GL_DST_COLOR:               return 4;   // VK_BLEND_FACTOR_DST_COLOR
        case GL_ONE_MINUS_DST_COLOR:     return 5;   // VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR
        case GL_SRC_ALPHA:               return 6;   // VK_BLEND_FACTOR_SRC_ALPHA
        case GL_ONE_MINUS_SRC_ALPHA:     return 7;   // VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA
        case GL_DST_ALPHA:               return 8;   // VK_BLEND_FACTOR_DST_ALPHA
        case GL_CONSTANT_ALPHA:          return 12;  // VK_BLEND_FACTOR_CONSTANT_ALPHA
        case GL_ONE_MINUS_CONSTANT_ALPHA: return 13; // VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA
        default: return 1;
        }
    };
    g_renderState.blendSrc = mapToVk(src);
    g_renderState.blendDst = mapToVk(dst);
}
void C4JRender::StateSetBlendFactor(unsigned int colour)
{
    // Blend constants stored for vkCmdSetBlendConstants (not used in most draws)
}
void C4JRender::StateSetAlphaFunc(int func, float param)
{
    g_renderState.alphaRef = param;
}
void C4JRender::StateSetDepthFunc(int func)
{
    // Depth compare op is baked into pipeline as LEQUAL for all variants
}
void C4JRender::StateSetFaceCull(bool enable)
{
    // Culling disabled in pipeline (VK_CULL_MODE_NONE) — matches original behavior
}
void C4JRender::StateSetFaceCullCW(bool enable) {}
void C4JRender::StateSetLineWidth(float width) {}
void C4JRender::StateSetWriteEnable(bool red, bool green, bool blue, bool alpha)
{
    g_renderState.colorWriteMask = (red ? 1 : 0) | (green ? 2 : 0) | (blue ? 4 : 0) | (alpha ? 8 : 0);
}
void C4JRender::StateSetDepthTestEnable(bool enable)
{
    g_renderState.depthTestEnable = enable;
}
void C4JRender::StateSetAlphaTestEnable(bool enable)
{
    g_renderState.alphaTestEnable = enable;
}
void C4JRender::StateSetDepthSlopeAndBias(float slope, float bias)
{
    g_renderState.depthBiasSlope = slope;
    g_renderState.depthBiasConst = bias;
}
void C4JRender::SetNextDrawVertexColor(bool hasColor) { tl_nextDrawHasVertexColor = hasColor; }
void C4JRender::StateSetTextureEnable(bool enable) { g_textureEnabled = enable; }
void C4JRender::StateSetFogEnable(bool enable) {
    g_renderState.fogEnable = enable;
}
void C4JRender::StateSetFogMode(int mode) { g_renderState.fogMode = (mode == GL_EXP) ? 1 : 0; }
void C4JRender::StateSetFogNearDistance(float dist) { g_renderState.fogNear = dist; }
void C4JRender::StateSetFogFarDistance(float dist) { g_renderState.fogFar = dist; }
void C4JRender::StateSetFogDensity(float density) { g_renderState.fogDensity = density; }
void C4JRender::StateSetFogColour(float r, float g, float b) {
    g_renderState.fogColor[0]=r; g_renderState.fogColor[1]=g; g_renderState.fogColor[2]=b;
}
void C4JRender::StateSetLightingEnable(bool enable) {}
void C4JRender::StateSetVertexTextureUV(float u, float v)
{
    tl_globalUV[0] = u;
    tl_globalUV[1] = v;
    g_sharedUVU.store(u, std::memory_order_relaxed);
    g_sharedUVV.store(v, std::memory_order_relaxed);
}
void C4JRender::StateSetLightColour(int light, float red, float green, float blue) { if (!ensureGLReady()) return; float d[4] = {red, green, blue, 1.0f}; mcglLightfv(light == 1 ? 0x4001 : 0x4000, 0x1201, d); }
void C4JRender::StateSetLightAmbientColour(float red, float green, float blue) { if (!ensureGLReady()) return; float a[4] = {red, green, blue, 1.0f}; mcglLightModelfv(0x0B53, a); }
void C4JRender::StateSetLightDirection(int light, float x, float y, float z) { if (!ensureGLReady()) return; float p[4] = {x, y, z, 0.0f}; mcglLightfv(light == 1 ? 0x4001 : 0x4000, 0x1203, p); }
void C4JRender::StateSetLightEnable(int light, bool enable)
{
    if (!ensureGLReady()) return;
    unsigned int l = (light == 1) ? 0x4001 : 0x4000;
    if (enable) mcglEnable(l); else mcglDisable(l);
}
void C4JRender::StateSetViewport(eViewportType viewportType)
{
    if (!ensureGLReady()) return;
    int x = 0, y = 0, w = g_iScreenWidth, h = g_iScreenHeight;
    switch (viewportType)
    {
    case VIEWPORT_TYPE_SPLIT_TOP: h = g_iScreenHeight / 2; y = g_iScreenHeight - h; break;
    case VIEWPORT_TYPE_SPLIT_BOTTOM: h = g_iScreenHeight / 2; y = 0; break;
    case VIEWPORT_TYPE_SPLIT_LEFT: w = g_iScreenWidth / 2; x = 0; break;
    case VIEWPORT_TYPE_SPLIT_RIGHT: w = g_iScreenWidth / 2; x = g_iScreenWidth - w; break;
    case VIEWPORT_TYPE_QUADRANT_TOP_LEFT: w = g_iScreenWidth / 2; h = g_iScreenHeight / 2; x = 0; y = g_iScreenHeight - h; break;
    case VIEWPORT_TYPE_QUADRANT_TOP_RIGHT: w = g_iScreenWidth / 2; h = g_iScreenHeight / 2; x = g_iScreenWidth - w; y = g_iScreenHeight - h; break;
    case VIEWPORT_TYPE_QUADRANT_BOTTOM_LEFT: w = g_iScreenWidth / 2; h = g_iScreenHeight / 2; x = 0; y = 0; break;
    case VIEWPORT_TYPE_QUADRANT_BOTTOM_RIGHT: w = g_iScreenWidth / 2; h = g_iScreenHeight / 2; x = g_iScreenWidth - w; y = 0; break;
    case VIEWPORT_TYPE_FULLSCREEN:
    default: break;
    }
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    mcglViewport(x, y, w, h);
}
void C4JRender::StateSetEnableViewportClipPlanes(bool /*enable*/) {}
void C4JRender::StateSetTexGenCol(int /*col*/, float /*x*/, float /*y*/, float /*z*/, float /*w*/, bool /*eyeSpace*/) {}
void C4JRender::StateSetStencil(int /*Function*/, uint8_t /*stencil_ref*/, uint8_t /*stencil_func_mask*/, uint8_t /*stencil_write_mask*/) {}
void C4JRender::StateSetForceLOD(int /*LOD*/) {}

// --- Event tracking -------------------------------------------------

void C4JRender::BeginEvent(LPCWSTR /*eventName*/) {}
void C4JRender::EndEvent() {}

// --- PLM event handling ---------------------------------------------

void C4JRender::Suspend() {}
bool C4JRender::Suspended() { return false; }
void C4JRender::Resume() {}


/* ********************************************************************
 *
 *  C_4JInput  --  Keyboard/mouse → virtual gamepad input
 *
 * ********************************************************************/

// --- Input state ---
static uint32_t s_joypadMaps[3][64] = {};   // maps[mapStyle][action] = button bitmask
static uint8_t  s_padMapStyle[4] = {};       // which map style each pad uses
static uint32_t s_buttons     = 0;           // current frame virtual button bitmask
static uint32_t s_buttonsPrev = 0;           // previous frame for edge detection
static float    s_stickLX = 0, s_stickLY = 0;
static float    s_stickRX = 0, s_stickRY = 0;
static uint8_t  s_triggerL = 0, s_triggerR = 0;
static bool     s_mouseCaptured = false;
static double   s_lastMouseX = 0, s_lastMouseY = 0;
static bool     s_firstMouse = true;
static bool     s_menuDisplayed = false;
static float    s_mouseSensitivity = 0.06f;

void C_4JInput::Initialise(int, unsigned char, unsigned char, unsigned char)
{
    if (g_window)
        glfwSetScrollCallback(g_window, inputScrollCallback);
}

void C_4JInput::Tick()
{
    if (!g_window) return;

    s_buttonsPrev = s_buttons;
    s_buttons = 0;
    s_stickLX = s_stickLY = 0;
    s_stickRX = s_stickRY = 0;
    s_triggerL = s_triggerR = 0;

    // --- WASD → left stick + digital stick buttons ---
    if (glfwGetKey(g_window, GLFW_KEY_W) == GLFW_PRESS) {
        s_buttons |= _360_JOY_BUTTON_LSTICK_UP;
        s_stickLY += 1.0f;
    }
    if (glfwGetKey(g_window, GLFW_KEY_S) == GLFW_PRESS) {
        s_buttons |= _360_JOY_BUTTON_LSTICK_DOWN;
        s_stickLY -= 1.0f;
    }
    if (glfwGetKey(g_window, GLFW_KEY_A) == GLFW_PRESS) {
        s_buttons |= _360_JOY_BUTTON_LSTICK_LEFT;
        s_stickLX -= 1.0f;
    }
    if (glfwGetKey(g_window, GLFW_KEY_D) == GLFW_PRESS) {
        s_buttons |= _360_JOY_BUTTON_LSTICK_RIGHT;
        s_stickLX += 1.0f;
    }
    // Clamp diagonal movement
    float stickLen = s_stickLX * s_stickLX + s_stickLY * s_stickLY;
    if (stickLen > 1.0f) {
        float inv = 1.0f / sqrtf(stickLen);
        s_stickLX *= inv;
        s_stickLY *= inv;
    }

    // --- Mouse → right stick (camera look) ---
    if (s_mouseCaptured) {
        double mx, my;
        glfwGetCursorPos(g_window, &mx, &my);
        if (!s_firstMouse) {
            float dx = (float)(mx - s_lastMouseX) * s_mouseSensitivity;
            float dy = (float)(my - s_lastMouseY) * s_mouseSensitivity;
            // Clamp to -1..1 range
            s_stickRX = (dx > 1.0f) ? 1.0f : (dx < -1.0f) ? -1.0f : dx;
            s_stickRY = (dy > 1.0f) ? 1.0f : (dy < -1.0f) ? -1.0f : dy;
            // Set digital stick buttons for look actions
            if (s_stickRX > 0.1f)  s_buttons |= _360_JOY_BUTTON_RSTICK_RIGHT;
            if (s_stickRX < -0.1f) s_buttons |= _360_JOY_BUTTON_RSTICK_LEFT;
            if (s_stickRY > 0.1f)  s_buttons |= _360_JOY_BUTTON_RSTICK_DOWN;
            if (s_stickRY < -0.1f) s_buttons |= _360_JOY_BUTTON_RSTICK_UP;
        }
        s_lastMouseX = mx;
        s_lastMouseY = my;
        s_firstMouse = false;
    }

    // --- Mouse buttons → triggers ---
    if (glfwGetMouseButton(g_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        s_buttons |= _360_JOY_BUTTON_RT;
        s_triggerR = 255;
    }
    if (glfwGetMouseButton(g_window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        s_buttons |= _360_JOY_BUTTON_LT;
        s_triggerL = 255;
    }
    if (glfwGetMouseButton(g_window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS)
        s_buttons |= _360_JOY_BUTTON_LTHUMB;

    // --- Face buttons ---
    if (glfwGetKey(g_window, GLFW_KEY_SPACE) == GLFW_PRESS)
        s_buttons |= _360_JOY_BUTTON_A;
    if (glfwGetKey(g_window, GLFW_KEY_Q) == GLFW_PRESS)
        s_buttons |= _360_JOY_BUTTON_B;
    if (glfwGetKey(g_window, GLFW_KEY_F) == GLFW_PRESS)
        s_buttons |= _360_JOY_BUTTON_X;
    if (glfwGetKey(g_window, GLFW_KEY_E) == GLFW_PRESS)
        s_buttons |= _360_JOY_BUTTON_Y;

    // --- Menu/system ---
    if (glfwGetKey(g_window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        s_buttons |= _360_JOY_BUTTON_START;
    if (glfwGetKey(g_window, GLFW_KEY_TAB) == GLFW_PRESS)
        s_buttons |= _360_JOY_BUTTON_BACK;
    if (glfwGetKey(g_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        s_buttons |= _360_JOY_BUTTON_RTHUMB;

    // --- Scroll wheel → bumpers (hotbar scroll) ---
    if (s_scrollAccum > 0.5f)  { s_buttons |= _360_JOY_BUTTON_RB; s_scrollAccum = 0; }
    if (s_scrollAccum < -0.5f) { s_buttons |= _360_JOY_BUTTON_LB; s_scrollAccum = 0; }

    // --- DPad (arrow keys) ---
    if (glfwGetKey(g_window, GLFW_KEY_UP) == GLFW_PRESS)
        s_buttons |= _360_JOY_BUTTON_DPAD_UP;
    if (glfwGetKey(g_window, GLFW_KEY_DOWN) == GLFW_PRESS)
        s_buttons |= _360_JOY_BUTTON_DPAD_DOWN;
    if (glfwGetKey(g_window, GLFW_KEY_LEFT) == GLFW_PRESS)
        s_buttons |= _360_JOY_BUTTON_DPAD_LEFT;
    if (glfwGetKey(g_window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        s_buttons |= _360_JOY_BUTTON_DPAD_RIGHT;

    // --- F5 → third person (LTHUMB) ---
    if (glfwGetKey(g_window, GLFW_KEY_F5) == GLFW_PRESS)
        s_buttons |= _360_JOY_BUTTON_LTHUMB;

    // --- Mouse capture: left-click grabs, Escape releases ---
    if (!s_mouseCaptured && !s_menuDisplayed &&
        glfwGetMouseButton(g_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
    {
        glfwSetInputMode(g_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        if (glfwRawMouseMotionSupported())
            glfwSetInputMode(g_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        s_mouseCaptured = true;
        s_firstMouse = true;
    }
    if (s_mouseCaptured && glfwGetKey(g_window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        glfwSetInputMode(g_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        s_mouseCaptured = false;
    }
}

void C_4JInput::SetDeadzoneAndMovementRange(unsigned int, unsigned int) {}

void C_4JInput::SetGameJoypadMaps(unsigned char ucMap, unsigned char ucAction, unsigned int uiActionVal)
{
    if (ucMap < 3 && ucAction < 64)
        s_joypadMaps[ucMap][ucAction] = uiActionVal;
}

unsigned int C_4JInput::GetGameJoypadMaps(unsigned char ucMap, unsigned char ucAction)
{
    if (ucMap < 3 && ucAction < 64)
        return s_joypadMaps[ucMap][ucAction];
    return 0;
}

void C_4JInput::SetJoypadMapVal(int iPad, unsigned char ucMap)
{
    if (iPad >= 0 && iPad < 4) s_padMapStyle[iPad] = ucMap;
}

unsigned char C_4JInput::GetJoypadMapVal(int iPad)
{
    return (iPad >= 0 && iPad < 4) ? s_padMapStyle[iPad] : 0;
}

void C_4JInput::SetJoypadSensitivity(int, float) {}

unsigned int C_4JInput::GetValue(int iPad, unsigned char ucAction, bool)
{
    if (iPad != 0) return 0;
    if (ucAction == 255) return s_buttons ? 1 : 0;
    unsigned int mapping = s_joypadMaps[s_padMapStyle[0]][ucAction < 64 ? ucAction : 0];
    return (s_buttons & mapping) ? 1 : 0;
}

bool C_4JInput::ButtonPressed(int iPad, unsigned char ucAction)
{
    if (iPad != 0) return false;
    if (ucAction == 255) {
        uint32_t newlyPressed = s_buttons & ~s_buttonsPrev;
        return newlyPressed != 0;
    }
    unsigned int mapping = s_joypadMaps[s_padMapStyle[0]][ucAction < 64 ? ucAction : 0];
    return (s_buttons & mapping) && !(s_buttonsPrev & mapping);
}

bool C_4JInput::ButtonReleased(int iPad, unsigned char ucAction)
{
    if (iPad != 0) return false;
    if (ucAction == 255) {
        uint32_t newlyReleased = s_buttonsPrev & ~s_buttons;
        return newlyReleased != 0;
    }
    unsigned int mapping = s_joypadMaps[s_padMapStyle[0]][ucAction < 64 ? ucAction : 0];
    return !(s_buttons & mapping) && (s_buttonsPrev & mapping);
}

bool C_4JInput::ButtonDown(int iPad, unsigned char ucAction)
{
    if (iPad != 0) return false;
    if (ucAction == 255) return s_buttons != 0;
    unsigned int mapping = s_joypadMaps[s_padMapStyle[0]][ucAction < 64 ? ucAction : 0];
    return (s_buttons & mapping) != 0;
}

void C_4JInput::SetJoypadStickAxisMap(int, unsigned int, unsigned int) {}
void C_4JInput::SetJoypadStickTriggerMap(int, unsigned int, unsigned int) {}

void C_4JInput::SetKeyRepeatRate(float, float) {}
void C_4JInput::SetDebugSequence(const char *, int(*)(LPVOID), LPVOID) {}

FLOAT C_4JInput::GetIdleSeconds(int) { return 0.0f; }

bool C_4JInput::IsPadConnected(int iPad)
{
    return (iPad == 0);
}

float C_4JInput::GetJoypadStick_LX(int iPad, bool) { return (iPad == 0) ? s_stickLX : 0.0f; }
float C_4JInput::GetJoypadStick_LY(int iPad, bool) { return (iPad == 0) ? s_stickLY : 0.0f; }
float C_4JInput::GetJoypadStick_RX(int iPad, bool) { return (iPad == 0) ? s_stickRX : 0.0f; }
float C_4JInput::GetJoypadStick_RY(int iPad, bool) { return (iPad == 0) ? s_stickRY : 0.0f; }
unsigned char C_4JInput::GetJoypadLTrigger(int iPad, bool) { return (iPad == 0) ? s_triggerL : (uint8_t)0; }
unsigned char C_4JInput::GetJoypadRTrigger(int iPad, bool) { return (iPad == 0) ? s_triggerR : (uint8_t)0; }

void C_4JInput::SetMenuDisplayed(int, bool bVal) { s_menuDisplayed = bVal; }

EKeyboardResult C_4JInput::RequestKeyboard(LPCWSTR /*Title*/, LPCWSTR /*Text*/, DWORD /*dwPad*/, UINT /*uiMaxChars*/,
                                           int(*/*Func*/)(LPVOID, const bool), LPVOID /*lpParam*/, C_4JInput::EKeyboardMode /*eMode*/)
{
    return EKeyboard_Cancelled;
}

void C_4JInput::GetText(uint16_t * /*UTF16String*/) {}

bool C_4JInput::VerifyStrings(WCHAR ** /*pwStringA*/, int /*iStringC*/,
                              int(*/*Func*/)(LPVOID, STRING_VERIFY_RESPONSE *), LPVOID /*lpParam*/)
{
    return true;
}

void C_4JInput::CancelQueuedVerifyStrings(int(*/*Func*/)(LPVOID, STRING_VERIFY_RESPONSE *), LPVOID /*lpParam*/) {}
void C_4JInput::CancelAllVerifyInProgress() {}


/* ********************************************************************
 *
 *  C4JStorage  --  Storage Manager stubs
 *
 * ********************************************************************/

C4JStorage::C4JStorage()
    : m_pStringTable(nullptr)
{
}

void C4JStorage::Tick() {}

// --- Messages -------------------------------------------------------

C4JStorage::EMessageResult C4JStorage::RequestMessageBox(UINT /*uiTitle*/, UINT /*uiText*/, UINT * /*uiOptionA*/, UINT /*uiOptionC*/,
                                                         DWORD /*dwPad*/, int(*/*Func*/)(LPVOID, int, const C4JStorage::EMessageResult),
                                                         LPVOID /*lpParam*/, C4JStringTable * /*pStringTable*/,
                                                         WCHAR * /*pwchFormatString*/, DWORD /*dwFocusButton*/)
{
    return EMessage_ResultAccept;
}

C4JStorage::EMessageResult C4JStorage::GetMessageBoxResult() { return EMessage_ResultAccept; }

// --- Save device ----------------------------------------------------

bool C4JStorage::SetSaveDevice(int(*/*Func*/)(LPVOID, const bool), LPVOID /*lpParam*/, bool /*bForceResetOfSaveDevice*/) { return true; }

// --- Save game ------------------------------------------------------

void C4JStorage::Init(unsigned int /*uiSaveVersion*/, LPCWSTR /*pwchDefaultSaveName*/, char * /*pszSavePackName*/,
                      int /*iMinimumSaveSize*/, int(*/*Func*/)(LPVOID, const ESavingMessage, int), LPVOID /*lpParam*/, LPCSTR /*szGroupID*/) {}

void C4JStorage::ResetSaveData() {}
void C4JStorage::SetDefaultSaveNameForKeyboardDisplay(LPCWSTR /*pwchDefaultSaveName*/) {}
void C4JStorage::SetSaveTitle(LPCWSTR /*pwchDefaultSaveName*/) {}
bool C4JStorage::GetSaveUniqueNumber(INT * /*piVal*/) { return false; }
bool C4JStorage::GetSaveUniqueFilename(char * /*pszName*/) { return false; }
void C4JStorage::SetSaveUniqueFilename(char * /*szFilename*/) {}
void C4JStorage::SetState(ESaveGameControlState /*eControlState*/, int(*/*Func*/)(LPVOID, const bool), LPVOID /*lpParam*/) {}
void C4JStorage::SetSaveDisabled(bool /*bDisable*/) {}
bool C4JStorage::GetSaveDisabled() { return false; }
unsigned int C4JStorage::GetSaveSize() { return 0; }
void C4JStorage::GetSaveData(void * /*pvData*/, unsigned int * /*puiBytes*/) {}
PVOID C4JStorage::AllocateSaveData(unsigned int uiBytes) { return (uiBytes > 0) ? calloc(1, uiBytes) : nullptr; }
void C4JStorage::SetSaveImages(PBYTE /*pbThumbnail*/, DWORD /*dwThumbnailBytes*/, PBYTE /*pbImage*/, DWORD /*dwImageBytes*/,
                               PBYTE /*pbTextData*/, DWORD /*dwTextDataBytes*/) {}

C4JStorage::ESaveGameState C4JStorage::SaveSaveData(int(*/*Func*/)(LPVOID, const bool), LPVOID /*lpParam*/)
{
    return ESaveGame_Idle;
}

void C4JStorage::CopySaveDataToNewSave(PBYTE /*pbThumbnail*/, DWORD /*cbThumbnail*/, WCHAR * /*wchNewName*/,
                                       int (*/*Func*/)(LPVOID lpParam, bool), LPVOID /*lpParam*/) {}

void C4JStorage::SetSaveDeviceSelected(unsigned int /*uiPad*/, bool /*bSelected*/) {}
bool C4JStorage::GetSaveDeviceSelected(unsigned int /*iPad*/) { return false; }

C4JStorage::ESaveGameState C4JStorage::DoesSaveExist(bool *pbExists)
{
    if (pbExists) *pbExists = false;
    return ESaveGame_Idle;
}

bool C4JStorage::EnoughSpaceForAMinSaveGame() { return true; }

void C4JStorage::SetSaveMessageVPosition(float /*fY*/) {}

C4JStorage::ESaveGameState C4JStorage::GetSavesInfo(int /*iPad*/, int (*/*Func*/)(LPVOID lpParam, SAVE_DETAILS *pSaveDetails, const bool),
                                                    LPVOID /*lpParam*/, char * /*pszSavePackName*/)
{
    return ESaveGame_Idle;
}

PSAVE_DETAILS C4JStorage::ReturnSavesInfo() { return nullptr; }
void C4JStorage::ClearSavesInfo() {}

C4JStorage::ESaveGameState C4JStorage::LoadSaveDataThumbnail(PSAVE_INFO /*pSaveInfo*/,
                                                             int(*/*Func*/)(LPVOID lpParam, PBYTE pbThumbnail, DWORD dwThumbnailBytes),
                                                             LPVOID /*lpParam*/)
{
    return ESaveGame_Idle;
}

void C4JStorage::GetSaveCacheFileInfo(DWORD /*dwFile*/, XCONTENT_DATA & /*xContentData*/) {}
void C4JStorage::GetSaveCacheFileInfo(DWORD /*dwFile*/, PBYTE * /*ppbImageData*/, DWORD * /*pdwImageBytes*/) {}

C4JStorage::ESaveGameState C4JStorage::LoadSaveData(PSAVE_INFO /*pSaveInfo*/,
                                                    int(*/*Func*/)(LPVOID lpParam, const bool, const bool), LPVOID /*lpParam*/)
{
    return ESaveGame_Idle;
}

C4JStorage::ESaveGameState C4JStorage::DeleteSaveData(PSAVE_INFO /*pSaveInfo*/,
                                                      int(*/*Func*/)(LPVOID lpParam, const bool), LPVOID /*lpParam*/)
{
    return ESaveGame_Idle;
}

// --- DLC ------------------------------------------------------------

void C4JStorage::RegisterMarketplaceCountsCallback(int (*/*Func*/)(LPVOID lpParam, C4JStorage::DLC_TMS_DETAILS *, int), LPVOID /*lpParam*/) {}
void C4JStorage::SetDLCPackageRoot(char * /*pszDLCRoot*/) {}

C4JStorage::EDLCStatus C4JStorage::GetDLCOffers(int /*iPad*/, int(*/*Func*/)(LPVOID, int, DWORD, int), LPVOID /*lpParam*/, DWORD /*dwOfferTypesBitmask*/)
{
    return EDLC_Idle;
}

DWORD C4JStorage::CancelGetDLCOffers() { return 0; }
void  C4JStorage::ClearDLCOffers() {}

XMARKETPLACE_CONTENTOFFER_INFO& C4JStorage::GetOffer(DWORD /*dw*/)
{
    static XMARKETPLACE_CONTENTOFFER_INFO s_dummyOffer;
    static bool s_init = false;
    if (!s_init) { memset(&s_dummyOffer, 0, sizeof(s_dummyOffer)); s_init = true; }
    return s_dummyOffer;
}

int C4JStorage::GetOfferCount() { return 0; }

DWORD C4JStorage::InstallOffer(int /*iOfferIDC*/, uint64_t * /*ullOfferIDA*/, int(*/*Func*/)(LPVOID, int, int), LPVOID /*lpParam*/, bool /*bTrial*/)
{
    return 0;
}

DWORD C4JStorage::GetAvailableDLCCount(int /*iPad*/) { return 0; }

C4JStorage::EDLCStatus C4JStorage::GetInstalledDLC(int /*iPad*/, int(*/*Func*/)(LPVOID, int, int), LPVOID /*lpParam*/)
{
    return EDLC_Idle;
}

XCONTENT_DATA& C4JStorage::GetDLC(DWORD /*dw*/)
{
    static XCONTENT_DATA s_dummyDLC;
    static bool s_init = false;
    if (!s_init) { memset(&s_dummyDLC, 0, sizeof(s_dummyDLC)); s_init = true; }
    return s_dummyDLC;
}

DWORD C4JStorage::MountInstalledDLC(int /*iPad*/, DWORD /*dwDLC*/, int(*/*Func*/)(LPVOID, int, DWORD, DWORD), LPVOID /*lpParam*/, LPCSTR /*szMountDrive*/)
{
    return 0;
}

DWORD C4JStorage::UnmountInstalledDLC(LPCSTR /*szMountDrive*/) { return 0; }

void C4JStorage::GetMountedDLCFileList(const char * /*szMountDrive*/, std::vector<std::string>& /*fileList*/) {}

std::string C4JStorage::GetMountedPath(std::string /*szMount*/)
{
    return std::string();
}

// --- TMS / Global title storage -------------------------------------

C4JStorage::ETMSStatus C4JStorage::ReadTMSFile(int /*iQuadrant*/, eGlobalStorage /*eStorageFacility*/, C4JStorage::eTMS_FileType /*eFileType*/,
                                               WCHAR * /*pwchFilename*/, BYTE ** /*ppBuffer*/, DWORD * /*pdwBufferSize*/,
                                               int(*/*Func*/)(LPVOID, WCHAR *, int, bool, int), LPVOID /*lpParam*/, int /*iAction*/)
{
    return ETMSStatus_Idle;
}

bool C4JStorage::WriteTMSFile(int /*iQuadrant*/, eGlobalStorage /*eStorageFacility*/, WCHAR * /*pwchFilename*/, BYTE * /*pBuffer*/, DWORD /*dwBufferSize*/)
{
    return false;
}

bool C4JStorage::DeleteTMSFile(int /*iQuadrant*/, eGlobalStorage /*eStorageFacility*/, WCHAR * /*pwchFilename*/)
{
    return false;
}

void C4JStorage::StoreTMSPathName(WCHAR * /*pwchName*/) {}

// --- TMS++ ----------------------------------------------------------

C4JStorage::ETMSStatus C4JStorage::TMSPP_ReadFile(int /*iPad*/, C4JStorage::eGlobalStorage /*eStorageFacility*/,
                                                  C4JStorage::eTMS_FILETYPEVAL /*eFileTypeVal*/, LPCSTR /*szFilename*/,
                                                  int(*/*Func*/)(LPVOID, int, int, PTMSPP_FILEDATA, LPCSTR), LPVOID /*lpParam*/, int /*iUserData*/)
{
    return ETMSStatus_Idle;
}

// --- CRC ------------------------------------------------------------

unsigned int C4JStorage::CRC(unsigned char *buf, int len) {
    /* Standard CRC-32 (ISO 3309 / ITU-T V.42) */
    unsigned int crc = 0xFFFFFFFF;
    for (int i = 0; i < len; i++) {
        crc ^= buf[i];
        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
    }
    return ~crc;
}


/* ********************************************************************
 *
 *  C_4JProfile  --  Profile Manager stubs
 *
 * ********************************************************************/

// Per-player profile data buffer (zeroed on startup, sized by Initialise)
static unsigned char s_profileData[4 * 4096];  // 4 players, up to 4 KiB each
static int s_profileDataPerPlayer = 4096;

void C_4JProfile::Initialise(DWORD /*dwTitleID*/, DWORD /*dwOfferID*/, unsigned short /*usProfileVersion*/,
                             UINT /*uiProfileValuesC*/, UINT /*uiProfileSettingsC*/, DWORD * /*pdwProfileSettingsA*/,
                             int iGameDefinedDataSizeX4, unsigned int *puiGameDefinedDataChangedBitmask)
{
    if (iGameDefinedDataSizeX4 > 0) {
        int perPlayer = iGameDefinedDataSizeX4 / 4;
        if (perPlayer > 0 && perPlayer <= 4096)
            s_profileDataPerPlayer = perPlayer;
    }
    memset(s_profileData, 0, sizeof(s_profileData));
    if (puiGameDefinedDataChangedBitmask) *puiGameDefinedDataChangedBitmask = 0;
}

void C_4JProfile::SetTrialTextStringTable(CXuiStringTable * /*pStringTable*/, int /*iAccept*/, int /*iReject*/) {}
void C_4JProfile::SetTrialAwardText(eAwardType /*AwardType*/, int /*iTitle*/, int /*iText*/) {}

int  C_4JProfile::GetLockedProfile() { return 0; }
void C_4JProfile::SetLockedProfile(int /*iProf*/) {}

bool C_4JProfile::IsSignedIn(int iQuadrant)
{
    // Quadrant / pad 0 is always "signed in" so the game can proceed
    return (iQuadrant == 0);
}

bool C_4JProfile::IsSignedInLive(int /*iProf*/) { return false; }
bool C_4JProfile::IsGuest(int /*iQuadrant*/)    { return false; }

UINT C_4JProfile::RequestSignInUI(bool /*bFromInvite*/, bool /*bLocalGame*/, bool /*bNoGuestsAllowed*/,
                                  bool /*bMultiplayerSignIn*/, bool /*bAddUser*/,
                                  int(*/*Func*/)(LPVOID, const bool, const int iPad), LPVOID /*lpParam*/, int /*iQuadrant*/)
{
    return 0;
}

UINT C_4JProfile::DisplayOfflineProfile(int(*/*Func*/)(LPVOID, const bool, const int iPad), LPVOID /*lpParam*/, int /*iQuadrant*/)
{
    return 0;
}

UINT C_4JProfile::RequestConvertOfflineToGuestUI(int(*/*Func*/)(LPVOID, const bool, const int iPad), LPVOID /*lpParam*/, int /*iQuadrant*/)
{
    return 0;
}

void C_4JProfile::SetPrimaryPlayerChanged(bool /*bVal*/) {}
bool C_4JProfile::QuerySigninStatus() { return true; }

void C_4JProfile::GetXUID(int /*iPad*/, PlayerUID *pXuid, bool /*bOnlineXuid*/)
{
    if (pXuid) { memset(pXuid, 0, sizeof(PlayerUID)); }
}

BOOL C_4JProfile::AreXUIDSEqual(PlayerUID xuid1, PlayerUID xuid2)
{
    return (xuid1 == xuid2) ? TRUE : FALSE;
}

BOOL C_4JProfile::XUIDIsGuest(PlayerUID /*xuid*/) { return FALSE; }

bool C_4JProfile::AllowedToPlayMultiplayer(int /*iProf*/) { return true; }

bool C_4JProfile::GetChatAndContentRestrictions(int /*iPad*/, bool *pbChatRestricted, bool *pbContentRestricted, int *piAge)
{
    if (pbChatRestricted)    *pbChatRestricted    = false;
    if (pbContentRestricted) *pbContentRestricted = false;
    if (piAge)               *piAge               = 18;
    return false;
}

void C_4JProfile::StartTrialGame() {}

void C_4JProfile::AllowedPlayerCreatedContent(int /*iPad*/, bool /*thisQuadrantOnly*/, BOOL *allAllowed, BOOL *friendsAllowed)
{
    if (allAllowed)     *allAllowed     = TRUE;
    if (friendsAllowed) *friendsAllowed = TRUE;
}

BOOL C_4JProfile::CanViewPlayerCreatedContent(int /*iPad*/, bool /*thisQuadrantOnly*/, PPlayerUID /*pXuids*/, DWORD /*dwXuidCount*/)
{
    return TRUE;
}

void C_4JProfile::ShowProfileCard(int /*iPad*/, PlayerUID /*targetUid*/) {}

bool C_4JProfile::GetProfileAvatar(int /*iPad*/, int(*/*Func*/)(LPVOID lpParam, PBYTE pbThumbnail, DWORD dwThumbnailBytes), LPVOID /*lpParam*/)
{
    return false;
}

void C_4JProfile::CancelProfileAvatarRequest() {}

// --- SYS ------------------------------------------------------------

int  C_4JProfile::GetPrimaryPad() { return 0; }
void C_4JProfile::SetPrimaryPad(int /*iPad*/) {}

char *C_4JProfile::GetGamertag(int /*iPad*/)
{
    static char s_gamertag[] = "Player";
    return s_gamertag;
}

wstring C_4JProfile::GetDisplayName(int /*iPad*/)
{
    return L"Player";
}

bool C_4JProfile::IsFullVersion() { return true; }

void C_4JProfile::SetSignInChangeCallback(void (*/*Func*/)(LPVOID, bool, unsigned int), LPVOID /*lpParam*/) {}
void C_4JProfile::SetNotificationsCallback(void (*/*Func*/)(LPVOID, DWORD, unsigned int), LPVOID /*lpParam*/) {}

bool C_4JProfile::RegionIsNorthAmerica() { return false; }
bool C_4JProfile::LocaleIsUSorCanada()   { return false; }

HRESULT C_4JProfile::GetLiveConnectionStatus() { return E_FAIL; }

bool C_4JProfile::IsSystemUIDisplayed() { return false; }

void C_4JProfile::SetProfileReadErrorCallback(void (*/*Func*/)(LPVOID), LPVOID /*lpParam*/) {}

// --- Profile data ---------------------------------------------------

int C_4JProfile::SetDefaultOptionsCallback(int(*/*Func*/)(LPVOID, PROFILESETTINGS *, const int iPad), LPVOID /*lpParam*/) { return 0; }
int C_4JProfile::SetOldProfileVersionCallback(int(*/*Func*/)(LPVOID, unsigned char *, const unsigned short, const int), LPVOID /*lpParam*/) { return 0; }

C_4JProfile::PROFILESETTINGS *C_4JProfile::GetDashboardProfileSettings(int /*iPad*/) { return nullptr; }

void C_4JProfile::WriteToProfile(int /*iQuadrant*/, bool /*bGameDefinedDataChanged*/, bool /*bOverride5MinuteLimitOnProfileWrites*/) {}
void C_4JProfile::ForceQueuedProfileWrites(int /*iPad*/) {}
void *C_4JProfile::GetGameDefinedProfileData(int iQuadrant)
{
    if (iQuadrant < 0 || iQuadrant >= 4) return nullptr;
    return &s_profileData[iQuadrant * s_profileDataPerPlayer];
}
void C_4JProfile::ResetProfileProcessState() {}
void C_4JProfile::Tick() {}

// --- Achievements & Awards ------------------------------------------

void C_4JProfile::RegisterAward(int /*iAwardNumber*/, int /*iGamerconfigID*/, eAwardType /*eType*/, bool /*bLeaderboardAffected*/,
                                CXuiStringTable * /*pStringTable*/, int /*iTitleStr*/, int /*iTextStr*/, int /*iAcceptStr*/,
                                char * /*pszThemeName*/, unsigned int /*uiThemeSize*/) {}

int       C_4JProfile::GetAwardId(int /*iAwardNumber*/) { return 0; }
eAwardType C_4JProfile::GetAwardType(int /*iAwardNumber*/) { return eAwardType_Achievement; }
bool      C_4JProfile::CanBeAwarded(int /*iQuadrant*/, int /*iAwardNumber*/) { return true; }
void      C_4JProfile::Award(int /*iQuadrant*/, int /*iAwardNumber*/, bool /*bForce*/) {}
bool      C_4JProfile::IsAwardsFlagSet(int /*iQuadrant*/, int /*iAward*/) { return false; }

// --- Rich Presence --------------------------------------------------

void C_4JProfile::RichPresenceInit(int /*iPresenceCount*/, int /*iContextCount*/) {}
void C_4JProfile::RegisterRichPresenceContext(int /*iGameConfigContextID*/) {}
void C_4JProfile::SetRichPresenceContextValue(int /*iPad*/, int /*iContextID*/, int /*iVal*/) {}
void C_4JProfile::SetCurrentGameActivity(int /*iPad*/, int /*iNewPresence*/, bool /*bSetOthersToIdle*/) {}

// --- Purchase -------------------------------------------------------

void C_4JProfile::DisplayFullVersionPurchase(bool /*bRequired*/, int /*iQuadrant*/, int /*iUpsellParam*/) {}
void C_4JProfile::SetUpsellCallback(void (*/*Func*/)(LPVOID lpParam, eUpsellType type, eUpsellResponse response, int iUserData), LPVOID /*lpParam*/) {}

// --- Debug ----------------------------------------------------------

void C_4JProfile::SetDebugFullOverride(bool /*bVal*/) {}
