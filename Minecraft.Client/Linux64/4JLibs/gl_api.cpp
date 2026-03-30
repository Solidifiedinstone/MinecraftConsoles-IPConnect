/*
 * gl_api.cpp — Stubbed out. All rendering now goes through Vulkan backend.
 * These functions are kept as no-ops so existing mcgl* call sites compile.
 */
#include "gl_api.h"

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
void mcglClear(unsigned int) {}
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
