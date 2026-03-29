#pragma once
/* Iggy UI library stubs for Linux - comprehensive type stubs */

#include <cstddef>

typedef void* HIGGYEXP;
typedef void* HIGGYUI;
typedef void* HIGGYSCENE;
typedef void* HIGGYPERFMON;
typedef void* IggyValuePath;
typedef void* IggyPlayerRootPath;
typedef unsigned short IggyUTF16;

typedef int IggyCustomDrawCallbackRegion;
typedef int IggyName;

struct GDrawFunctions {};
struct GDrawTexture { void* handle; int width; int height; };
/* CustomDrawData is defined in Common/UI/UIStructs.h - do not redefine */
struct GDrawVertexPosition2D { float x, y; };
struct IggyLibrary {};

/* Iggy class - used directly as a type in the codebase */
class Iggy {
public:
    typedef void* Value;
    typedef void* ObjectInterface;
};

/* RAD Game Tools types used by Iggy */
typedef int rrbool;
#define RADLINK
#define RADEXPFUNC

typedef Iggy* IggyPlayer;

struct IggyExternalFunctionCallUTF16 {
    const IggyUTF16* functionName;
    int argCount;
};

struct IggyDataBindingUTF16 {
    const IggyUTF16* name;
};

/* Stub functions - all no-ops */
static inline void IggyLibrarySetAllocator(void*, void*, void*) {}
static inline HIGGYEXP IggyExpressionCreate() { return NULL; }
static inline void IggyExpressionDestroy(HIGGYEXP) {}
static inline int IggyLibraryInitialize(void) { return 0; }
static inline void IggyLibraryTerminate(void) {}
static inline HIGGYUI IggyPlayerCreate(void*, int, int) { return NULL; }
static inline void IggyPlayerDestroy(HIGGYUI) {}
static inline void IggyPlayerTick(HIGGYUI, float) {}
static inline void IggyPlayerDraw(HIGGYUI) {}

/* Iggy font/bitmap types */
struct IggyBitmapCharacter { int width; int height; };
struct IggyGlyphMetrics { int advance; int width; int height; };
struct IggyFontMetrics { int ascent; int descent; int lineHeight; };
struct IggyBitmapFontProvider {};
struct IggyMemoryUseInfo { int totalBytes; int peakBytes; };
