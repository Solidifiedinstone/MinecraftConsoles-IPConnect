/*
 * gl_api.cpp — Minimal GL API stubs. Most rendering goes through Vulkan backend.
 * mcglClear is implemented to support mid-frame depth/color clears.
 */
#include "gl_api.h"
#include "vk_backend.h"

void mcglViewport(int, int, int, int) {}
void mcglEnable(unsigned int) {}
void mcglDisable(unsigned int) {}
void mcglDepthFunc(unsigned int) {}
void mcglClearColor(float, float, float, float) {}
void mcglMatrixMode(unsigned int) {}
void mcglLoadIdentity() {}
void mcglLoadMatrixf(const float*) {}
void mcglBegin(unsigned int) {}
void mcglEnd() {}
void mcglColor4ub(unsigned char, unsigned char, unsigned char, unsigned char) {}
void mcglColor4f(float, float, float, float) {}
void mcglTexCoord2f(float, float) {}
void mcglVertex3f(float, float, float) {}
void mcglTranslatef(float, float, float) {}
void mcglRotatef(float, float, float, float) {}
void mcglScalef(float, float, float) {}
void mcglMultMatrixf(const float*) {}
void mcglOrtho(double, double, double, double, double, double) {}
void mcglFrustum(double, double, double, double, double, double) {}
void mcglPushMatrix() {}
void mcglPopMatrix() {}
void mcglGetFloatv(unsigned int, float*) {}
void mcglGenTextures(int, unsigned int*) {}
void mcglDeleteTextures(int, const unsigned int*) {}
void mcglBindTexture(unsigned int, unsigned int) {}
void mcglTexImage2D(unsigned int, int, int, int, int, int, unsigned int, unsigned int, const void*) {}
void mcglTexSubImage2D(unsigned int, int, int, int, int, int, unsigned int, unsigned int, const void*) {}
void mcglTexParameteri(unsigned int, unsigned int, int) {}
void mcglClear(unsigned int mask)
{
    // Only clear inside an active render pass on the render thread
    if (!g_vk.inRenderPass || !g_vk.device) return;
    VkCommandBuffer cmd = g_vk.commandBuffers[g_vk.currentFrame];
    if (!cmd) return;

    VkClearAttachment clears[2] = {};
    uint32_t clearCount = 0;
    if (mask & 0x00004000) // GL_COLOR_BUFFER_BIT
    {
        clears[clearCount].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        clears[clearCount].clearValue.color = {{ g_vk.clearColor[0], g_vk.clearColor[1], g_vk.clearColor[2], g_vk.clearColor[3] }};
        clearCount++;
    }
    if (mask & 0x00000100) // GL_DEPTH_BUFFER_BIT
    {
        clears[clearCount].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        clears[clearCount].clearValue.depthStencil = { 1.0f, 0 };
        clearCount++;
    }
    if (clearCount == 0) return;

    VkClearRect rect = {};
    rect.rect = { {0, 0}, g_vk.swapchainExtent };
    rect.layerCount = 1;
    vkCmdClearAttachments(cmd, clearCount, clears, 1, &rect);
}
void mcglScissor(int, int, int, int) {}
void mcglDepthMask(unsigned char) {}
void mcglBlendFunc(unsigned int, unsigned int) {}
void mcglBlendColor(float, float, float, float) {}
void mcglAlphaFunc(unsigned int, float) {}
void mcglFrontFace(unsigned int) {}
void mcglLineWidth(float) {}
void mcglColorMask(unsigned char, unsigned char, unsigned char, unsigned char) {}
void mcglPolygonOffset(float, float) {}
void mcglFogi(unsigned int, int) {}
void mcglFogf(unsigned int, float) {}
void mcglFogfv(unsigned int, const float*) {}
void mcglLightfv(unsigned int, unsigned int, const float*) {}
void mcglLightModelfv(unsigned int, const float*) {}
void mcglShadeModel(unsigned int) {}
unsigned int mcglGetError() { return 0; }
void mcglTexEnvi(unsigned int, unsigned int, int) {}
void mcglGetIntegerv(unsigned int, int*) {}
