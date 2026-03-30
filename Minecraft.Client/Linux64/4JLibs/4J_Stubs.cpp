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
int g_nextTextureId = 1;
int g_nextCBuffId = 1;
std::unordered_map<int, TextureInfo> g_textures;
std::unordered_map<int, std::vector<DrawCall>> g_cbuffs;
std::mutex g_cbuffMutex;
thread_local int tl_recordingCbuff = -1;
thread_local std::vector<DrawCall> tl_recordingCalls;
thread_local int tl_currentTexture = -1;
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

    float o[16];
    matIdentity(o);
    o[0] = 2.0f / rl;
    o[5] = 2.0f / tb;
    o[10] = -2.0f / fn;
    o[12] = -(right + left) / rl;
    o[13] = -(top + bottom) / tb;
    o[14] = -(zFar + zNear) / fn;
    matPostMul(currentSoftMatrix(), o);
}

static void softFrustum(float left, float right, float bottom, float top, float zNear, float zFar)
{
    const float rl = right - left;
    const float tb = top - bottom;
    const float fn = zFar - zNear;
    if (std::fabs(rl) <= 1e-8f || std::fabs(tb) <= 1e-8f || std::fabs(fn) <= 1e-8f || zNear <= 0.0f || zFar <= 0.0f)
        return;

    float f[16];
    memset(f, 0, sizeof(f));
    f[0] = (2.0f * zNear) / rl;
    f[5] = (2.0f * zNear) / tb;
    f[8] = (right + left) / rl;
    f[9] = (top + bottom) / tb;
    f[10] = -(zFar + zNear) / fn;
    f[11] = -1.0f;
    f[14] = -(2.0f * zFar * zNear) / fn;
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
            SessionLog_Printf("[linuxgl] glfwInit failed\n");
            return false;
        }
        glfwInitDone = true;
    }

    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    // Request compat profile for fixed-function GL (glBegin/End, matrix stack, lighting, fog).
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
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
        SessionLog_Printf("[linuxgl] failed to create GLFW window\n");
        return false;
    }

    glfwMakeContextCurrent(g_window);
    glfwShowWindow(g_window);
    glfwSwapInterval(1);
    glfwGetFramebufferSize(g_window, &g_iScreenWidth, &g_iScreenHeight);
    if (g_iScreenWidth < 1) g_iScreenWidth = 1;
    if (g_iScreenHeight < 1) g_iScreenHeight = 1;
    g_iAspectRatio = (float)g_iScreenWidth / (float)g_iScreenHeight;
    mcglViewport(0, 0, g_iScreenWidth, g_iScreenHeight);
    mcglEnable(0x0DE1); // GL_TEXTURE_2D
    mcglEnable(0x0B71); // GL_DEPTH_TEST
    mcglDepthFunc(0x0203); // GL_LEQUAL
    mcglClearColor(g_clearRGBA[0], g_clearRGBA[1], g_clearRGBA[2], g_clearRGBA[3]);

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

static void executeDraw(const DrawCall &call)
{
    if (call.count <= 0 || call.bytes.empty())
        return;

    // Conservative fixed-function baseline for replayed world draws.
    // Legacy state leakage here can cause missing faces / incorrect block colors.
    mcglDisable(0x0B44); // GL_CULL_FACE
    mcglFrontFace(0x0901); // GL_CCW
    mcglDisable(0x0B50); // GL_LIGHTING
    mcglDisable(0x4000); // GL_LIGHT0
    mcglDisable(0x4001); // GL_LIGHT1
    mcglColor4ub(255, 255, 255, 255);

    int effectiveTextureId = call.textureId;
    if (effectiveTextureId <= 0)
    {
        if (g_currentTexture > 0)
            effectiveTextureId = g_currentTexture;
        else if (tl_currentTexture > 0)
            effectiveTextureId = tl_currentTexture;
        else
            effectiveTextureId = g_sharedTextureId.load(std::memory_order_relaxed);
    }

    if (effectiveTextureId > 0)
    {
        auto it = g_textures.find(effectiveTextureId);
        if (it != g_textures.end())
        {
            mcglEnable(0x0DE1); // GL_TEXTURE_2D
            mcglBindTexture(0x0DE1, it->second.glId);
        }
        else
        {
            mcglDisable(0x0DE1); // GL_TEXTURE_2D
            mcglBindTexture(0x0DE1, 0);
        }
    }
    else
    {
        mcglDisable(0x0DE1); // GL_TEXTURE_2D
        mcglBindTexture(0x0DE1, 0);
    }

    if (call.hasMatrices)
    {
        mcglMatrixMode(0x1700); // GL_MODELVIEW
        mcglPushMatrix();
        // Display-list style replay should preserve the caller's live camera/projection state.
        // Apply recorded list-local transform on top of current modelview instead of replacing it.
        mcglMultMatrixf(call.modelview);
    }

    // Avoid leaked texture-matrix state flattening/distorting UVs during replay.
    mcglMatrixMode(0x1702); // GL_TEXTURE
    mcglPushMatrix();
    mcglLoadIdentity();
    mcglMatrixMode(0x1700); // GL_MODELVIEW

    const bool isQuadList = (call.primitive == C4JRender::PRIMITIVE_TYPE_QUAD_LIST);
    mcglBegin(isQuadList ? 0x0004 : mapPrimitive(call.primitive)); // GL_TRIANGLES for quads
    if (call.vtype == C4JRender::VERTEX_TYPE_COMPRESSED)
    {
        const int16_t *v = reinterpret_cast<const int16_t *>(call.bytes.data());
        auto emitCompVertex = [&](int i)
        {
            const int16_t *s = v + (i * 8);
            const float x = (float)s[0] / 1024.0f;
            const float y = (float)s[1] / 1024.0f;
            const float z = (float)s[2] / 1024.0f;
            const float u = (float)s[4] / 8192.0f;
            const float t = (float)s[5] / 8192.0f;
            const unsigned short packed = (unsigned short)(((int)s[3] + 32768) & 0xFFFF);

            unsigned char r = 255, g = 255, b = 255;
            decode565(packed, r, g, b);
            mcglColor4ub(r, g, b, 255);
            mcglTexCoord2f(u, t);
            mcglVertex3f(x, y, z);
        };

        if (isQuadList)
        {
            for (int i = 0; i + 3 < call.count; i += 4)
            {
                emitCompVertex(i + 0);
                emitCompVertex(i + 1);
                emitCompVertex(i + 2);
                emitCompVertex(i + 0);
                emitCompVertex(i + 2);
                emitCompVertex(i + 3);
            }
        }
        else
        {
            for (int i = 0; i < call.count; ++i)
                emitCompVertex(i);
        }
    }
    else
    {
        const uint32_t *v = reinterpret_cast<const uint32_t *>(call.bytes.data());
        auto emitStdVertex = [&](int i)
        {
            const uint32_t *s = v + (i * 8);
            const float x = *reinterpret_cast<const float *>(&s[0]);
            const float y = *reinterpret_cast<const float *>(&s[1]);
            const float z = *reinterpret_cast<const float *>(&s[2]);
            float u = *reinterpret_cast<const float *>(&s[3]);
            float t = *reinterpret_cast<const float *>(&s[4]);
            const uint32_t c = s[5];
            // Tesselator packs color as 0xRRGGBBAA.
            mcglColor4ub((c >> 24) & 0xFF,
                         (c >> 16) & 0xFF,
                         (c >> 8) & 0xFF,
                         c & 0xFF);
            mcglTexCoord2f(u, t);
            mcglVertex3f(x, y, z);
        };

        if (isQuadList)
        {
            for (int i = 0; i + 3 < call.count; i += 4)
            {
                emitStdVertex(i + 0);
                emitStdVertex(i + 1);
                emitStdVertex(i + 2);
                emitStdVertex(i + 0);
                emitStdVertex(i + 2);
                emitStdVertex(i + 3);
            }
        }
        else
        {
            for (int i = 0; i < call.count; ++i)
                emitStdVertex(i);
        }
    }
    mcglEnd();

    mcglMatrixMode(0x1702); // GL_TEXTURE
    mcglPopMatrix();
    mcglMatrixMode(0x1700); // GL_MODELVIEW

    if (call.hasMatrices)
    {
        mcglPopMatrix();            // modelview
        mcglMatrixMode(0x1700);     // GL_MODELVIEW
    }
}

} // namespace

// --- Core -----------------------------------------------------------

void C4JRender::Initialise(void * /*pDevice*/, void * /*pSwapChain*/) { ensureGLReady(); }
void C4JRender::InitialiseContext() { ensureGLReady(); }
void C4JRender::Tick()
{
    if (!ensureGLReady() || std::this_thread::get_id() != g_renderThread)
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
    if (!ensureGLReady() || std::this_thread::get_id() != g_renderThread) return;
    // Force sane baseline state each frame to prevent inter-frame state leaks.
    mcglColorMask(1, 1, 1, 1);
    mcglDepthMask(1);
    mcglDisable(0x0C11); // GL_SCISSOR_TEST
    mcglDisable(0x0BE2); // GL_BLEND
    mcglEnable(0x0B71);  // GL_DEPTH_TEST
    mcglDepthFunc(0x0203); // GL_LEQUAL
    mcglAlphaFunc(0x0204, 0.1f); // GL_GREATER, 0.1f
    mcglBlendFunc(0x0302, 0x0303); // GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA
}
void C4JRender::Present()
{
    if (!ensureGLReady() || std::this_thread::get_id() != g_renderThread)
        return;
    glfwSwapBuffers(g_window);
}
void C4JRender::Clear(int flags, D3D11_RECT *pRect)
{
    if (!ensureGLReady() || std::this_thread::get_id() != g_renderThread)
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
    if (ensureGLReady() && std::this_thread::get_id() == g_renderThread)
        mcglClearColor(g_clearRGBA[0], g_clearRGBA[1], g_clearRGBA[2], g_clearRGBA[3]);
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

    if (!ensureGLReady() || std::this_thread::get_id() != g_renderThread) return;
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
    static thread_local float mat[16];
    if (!ensureGLReady() || std::this_thread::get_id() != g_renderThread)
    {
        memset(mat, 0, sizeof(mat));
        mat[0] = mat[5] = mat[10] = mat[15] = 1.0f;
        return mat;
    }
    unsigned int pname = 0x0BA6; // GL_MODELVIEW_MATRIX
    if (type == GL_PROJECTION || type == GL_PROJECTION_MATRIX)
        pname = 0x0BA7; // GL_PROJECTION_MATRIX
    else if (type == GL_TEXTURE)
        pname = 0x0BA8; // GL_TEXTURE_MATRIX
    mcglGetFloatv(pname, mat);
    return mat;
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
    call.hasMatrices = true;
    memcpy(call.modelview, tl_softMatrices.model, sizeof(call.modelview));
    memcpy(call.projection, tl_softMatrices.proj, sizeof(call.projection));
    call.textureId = tl_currentTexture;
    call.globalUV[0] = g_sharedUVU.load(std::memory_order_relaxed);
    call.globalUV[1] = g_sharedUVV.load(std::memory_order_relaxed);

    if (tl_recordingCbuff >= 0)
    {
        tl_recordingCalls.push_back(std::move(call));
        return;
    }

    if (!ensureGLReady() || std::this_thread::get_id() != g_renderThread)
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
    if (tl_currentTexture <= 0)
    {
        if (g_currentTexture > 0) tl_currentTexture = g_currentTexture;
        else tl_currentTexture = g_sharedTextureId.load(std::memory_order_relaxed);
    }
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
}
bool C4JRender::CBuffCall(int index, bool /*full*/)
{
    if (!ensureGLReady() || std::this_thread::get_id() != g_renderThread)
        return false;
    std::vector<DrawCall> local;
    {
        std::lock_guard<std::mutex> lock(g_cbuffMutex);
        auto it = g_cbuffs.find(index);
        if (it == g_cbuffs.end()) return false;
        local = it->second;
    }
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
        // Command-buffer playback should preserve bound texture state across draws.
        // Some recorded draws do not carry an explicit texture id and rely on prior bind.
        if (c.textureId > 0)
        {
            replayTextureId = c.textureId;
        }
        else if (replayTextureId > 0)
        {
            c.textureId = replayTextureId;
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
    if (!ensureGLReady() || std::this_thread::get_id() != g_renderThread) return 0;
    unsigned int glTex = 0;
    mcglGenTextures(1, &glTex);
    if (glTex == 0) return 0;
    int id = g_nextTextureId++;
    g_textures[id].glId = glTex;
    return id;
}

void C4JRender::TextureFree(int idx)
{
    if (!ensureGLReady() || std::this_thread::get_id() != g_renderThread) return;
    auto it = g_textures.find(idx);
    if (it == g_textures.end()) return;
    unsigned int glTex = it->second.glId;
    mcglDeleteTextures(1, &glTex);
    g_textures.erase(it);
}
void C4JRender::TextureBind(int idx)
{
    // Track logical bind state even off render thread (CBuff recording happens there).
    tl_currentTexture = (idx > 0) ? idx : -1;
    g_sharedTextureId.store(tl_currentTexture, std::memory_order_relaxed);
    if (!ensureGLReady() || std::this_thread::get_id() != g_renderThread) return;
    if (idx <= 0) { g_currentTexture = -1; mcglDisable(0x0DE1); mcglBindTexture(0x0DE1, 0); return; }
    auto it = g_textures.find(idx);
    if (it == g_textures.end()) return;
    g_currentTexture = idx;
    mcglEnable(0x0DE1); // GL_TEXTURE_2D
    mcglBindTexture(0x0DE1, it->second.glId);
}
void C4JRender::TextureBindVertex(int idx) { TextureBind(idx); }
void C4JRender::TextureSetTextureLevels(int levels) { g_textureLevels = (levels > 0) ? levels : 1; }
int  C4JRender::TextureGetTextureLevels() { return g_textureLevels; }
void C4JRender::TextureData(int width, int height, void *data, int level, eTextureFormat /*format*/)
{
    if (!ensureGLReady() || std::this_thread::get_id() != g_renderThread) return;
    if (g_currentTexture <= 0 || !data || width <= 0 || height <= 0) return;
    // Keep upload in RGBA order for desktop GL fixed-function sampling.
    mcglTexImage2D(0x0DE1, level, 0x1908, width, height, 0, 0x1908, 0x1401, data); // GL_RGBA/UNSIGNED_BYTE
}
void C4JRender::TextureDataUpdate(int xoffset, int yoffset, int width, int height, void *data, int level)
{
    if (!ensureGLReady() || std::this_thread::get_id() != g_renderThread) return;
    if (g_currentTexture <= 0 || !data || width <= 0 || height <= 0) return;
    mcglTexSubImage2D(0x0DE1, level, xoffset, yoffset, width, height, 0x1908, 0x1401, data); // GL_RGBA
}
void C4JRender::TextureSetParam(int param, int value)
{
    if (!ensureGLReady() || std::this_thread::get_id() != g_renderThread) return;
    int pname = 0x2801; // GL_TEXTURE_MIN_FILTER
    int mapped = 0x2600; // GL_NEAREST
    if (param == GL_TEXTURE_MIN_FILTER)
    {
        pname = 0x2801;
        // Keep minification non-mipmapped unless we have complete mip chains for all textures.
        mapped = (value == GL_LINEAR) ? 0x2601 : 0x2600;
    }
    else if (param == GL_TEXTURE_MAG_FILTER)
    {
        pname = 0x2800;
        mapped = (value == GL_LINEAR) ? 0x2601 : 0x2600;
    }
    else if (param == GL_TEXTURE_WRAP_S)
    {
        pname = 0x2802;
        mapped = (value == GL_REPEAT) ? 0x2901 : 0x812F;
    }
    else if (param == GL_TEXTURE_WRAP_T)
    {
        pname = 0x2803;
        mapped = (value == GL_REPEAT) ? 0x2901 : 0x812F;
    }
    else
    {
        return;
    }
    mcglTexParameteri(0x0DE1, pname, mapped);
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
    if (ensureGLReady() && std::this_thread::get_id() == g_renderThread) mcglColor4f(r, g, b, a);
}
void C4JRender::StateSetDepthMask(bool enable)
{
    if (ensureGLReady() && std::this_thread::get_id() == g_renderThread) mcglDepthMask(enable ? 1 : 0);
}
void C4JRender::StateSetBlendEnable(bool enable)
{
    if (!ensureGLReady() || std::this_thread::get_id() != g_renderThread) return;
    if (enable) mcglEnable(0x0BE2); else mcglDisable(0x0BE2); // GL_BLEND
}
void C4JRender::StateSetBlendFunc(int src, int dst)
{
    if (!ensureGLReady() || std::this_thread::get_id() != g_renderThread) return;
    auto map = [](int f) -> unsigned int {
        switch (f) {
        case GL_ZERO: return 0;
        case GL_ONE: return 1;
        case GL_SRC_COLOR: return 0x0300;
        case GL_ONE_MINUS_SRC_COLOR: return 0x0301;
        case GL_SRC_ALPHA: return 0x0302;
        case GL_ONE_MINUS_SRC_ALPHA: return 0x0303;
        case GL_DST_ALPHA: return 0x0304;
        case GL_DST_COLOR: return 0x0306;
        case GL_ONE_MINUS_DST_COLOR: return 0x0307;
        case GL_CONSTANT_ALPHA: return 0x8003;
        case GL_ONE_MINUS_CONSTANT_ALPHA: return 0x8004;
        default: return 1;
        }
    };
    mcglBlendFunc(map(src), map(dst));
}
void C4JRender::StateSetBlendFactor(unsigned int colour)
{
    if (!ensureGLReady() || std::this_thread::get_id() != g_renderThread) return;
    mcglBlendColor(((colour >> 16) & 0xFF) / 255.0f, ((colour >> 8) & 0xFF) / 255.0f, (colour & 0xFF) / 255.0f, ((colour >> 24) & 0xFF) / 255.0f);
}
void C4JRender::StateSetAlphaFunc(int func, float param)
{
    if (!ensureGLReady() || std::this_thread::get_id() != g_renderThread) return;
    unsigned int f = 0x0201; // GL_LESS
    if (func == GL_EQUAL) f = 0x0202;
    else if (func == GL_LEQUAL) f = 0x0203;
    else if (func == GL_GREATER) f = 0x0204;
    else if (func == GL_GEQUAL) f = 0x0206;
    else if (func == GL_ALWAYS) f = 0x0207;
    mcglAlphaFunc(f, param);
}
void C4JRender::StateSetDepthFunc(int func)
{
    if (!ensureGLReady() || std::this_thread::get_id() != g_renderThread) return;
    unsigned int f = 0x0201; // GL_LESS
    if (func == GL_EQUAL) f = 0x0202;
    else if (func == GL_LEQUAL) f = 0x0203;
    else if (func == GL_GREATER) f = 0x0204;
    else if (func == GL_GEQUAL) f = 0x0206;
    else if (func == GL_ALWAYS) f = 0x0207;
    mcglDepthFunc(f);
}
void C4JRender::StateSetFaceCull(bool enable)
{
    if (!ensureGLReady() || std::this_thread::get_id() != g_renderThread) return;
    if (enable) mcglEnable(0x0B44); else mcglDisable(0x0B44); // GL_CULL_FACE
}
void C4JRender::StateSetFaceCullCW(bool enable)
{
    if (ensureGLReady() && std::this_thread::get_id() == g_renderThread) mcglFrontFace(enable ? 0x0900 : 0x0901);
}
void C4JRender::StateSetLineWidth(float width)
{
    if (ensureGLReady() && std::this_thread::get_id() == g_renderThread) mcglLineWidth(width);
}
void C4JRender::StateSetWriteEnable(bool red, bool green, bool blue, bool alpha)
{
    if (ensureGLReady() && std::this_thread::get_id() == g_renderThread)
        mcglColorMask(red ? 1 : 0, green ? 1 : 0, blue ? 1 : 0, alpha ? 1 : 0);
}
void C4JRender::StateSetDepthTestEnable(bool enable)
{
    if (!ensureGLReady() || std::this_thread::get_id() != g_renderThread) return;
    if (enable) mcglEnable(0x0B71); else mcglDisable(0x0B71); // GL_DEPTH_TEST
}
void C4JRender::StateSetAlphaTestEnable(bool enable)
{
    if (!ensureGLReady() || std::this_thread::get_id() != g_renderThread) return;
    if (enable) mcglEnable(0x0BC0); else mcglDisable(0x0BC0); // GL_ALPHA_TEST
}
void C4JRender::StateSetDepthSlopeAndBias(float slope, float bias)
{
    if (!ensureGLReady() || std::this_thread::get_id() != g_renderThread) return;
    if (slope != 0.0f || bias != 0.0f) { mcglEnable(0x8037); mcglPolygonOffset(slope, bias); } else mcglDisable(0x8037);
}
void C4JRender::StateSetFogEnable(bool enable) { if (!ensureGLReady() || std::this_thread::get_id() != g_renderThread) return; if (enable) mcglEnable(0x0B60); else mcglDisable(0x0B60); }
void C4JRender::StateSetFogMode(int mode) { if (ensureGLReady() && std::this_thread::get_id() == g_renderThread) mcglFogi(0x0B65, (mode == GL_EXP) ? 0x0800 : 0x2601); }
void C4JRender::StateSetFogNearDistance(float dist) { if (ensureGLReady() && std::this_thread::get_id() == g_renderThread) mcglFogf(0x0B63, dist); }
void C4JRender::StateSetFogFarDistance(float dist) { if (ensureGLReady() && std::this_thread::get_id() == g_renderThread) mcglFogf(0x0B64, dist); }
void C4JRender::StateSetFogDensity(float density) { if (ensureGLReady() && std::this_thread::get_id() == g_renderThread) mcglFogf(0x0B62, density); }
void C4JRender::StateSetFogColour(float red, float green, float blue) { if (!ensureGLReady() || std::this_thread::get_id() != g_renderThread) return; float c[4] = {red, green, blue, 1.0f}; mcglFogfv(0x0B66, c); }
void C4JRender::StateSetLightingEnable(bool enable)
{
    if (!ensureGLReady() || std::this_thread::get_id() != g_renderThread) return;
    if (enable) mcglEnable(0x0B50); else mcglDisable(0x0B50); // GL_LIGHTING
}
void C4JRender::StateSetVertexTextureUV(float u, float v)
{
    tl_globalUV[0] = u;
    tl_globalUV[1] = v;
    g_sharedUVU.store(u, std::memory_order_relaxed);
    g_sharedUVV.store(v, std::memory_order_relaxed);
}
void C4JRender::StateSetLightColour(int light, float red, float green, float blue) { if (!ensureGLReady() || std::this_thread::get_id() != g_renderThread) return; float d[4] = {red, green, blue, 1.0f}; mcglLightfv(light == 1 ? 0x4001 : 0x4000, 0x1201, d); }
void C4JRender::StateSetLightAmbientColour(float red, float green, float blue) { if (!ensureGLReady() || std::this_thread::get_id() != g_renderThread) return; float a[4] = {red, green, blue, 1.0f}; mcglLightModelfv(0x0B53, a); }
void C4JRender::StateSetLightDirection(int light, float x, float y, float z) { if (!ensureGLReady() || std::this_thread::get_id() != g_renderThread) return; float p[4] = {x, y, z, 0.0f}; mcglLightfv(light == 1 ? 0x4001 : 0x4000, 0x1203, p); }
void C4JRender::StateSetLightEnable(int light, bool enable)
{
    if (!ensureGLReady() || std::this_thread::get_id() != g_renderThread) return;
    unsigned int l = (light == 1) ? 0x4001 : 0x4000;
    if (enable) mcglEnable(l); else mcglDisable(l);
}
void C4JRender::StateSetViewport(eViewportType viewportType)
{
    if (!ensureGLReady() || std::this_thread::get_id() != g_renderThread) return;
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
 *  C_4JInput  --  Input Manager stubs
 *
 * ********************************************************************/

void C_4JInput::Initialise(int /*iInputStateC*/, unsigned char /*ucMapC*/, unsigned char /*ucActionC*/, unsigned char /*ucMenuActionC*/) {}
void C_4JInput::Tick() {}
void C_4JInput::SetDeadzoneAndMovementRange(unsigned int /*uiDeadzone*/, unsigned int /*uiMovementRangeMax*/) {}

void         C_4JInput::SetGameJoypadMaps(unsigned char /*ucMap*/, unsigned char /*ucAction*/, unsigned int /*uiActionVal*/) {}
unsigned int C_4JInput::GetGameJoypadMaps(unsigned char /*ucMap*/, unsigned char /*ucAction*/) { return 0; }

void          C_4JInput::SetJoypadMapVal(int /*iPad*/, unsigned char /*ucMap*/) {}
unsigned char C_4JInput::GetJoypadMapVal(int /*iPad*/) { return 0; }

void C_4JInput::SetJoypadSensitivity(int /*iPad*/, float /*fSensitivity*/) {}

unsigned int C_4JInput::GetValue(int /*iPad*/, unsigned char /*ucAction*/, bool /*bRepeat*/) { return 0; }

bool C_4JInput::ButtonPressed(int /*iPad*/, unsigned char /*ucAction*/)  { return false; }
bool C_4JInput::ButtonReleased(int /*iPad*/, unsigned char /*ucAction*/) { return false; }
bool C_4JInput::ButtonDown(int /*iPad*/, unsigned char /*ucAction*/)     { return false; }

void C_4JInput::SetJoypadStickAxisMap(int /*iPad*/, unsigned int /*uiFrom*/, unsigned int /*uiTo*/) {}
void C_4JInput::SetJoypadStickTriggerMap(int /*iPad*/, unsigned int /*uiFrom*/, unsigned int /*uiTo*/) {}

void C_4JInput::SetKeyRepeatRate(float /*fRepeatDelaySecs*/, float /*fRepeatRateSecs*/) {}
void C_4JInput::SetDebugSequence(const char * /*chSequenceA*/, int(*/*Func*/)(LPVOID), LPVOID /*lpParam*/) {}

FLOAT C_4JInput::GetIdleSeconds(int /*iPad*/) { return 0.0f; }

bool C_4JInput::IsPadConnected(int iPad)
{
    // Pad 0 is always "connected" so the game has a primary controller
    return (iPad == 0);
}

float         C_4JInput::GetJoypadStick_LX(int /*iPad*/, bool /*bCheckMenuDisplay*/) { return 0.0f; }
float         C_4JInput::GetJoypadStick_LY(int /*iPad*/, bool /*bCheckMenuDisplay*/) { return 0.0f; }
float         C_4JInput::GetJoypadStick_RX(int /*iPad*/, bool /*bCheckMenuDisplay*/) { return 0.0f; }
float         C_4JInput::GetJoypadStick_RY(int /*iPad*/, bool /*bCheckMenuDisplay*/) { return 0.0f; }
unsigned char C_4JInput::GetJoypadLTrigger(int /*iPad*/, bool /*bCheckMenuDisplay*/) { return 0; }
unsigned char C_4JInput::GetJoypadRTrigger(int /*iPad*/, bool /*bCheckMenuDisplay*/) { return 0; }

void C_4JInput::SetMenuDisplayed(int /*iPad*/, bool /*bVal*/) {}

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
