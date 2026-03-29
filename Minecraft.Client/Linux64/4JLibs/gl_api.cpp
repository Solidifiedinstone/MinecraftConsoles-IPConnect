#include <GL/gl.h>
#include "gl_api.h"

void mcglViewport(int x, int y, int w, int h) { glViewport(x, y, w, h); }
void mcglEnable(unsigned int cap) { glEnable(cap); }
void mcglDisable(unsigned int cap) { glDisable(cap); }
void mcglDepthFunc(unsigned int func) { glDepthFunc(func); }
void mcglClearColor(float r, float g, float b, float a) { glClearColor(r, g, b, a); }
void mcglMatrixMode(unsigned int mode) { glMatrixMode(mode); }
void mcglLoadIdentity() { glLoadIdentity(); }
void mcglLoadMatrixf(const float *m) { glLoadMatrixf(m); }
void mcglBegin(unsigned int mode) { glBegin(mode); }
void mcglEnd() { glEnd(); }
void mcglColor4ub(unsigned char r, unsigned char g, unsigned char b, unsigned char a) { glColor4ub(r, g, b, a); }
void mcglColor4f(float r, float g, float b, float a) { glColor4f(r, g, b, a); }
void mcglTexCoord2f(float u, float v) { glTexCoord2f(u, v); }
void mcglVertex3f(float x, float y, float z) { glVertex3f(x, y, z); }
void mcglTranslatef(float x, float y, float z) { glTranslatef(x, y, z); }
void mcglRotatef(float angle, float x, float y, float z) { glRotatef(angle, x, y, z); }
void mcglScalef(float x, float y, float z) { glScalef(x, y, z); }
void mcglMultMatrixf(const float *m) { glMultMatrixf(m); }
void mcglOrtho(double left, double right, double bottom, double top, double zNear, double zFar) { glOrtho(left, right, bottom, top, zNear, zFar); }
void mcglFrustum(double left, double right, double bottom, double top, double zNear, double zFar) { glFrustum(left, right, bottom, top, zNear, zFar); }
void mcglPushMatrix() { glPushMatrix(); }
void mcglPopMatrix() { glPopMatrix(); }
void mcglGetFloatv(unsigned int pname, float *params) { glGetFloatv(pname, params); }
void mcglGenTextures(int n, unsigned int *textures) { glGenTextures(n, textures); }
void mcglDeleteTextures(int n, const unsigned int *textures) { glDeleteTextures(n, textures); }
void mcglBindTexture(unsigned int target, unsigned int texture) { glBindTexture(target, texture); }
void mcglTexImage2D(unsigned int target, int level, int internalformat, int width, int height, int border, unsigned int format, unsigned int type, const void *data) { glTexImage2D(target, level, internalformat, width, height, border, format, type, data); }
void mcglTexSubImage2D(unsigned int target, int level, int xoffset, int yoffset, int width, int height, unsigned int format, unsigned int type, const void *data) { glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, data); }
void mcglTexParameteri(unsigned int target, unsigned int pname, int value) { glTexParameteri(target, pname, value); }
void mcglClear(unsigned int mask) { glClear(mask); }
void mcglScissor(int x, int y, int w, int h) { glScissor(x, y, w, h); }
void mcglDepthMask(unsigned char flag) { glDepthMask(flag); }
void mcglBlendFunc(unsigned int src, unsigned int dst) { glBlendFunc(src, dst); }
void mcglBlendColor(float r, float g, float b, float a) { glBlendColor(r, g, b, a); }
void mcglAlphaFunc(unsigned int func, float ref) { glAlphaFunc(func, ref); }
void mcglFrontFace(unsigned int mode) { glFrontFace(mode); }
void mcglLineWidth(float width) { glLineWidth(width); }
void mcglColorMask(unsigned char r, unsigned char g, unsigned char b, unsigned char a) { glColorMask(r, g, b, a); }
void mcglPolygonOffset(float factor, float units) { glPolygonOffset(factor, units); }
void mcglFogi(unsigned int pname, int value) { glFogi(pname, value); }
void mcglFogf(unsigned int pname, float value) { glFogf(pname, value); }
void mcglFogfv(unsigned int pname, const float *params) { glFogfv(pname, params); }
void mcglLightfv(unsigned int light, unsigned int pname, const float *params) { glLightfv(light, pname, params); }
void mcglLightModelfv(unsigned int pname, const float *params) { glLightModelfv(pname, params); }
