#pragma once
#ifndef _MC_LINUX_TYPES_H
#define _MC_LINUX_TYPES_H

/*
 * LinuxTypes.h
 *
 * Comprehensive Windows-to-Linux type compatibility header for porting
 * a Windows C++ game codebase to Linux. This file must be included very
 * early and is entirely self-contained -- no project file dependencies.
 *
 * NOTE: Do NOT define `byte` here. It is already typedef'd as
 *       `unsigned char` in extraX64.h.
 */

/* --------------------------------------------------------------------
 * Standard system headers
 * -------------------------------------------------------------------- */
#include <stdint.h>
#include <stddef.h>
#include <climits>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <wchar.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <cstdio>
#include <cstdlib>
#include <cfloat>
/* Include sys/stat.h early, before our type definitions conflict with kernel headers */
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

/* ====================================================================
 *  Fundamental Windows scalar typedefs
 * ==================================================================== */

typedef uint8_t     BYTE;
typedef uint16_t    WORD;
typedef uint32_t    DWORD;
typedef int32_t     BOOL;
typedef int32_t     INT;
typedef uint32_t    UINT;
typedef int32_t     LONG;
typedef int64_t     LONGLONG;
typedef uint64_t    ULONGLONG;
typedef uint64_t    DWORD64;
typedef int64_t     LONG_PTR;
typedef uint64_t    ULONG_PTR;
typedef uint32_t    ULONG;
typedef int16_t     SHORT;
typedef uint16_t    USHORT;
typedef float       FLOAT;
typedef void        VOID;
typedef char        CHAR;
typedef wchar_t     WCHAR;
typedef size_t      SIZE_T;

/* Pointer-width types */
typedef void*       PVOID;
typedef BYTE*       PBYTE;
typedef DWORD*      PDWORD;
typedef BOOL*       PBOOL;
typedef LONG*       PLONG;
typedef DWORD*      LPDWORD;
typedef void*       LPVOID;
typedef const void* LPCVOID;
typedef char*       LPSTR;
typedef const char* LPCSTR;
typedef wchar_t*    LPWSTR;
typedef const wchar_t* LPCWSTR;

/* Handle types */
typedef void*       HANDLE;
typedef void*       HWND;
typedef void*       HINSTANCE;
typedef void*       HMODULE;
typedef void*       HDC;
typedef void*       HGLRC;
typedef void*       HICON;
typedef void*       HCURSOR;
typedef void*       HBRUSH;
typedef void*       HMENU;
typedef void*       HFONT;
typedef void*       HBITMAP;
typedef void*       HGLOBAL;

/* HRESULT is a 32-bit signed value on Windows */
typedef int32_t     HRESULT;

/* Message-callback types */
typedef LONG_PTR    LRESULT;
typedef ULONG_PTR   WPARAM;
typedef LONG_PTR    LPARAM;

/* Windows CONST qualifier */
#ifndef CONST
#define CONST const
#endif

/* ====================================================================
 *  Boolean constants
 * ==================================================================== */
#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* ====================================================================
 *  HRESULT helpers
 * ==================================================================== */
#ifndef S_OK
#define S_OK       ((HRESULT)0x00000000L)
#endif
#ifndef S_FALSE
#define S_FALSE    ((HRESULT)0x00000001L)
#endif
#ifndef E_FAIL
#define E_FAIL     ((HRESULT)0x80004005L)
#endif
#ifndef E_OUTOFMEMORY
#define E_OUTOFMEMORY ((HRESULT)0x8007000EL)
#endif
#ifndef E_INVALIDARG
#define E_INVALIDARG  ((HRESULT)0x80070057L)
#endif
#ifndef E_NOTIMPL
#define E_NOTIMPL     ((HRESULT)0x80004001L)
#endif
#ifndef E_ABORT
#define E_ABORT       ((HRESULT)0x80004004L)
#endif

#ifndef SUCCEEDED
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#endif
#ifndef FAILED
#define FAILED(hr)    (((HRESULT)(hr)) < 0)
#endif
#ifndef HRESULT_SUCCEEDED
#define HRESULT_SUCCEEDED(hr) SUCCEEDED(hr)
#endif

/* ====================================================================
 *  Miscellaneous Windows macros
 * ==================================================================== */
#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#ifndef CALLBACK
#define CALLBACK
#endif
#ifndef WINAPI
#define WINAPI
#endif
#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef STDMETHODCALLTYPE
#define STDMETHODCALLTYPE
#endif

#ifndef MAKEWORD
#define MAKEWORD(low, high) \
    ((WORD)(((BYTE)(((DWORD)(low)) & 0xFF)) | \
            ((WORD)((BYTE)(((DWORD)(high)) & 0xFF))) << 8))
#endif

#ifndef MAKELONG
#define MAKELONG(low, high) \
    ((LONG)(((WORD)(((DWORD)(low)) & 0xFFFF)) | \
            ((DWORD)((WORD)(((DWORD)(high)) & 0xFFFF))) << 16))
#endif

#ifndef LOWORD
#define LOWORD(l) ((WORD)(((DWORD)(l)) & 0xFFFF))
#endif
#ifndef HIWORD
#define HIWORD(l) ((WORD)((((DWORD)(l)) >> 16) & 0xFFFF))
#endif
#ifndef LOBYTE
#define LOBYTE(w) ((BYTE)(((DWORD)(w)) & 0xFF))
#endif
#ifndef HIBYTE
#define HIBYTE(w) ((BYTE)((((DWORD)(w)) >> 8) & 0xFF))
#endif

/* INVALID_HANDLE_VALUE */
#ifndef INVALID_HANDLE_VALUE
#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)
#endif

/* ====================================================================
 *  Socket compatibility
 * ==================================================================== */
#ifndef SOCKET
typedef int SOCKET;
#endif

#ifndef INVALID_SOCKET
#define INVALID_SOCKET (-1)
#endif

#ifndef SOCKET_ERROR
#define SOCKET_ERROR   (-1)
#endif

/* closesocket -> close */
#ifndef closesocket
#define closesocket(s) close(s)
#endif

/* WSA stubs */
typedef struct WSAData {
    WORD wVersion;
    WORD wHighVersion;
    char szDescription[257];
    char szSystemStatus[129];
    unsigned short iMaxSockets;
    unsigned short iMaxUdpDg;
    char* lpVendorInfo;
} WSADATA, *LPWSADATA;

static inline int WSAStartup(WORD /*wVersionRequested*/, LPWSADATA /*lpWSAData*/)
{
    return 0; /* no-op on Linux */
}

static inline int WSACleanup(void)
{
    return 0; /* no-op on Linux */
}

static inline int WSAGetLastError(void)
{
    return errno;
}

/* ====================================================================
 *  FILETIME / SYSTEMTIME
 * ==================================================================== */
#ifndef _FILETIME_
#define _FILETIME_
typedef struct _FILETIME {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
} FILETIME, *PFILETIME, *LPFILETIME;
#endif

#ifndef _SYSTEMTIME_
#define _SYSTEMTIME_
typedef struct _SYSTEMTIME {
    WORD wYear;
    WORD wMonth;
    WORD wDayOfWeek;
    WORD wDay;
    WORD wHour;
    WORD wMinute;
    WORD wSecond;
    WORD wMilliseconds;
} SYSTEMTIME, *PSYSTEMTIME, *LPSYSTEMTIME;
#endif

/* ====================================================================
 *  LARGE_INTEGER / ULARGE_INTEGER
 * ==================================================================== */
#ifndef _LARGE_INTEGER_
#define _LARGE_INTEGER_
typedef union _LARGE_INTEGER {
    struct { DWORD LowPart; LONG  HighPart; };
    struct { DWORD LowPart; LONG  HighPart; } u;
    LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

typedef union _ULARGE_INTEGER {
    struct { DWORD LowPart; DWORD HighPart; };
    struct { DWORD LowPart; DWORD HighPart; } u;
    ULONGLONG QuadPart;
} ULARGE_INTEGER, *PULARGE_INTEGER;
#endif

/* ====================================================================
 *  CRITICAL_SECTION  (wraps pthread_mutex_t)
 * ==================================================================== */
#ifndef _CRITICAL_SECTION_DEFINED
#define _CRITICAL_SECTION_DEFINED

typedef struct _CRITICAL_SECTION {
    pthread_mutex_t mutex;
} CRITICAL_SECTION, *PCRITICAL_SECTION, *LPCRITICAL_SECTION;

static inline void InitializeCriticalSection(CRITICAL_SECTION* cs)
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&cs->mutex, &attr);
    pthread_mutexattr_destroy(&attr);
}

static inline BOOL InitializeCriticalSectionAndSpinCount(CRITICAL_SECTION* cs, DWORD /*dwSpinCount*/)
{
    /* Linux pthreads do not expose spin counts; just initialise normally. */
    InitializeCriticalSection(cs);
    return TRUE;
}

static inline void EnterCriticalSection(CRITICAL_SECTION* cs)
{
    pthread_mutex_lock(&cs->mutex);
}

static inline BOOL TryEnterCriticalSection(CRITICAL_SECTION* cs)
{
    return (pthread_mutex_trylock(&cs->mutex) == 0) ? TRUE : FALSE;
}

static inline void LeaveCriticalSection(CRITICAL_SECTION* cs)
{
    pthread_mutex_unlock(&cs->mutex);
}

static inline void DeleteCriticalSection(CRITICAL_SECTION* cs)
{
    pthread_mutex_destroy(&cs->mutex);
}

#endif /* _CRITICAL_SECTION_DEFINED */

/* ====================================================================
 *  Thread helpers
 * ==================================================================== */

/*
 * CreateThread_Linux  --  rough equivalent of Win32 CreateThread().
 *
 * Returns a HANDLE which is really a heap-allocated pthread_t*.
 * The caller can join on it or just let it run.  Pass NULL for the
 * handle if you do not need it.
 *
 * lpStartAddress signature on Windows is  DWORD WINAPI func(LPVOID).
 * We accept the same signature here and throw away the return value
 * inside a small thunk.
 */
typedef DWORD (WINAPI *LPTHREAD_START_ROUTINE)(LPVOID);

/* File/thread/event wrappers so HANDLE ownership is type-safe. */
#define _LINUX_FILE_HANDLE_MAGIC   0x46494C45u  /* 'FILE' */
#define _LINUX_THREAD_HANDLE_MAGIC 0x54485244u  /* 'THRD' */
#define _LINUX_EVENT_HANDLE_MAGIC  0x45564E54u  /* 'EVNT' */
struct _LinuxFileHandle {
    uint32_t magic;
    FILE*    fp;
};
struct _LinuxThreadHandle {
    uint32_t magic;
    pthread_t tid;
    BOOL joined;
};
struct _LinuxEventHandle {
    uint32_t magic;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    BOOL manualReset;
    BOOL signaled;
};

struct _LinuxThreadData {
    LPTHREAD_START_ROUTINE func;
    LPVOID param;
};

static inline void* _LinuxThreadThunk(void* arg)
{
    struct _LinuxThreadData* td = (struct _LinuxThreadData*)arg;
    LPTHREAD_START_ROUTINE fn = td->func;
    LPVOID p = td->param;
    free(td);
    fn(p);
    return NULL;
}

static inline HANDLE CreateThread_Linux(
    void*                   /*lpThreadAttributes*/,
    SIZE_T                  /*dwStackSize*/,
    LPTHREAD_START_ROUTINE  lpStartAddress,
    LPVOID                  lpParameter,
    DWORD                   /*dwCreationFlags*/,
    DWORD*                  lpThreadId)
{
    _LinuxThreadHandle* th = (_LinuxThreadHandle*)malloc(sizeof(_LinuxThreadHandle));
    if (!th) return NULL;

    struct _LinuxThreadData* td =
        (struct _LinuxThreadData*)malloc(sizeof(struct _LinuxThreadData));
    if (!td) { free(th); return NULL; }

    td->func  = lpStartAddress;
    td->param = lpParameter;

    if (pthread_create(&th->tid, NULL, _LinuxThreadThunk, td) != 0) {
        free(td);
        free(th);
        return NULL;
    }
    th->magic = _LINUX_THREAD_HANDLE_MAGIC;
    th->joined = FALSE;

    if (lpThreadId) {
        *lpThreadId = (DWORD)(uintptr_t)th->tid;
    }

    return (HANDLE)th;
}

/* Convenience: map CreateThread to our Linux variant. */
#ifndef CreateThread
#define CreateThread CreateThread_Linux
#endif

/* CloseHandle: handles file, thread, and event HANDLEs. */
static inline BOOL CloseHandle_Linux(HANDLE h)
{
    if (!h || h == INVALID_HANDLE_VALUE)
        return TRUE;
    _LinuxFileHandle* fh = (_LinuxFileHandle*)h;
    if (fh->magic == _LINUX_FILE_HANDLE_MAGIC) {
        fclose(fh->fp);
        fh->magic = 0;
        free(fh);
        return TRUE;
    }
    _LinuxThreadHandle* th = (_LinuxThreadHandle*)h;
    if (th->magic == _LINUX_THREAD_HANDLE_MAGIC) {
        if (!th->joined) pthread_detach(th->tid);
        th->magic = 0;
        free(th);
        return TRUE;
    }
    _LinuxEventHandle* ev = (_LinuxEventHandle*)h;
    if (ev->magic == _LINUX_EVENT_HANDLE_MAGIC) {
        pthread_mutex_destroy(&ev->mutex);
        pthread_cond_destroy(&ev->cond);
        ev->magic = 0;
        free(ev);
        return TRUE;
    }
    return FALSE;
}

#ifndef CloseHandle
#define CloseHandle CloseHandle_Linux
#endif

/* WaitForSingleObject -- simplified: only handles thread HANDLEs. */
#ifndef WAIT_OBJECT_0
#define WAIT_OBJECT_0  0x00000000L
#endif
#ifndef WAIT_TIMEOUT
#define WAIT_TIMEOUT   0x00000102L
#endif
#ifndef WAIT_ABANDONED
#define WAIT_ABANDONED 0x00000080L
#endif
#ifndef WAIT_FAILED
#define WAIT_FAILED    0xFFFFFFFF
#endif
#ifndef INFINITE
#define INFINITE       0xFFFFFFFF
#endif

static inline DWORD WaitForSingleObject_Linux(HANDLE h, DWORD dwMilliseconds)
{
    if (!h || h == INVALID_HANDLE_VALUE) return WAIT_FAILED;

    _LinuxThreadHandle* th = (_LinuxThreadHandle*)h;
    if (th->magic == _LINUX_THREAD_HANDLE_MAGIC) {
        if (!th->joined) {
            pthread_join(th->tid, NULL);
            th->joined = TRUE;
        }
        return WAIT_OBJECT_0;
    }

    _LinuxEventHandle* ev = (_LinuxEventHandle*)h;
    if (ev->magic == _LINUX_EVENT_HANDLE_MAGIC) {
        pthread_mutex_lock(&ev->mutex);
        if (!ev->signaled) {
            if (dwMilliseconds == 0) {
                pthread_mutex_unlock(&ev->mutex);
                return WAIT_TIMEOUT;
            }
            if (dwMilliseconds == INFINITE) {
                while (!ev->signaled) pthread_cond_wait(&ev->cond, &ev->mutex);
            } else {
                struct timespec ts;
                clock_gettime(CLOCK_REALTIME, &ts);
                ts.tv_sec += dwMilliseconds / 1000;
                ts.tv_nsec += (long)(dwMilliseconds % 1000) * 1000000L;
                if (ts.tv_nsec >= 1000000000L) {
                    ts.tv_sec += 1;
                    ts.tv_nsec -= 1000000000L;
                }
                while (!ev->signaled) {
                    int rc = pthread_cond_timedwait(&ev->cond, &ev->mutex, &ts);
                    if (rc == ETIMEDOUT) {
                        pthread_mutex_unlock(&ev->mutex);
                        return WAIT_TIMEOUT;
                    }
                }
            }
        }
        if (!ev->manualReset) ev->signaled = FALSE;
        pthread_mutex_unlock(&ev->mutex);
        return WAIT_OBJECT_0;
    }

    return WAIT_FAILED;
}

#ifndef WaitForSingleObject
#define WaitForSingleObject WaitForSingleObject_Linux
#endif

/* ====================================================================
 *  Sleep, GetTickCount, GetCurrentThreadId
 * ==================================================================== */

static inline void Sleep(DWORD dwMilliseconds)
{
    usleep((useconds_t)dwMilliseconds * 1000u);
}

static inline DWORD GetTickCount(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (DWORD)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
}

static inline ULONGLONG GetTickCount64(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ULONGLONG)ts.tv_sec * 1000ULL + (ULONGLONG)ts.tv_nsec / 1000000ULL;
}

static inline DWORD GetCurrentThreadId(void)
{
    return (DWORD)(uintptr_t)pthread_self();
}

/* ====================================================================
 *  Thread Local Storage (TLS) — map Windows TLS to pthread_key_t
 * ==================================================================== */
#define TLS_OUT_OF_INDEXES ((DWORD)0xFFFFFFFF)

static inline DWORD TlsAlloc(void)
{
    pthread_key_t key;
    if (pthread_key_create(&key, NULL) != 0)
        return TLS_OUT_OF_INDEXES;
    return (DWORD)key;
}

static inline BOOL TlsSetValue(DWORD dwTlsIndex, LPVOID lpTlsValue)
{
    return pthread_setspecific((pthread_key_t)dwTlsIndex, lpTlsValue) == 0;
}

static inline LPVOID TlsGetValue(DWORD dwTlsIndex)
{
    return pthread_getspecific((pthread_key_t)dwTlsIndex);
}

static inline BOOL TlsFree(DWORD dwTlsIndex)
{
    return pthread_key_delete((pthread_key_t)dwTlsIndex) == 0;
}

/* ====================================================================
 *  MSVC intrinsics / types
 * ==================================================================== */
#ifndef __debugbreak
#define __debugbreak() __builtin_trap()
#endif

typedef int32_t __int32;
typedef int64_t __int64;

/* _byteswap for endian conversion */
#ifndef _byteswap_ushort
#define _byteswap_ushort(x) __builtin_bswap16(x)
#endif
#ifndef _byteswap_ulong
#define _byteswap_ulong(x) __builtin_bswap32(x)
#endif
#ifndef _byteswap_uint64
#define _byteswap_uint64(x) __builtin_bswap64(x)
#endif

/* fopen_s, sscanf_s stubs */
#ifndef fopen_s
static inline int fopen_s(FILE** pFile, const char* filename, const char* mode) {
    *pFile = fopen(filename, mode);
    return (*pFile == NULL) ? errno : 0;
}
#endif

#ifndef freopen_s
static inline int freopen_s(FILE** pFile, const char* filename, const char* mode, FILE* stream) {
    *pFile = freopen(filename, mode, stream);
    return (*pFile == NULL) ? errno : 0;
}
#endif

/* ====================================================================
 *  GetLocalTime
 * ==================================================================== */
static inline void GetLocalTime(SYSTEMTIME* st)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm tm_buf;
    localtime_r(&tv.tv_sec, &tm_buf);
    st->wYear         = (WORD)(tm_buf.tm_year + 1900);
    st->wMonth        = (WORD)(tm_buf.tm_mon + 1);
    st->wDayOfWeek    = (WORD)tm_buf.tm_wday;
    st->wDay          = (WORD)tm_buf.tm_mday;
    st->wHour         = (WORD)tm_buf.tm_hour;
    st->wMinute       = (WORD)tm_buf.tm_min;
    st->wSecond       = (WORD)tm_buf.tm_sec;
    st->wMilliseconds = (WORD)(tv.tv_usec / 1000);
}

/* ====================================================================
 *  OutputDebugStringA
 * ==================================================================== */
#ifndef OutputDebugStringA
#define OutputDebugStringA(s) fprintf(stderr, "%s", (s))
#endif
#ifndef OutputDebugStringW
#define OutputDebugStringW(s) fwprintf(stderr, L"%ls", (s))
#endif
#ifndef OutputDebugString
#define OutputDebugString OutputDebugStringA
#endif

/* ====================================================================
 *  ZeroMemory / CopyMemory / FillMemory / MoveMemory
 * ==================================================================== */
#ifndef ZeroMemory
#define ZeroMemory(dest, len) memset((dest), 0, (len))
#endif
#ifndef CopyMemory
#define CopyMemory(dest, src, len) memcpy((dest), (src), (len))
#endif
#ifndef FillMemory
#define FillMemory(dest, len, val) memset((dest), (val), (len))
#endif
#ifndef MoveMemory
#define MoveMemory(dest, src, len) memmove((dest), (src), (len))
#endif

/* ====================================================================
 *  min / max — use templates instead of macros to avoid breaking
 *  std::min / std::max (which have 3-arg overloads).
 * ==================================================================== */
#include <algorithm>
using std::min;
using std::max;

/* ====================================================================
 *  _countof
 * ==================================================================== */
#ifndef _countof
#define _countof(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

/* ====================================================================
 *  sprintf_s / _snprintf / _stricmp / _wcsicmp and friends
 * ==================================================================== */
#ifndef sprintf_s
#define sprintf_s  snprintf
#endif
#ifndef _snprintf
#define _snprintf  snprintf
#endif
#ifndef _snwprintf
#define _snwprintf swprintf
#endif
#ifndef swprintf_s
#define swprintf_s swprintf
#endif
#ifndef _stricmp
#define _stricmp   strcasecmp
#endif
#ifndef _strnicmp
#define _strnicmp  strncasecmp
#endif
#ifndef _wcsicmp
#define _wcsicmp   wcscasecmp
#endif
#ifndef _wcsnicmp
#define _wcsnicmp  wcsncasecmp
#endif
#ifndef stricmp
#define stricmp    strcasecmp
#endif
#ifndef strnicmp
#define strnicmp   strncasecmp
#endif
#ifndef _vsnprintf
#define _vsnprintf vsnprintf
#endif
#ifndef vsprintf_s
#define vsprintf_s(buf, sz, fmt, ap) vsnprintf((buf), (sz), (fmt), (ap))
#endif
#ifndef wcscpy_s
#define wcscpy_s(dst, sz, src) wcsncpy((dst), (src), (sz))
#endif
#ifndef strcpy_s
#define strcpy_s(dst, sz, src) strncpy((dst), (src), (sz))
#endif
#ifndef strcat_s
#define strcat_s(dst, sz, src) strncat((dst), (src), (sz) - strlen(dst) - 1)
#endif
#ifndef wcscat_s
#define wcscat_s(dst, sz, src) wcsncat((dst), (src), (sz) - wcslen(dst) - 1)
#endif

/* ====================================================================
 *  D3D11 blend constants
 * ==================================================================== */
#ifndef D3D11_BLEND_ZERO
#define D3D11_BLEND_ZERO               1
#endif
#ifndef D3D11_BLEND_ONE
#define D3D11_BLEND_ONE                2
#endif
#ifndef D3D11_BLEND_SRC_COLOR
#define D3D11_BLEND_SRC_COLOR          3
#endif
#ifndef D3D11_BLEND_INV_SRC_COLOR
#define D3D11_BLEND_INV_SRC_COLOR      4
#endif
#ifndef D3D11_BLEND_SRC_ALPHA
#define D3D11_BLEND_SRC_ALPHA          5
#endif
#ifndef D3D11_BLEND_INV_SRC_ALPHA
#define D3D11_BLEND_INV_SRC_ALPHA      6
#endif
#ifndef D3D11_BLEND_DEST_ALPHA
#define D3D11_BLEND_DEST_ALPHA         7
#endif
#ifndef D3D11_BLEND_DEST_COLOR
#define D3D11_BLEND_DEST_COLOR         9
#endif
#ifndef D3D11_BLEND_INV_DEST_COLOR
#define D3D11_BLEND_INV_DEST_COLOR    10
#endif
#ifndef D3D11_BLEND_BLEND_FACTOR
#define D3D11_BLEND_BLEND_FACTOR      14
#endif
#ifndef D3D11_BLEND_INV_BLEND_FACTOR
#define D3D11_BLEND_INV_BLEND_FACTOR  15
#endif

/* ====================================================================
 *  D3D11 comparison function constants
 * ==================================================================== */
#ifndef D3D11_COMPARISON_EQUAL
#define D3D11_COMPARISON_EQUAL          3
#endif
#ifndef D3D11_COMPARISON_LESS_EQUAL
#define D3D11_COMPARISON_LESS_EQUAL     4
#endif
#ifndef D3D11_COMPARISON_GREATER
#define D3D11_COMPARISON_GREATER        5
#endif
#ifndef D3D11_COMPARISON_GREATER_EQUAL
#define D3D11_COMPARISON_GREATER_EQUAL  6
#endif
#ifndef D3D11_COMPARISON_ALWAYS
#define D3D11_COMPARISON_ALWAYS         8
#endif

/* ====================================================================
 *  D3D11_RECT
 * ==================================================================== */
#ifndef _D3D11_RECT_DEFINED
#define _D3D11_RECT_DEFINED
typedef struct _D3D11_RECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} D3D11_RECT;
#endif

/* Windows RECT (identical layout) */
#ifndef _WINDEF_RECT_DEFINED
#define _WINDEF_RECT_DEFINED
typedef struct tagRECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} RECT, *PRECT, *LPRECT;
#endif

/* POINT */
#ifndef _WINDEF_POINT_DEFINED
#define _WINDEF_POINT_DEFINED
typedef struct tagPOINT {
    LONG x;
    LONG y;
} POINT, *PPOINT, *LPPOINT;
#endif

/* SIZE */
#ifndef _WINDEF_SIZE_DEFINED
#define _WINDEF_SIZE_DEFINED
typedef struct tagSIZE {
    LONG cx;
    LONG cy;
} SIZE, *PSIZE, *LPSIZE;
#endif

/* ====================================================================
 *  D3D11 stub forward declarations
 *
 *  These are empty classes so that pointer types (ID3D11Device*, etc.)
 *  compile without pulling in real DirectX headers.
 * ==================================================================== */
class ID3D11Device {};
class IDXGISwapChain {};
class ID3D11DeviceContext {};
class ID3D11RenderTargetView {};
class ID3D11DepthStencilView {};
class ID3D11Buffer {};
class ID3D11ShaderResourceView {};
class ID3D11Texture2D {};
class ID3D11VertexShader {};
class ID3D11PixelShader {};
class ID3D11InputLayout {};
class ID3D11BlendState {};
class ID3D11DepthStencilState {};
class ID3D11RasterizerState {};
class ID3D11SamplerState {};
class IDXGIAdapter {};
class IDXGIFactory {};
class IDXGIOutput {};

/* ====================================================================
 *  GUID stub (used by COM-like interfaces)
 * ==================================================================== */
#ifndef _GUID_DEFINED
#define _GUID_DEFINED
typedef struct _GUID {
    DWORD Data1;
    WORD  Data2;
    WORD  Data3;
    BYTE  Data4[8];
} GUID, IID, CLSID;
#define REFIID    const IID&
#define REFCLSID  const CLSID&
#define REFGUID   const GUID&
#endif

/* IUnknown stub */
#ifndef __IUnknown_INTERFACE_DEFINED__
#define __IUnknown_INTERFACE_DEFINED__
class IUnknown {
public:
    virtual HRESULT QueryInterface(REFIID riid, void** ppvObject) = 0;
    virtual ULONG   AddRef(void)  = 0;
    virtual ULONG   Release(void) = 0;
    virtual ~IUnknown() {}
};
#endif

/* ====================================================================
 *  Interlocked operations
 * ==================================================================== */
static inline LONG InterlockedIncrement(volatile LONG* pVal)
{
    return __sync_add_and_fetch(pVal, 1);
}

static inline LONG InterlockedDecrement(volatile LONG* pVal)
{
    return __sync_sub_and_fetch(pVal, 1);
}

static inline LONG InterlockedExchange(volatile LONG* pTarget, LONG value)
{
    return __sync_lock_test_and_set(pTarget, value);
}

static inline LONG InterlockedCompareExchange(volatile LONG* pDest, LONG exchange, LONG comparand)
{
    return __sync_val_compare_and_swap(pDest, comparand, exchange);
}

/* ====================================================================
 *  QueryPerformanceCounter / QueryPerformanceFrequency
 * ==================================================================== */
static inline BOOL QueryPerformanceCounter(LARGE_INTEGER* lpCount)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    lpCount->QuadPart = (LONGLONG)ts.tv_sec * 1000000000LL + (LONGLONG)ts.tv_nsec;
    return TRUE;
}

static inline BOOL QueryPerformanceFrequency(LARGE_INTEGER* lpFreq)
{
    lpFreq->QuadPart = 1000000000LL; /* nanoseconds */
    return TRUE;
}

/* ====================================================================
 *  timeGetTime (multimedia timer)
 * ==================================================================== */
static inline DWORD timeGetTime(void)
{
    return GetTickCount();
}

/* ====================================================================
 *  UNREFERENCED_PARAMETER
 * ==================================================================== */
#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(p) (void)(p)
#endif

/* ====================================================================
 *  TEXT / _T macros (ANSI -- Linux has no native wchar concept)
 * ==================================================================== */
#ifndef TEXT
#define TEXT(x) x
#endif
#ifndef _T
#define _T(x) x
#endif
#ifndef _TEXT
#define _TEXT(x) x
#endif

/* ====================================================================
 *  SAFE_RELEASE (COM pattern)
 * ==================================================================== */
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) do { if (p) { (p)->Release(); (p) = nullptr; } } while(0)
#endif

/* ====================================================================
 *  Virtual Memory API stubs
 * ==================================================================== */
#define MEM_RESERVE 0x2000
#define MEM_COMMIT 0x1000
#define MEM_DECOMMIT 0x4000
#define MEM_RELEASE 0x8000
#define MEM_LARGE_PAGES 0x20000000
#define PAGE_READWRITE 0x04
#define MAXULONG_PTR ((ULONG_PTR)~((ULONG_PTR)0))

static inline LPVOID VirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect) {
    (void)flAllocationType; (void)flProtect;
    if (lpAddress) return lpAddress; // MEM_COMMIT on existing
    void* p = malloc(dwSize);
    if (p) memset(p, 0, dwSize);
    return p;
}
static inline BOOL VirtualFree(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType) {
    (void)dwSize; (void)dwFreeType;
    free(lpAddress);
    return TRUE;
}

/* ====================================================================
 *  File API stubs
 * ==================================================================== */
#define GENERIC_READ 0x80000000
#define GENERIC_WRITE 0x40000000
#define OPEN_EXISTING 3
#define OPEN_ALWAYS 4
#define CREATE_ALWAYS 2
#define FILE_ATTRIBUTE_NORMAL 0x80
#define FILE_FLAG_RANDOM_ACCESS 0x10000000
#define FILE_BEGIN 0
#define FILE_CURRENT 1
#define FILE_END 2

static inline FILE* _lfh_fp(HANDLE h) { return ((_LinuxFileHandle*)h)->fp; }

static inline HANDLE CreateFile(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
    void* lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    (void)dwShareMode; (void)lpSecurityAttributes; (void)dwFlagsAndAttributes; (void)hTemplateFile;
    const char* mode = "rb";
    if (dwDesiredAccess & GENERIC_WRITE) {
        if (dwCreationDisposition == OPEN_ALWAYS || dwCreationDisposition == CREATE_ALWAYS) mode = "w+b";
        else mode = "r+b";
    }
    FILE* f = fopen(lpFileName, mode);
    if (!f) return INVALID_HANDLE_VALUE;
    _LinuxFileHandle* h = (_LinuxFileHandle*)malloc(sizeof(_LinuxFileHandle));
    if (!h) { fclose(f); return INVALID_HANDLE_VALUE; }
    h->magic = _LINUX_FILE_HANDLE_MAGIC;
    h->fp    = f;
    return (HANDLE)h;
}

static inline BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, void* lpOverlapped) {
    (void)lpOverlapped;
    size_t r = fread(lpBuffer, 1, nNumberOfBytesToRead, _lfh_fp(hFile));
    if (lpNumberOfBytesRead) *lpNumberOfBytesRead = (DWORD)r;
    return TRUE;
}

static inline BOOL WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, void* lpOverlapped) {
    (void)lpOverlapped;
    size_t w = fwrite(lpBuffer, 1, nNumberOfBytesToWrite, _lfh_fp(hFile));
    if (lpNumberOfBytesWritten) *lpNumberOfBytesWritten = (DWORD)w;
    return TRUE;
}

static inline DWORD SetFilePointer(HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod) {
    (void)lpDistanceToMoveHigh;
    int origin = SEEK_SET;
    if (dwMoveMethod == FILE_CURRENT) origin = SEEK_CUR;
    else if (dwMoveMethod == FILE_END) origin = SEEK_END;
    FILE* f = _lfh_fp(hFile);
    fseek(f, lDistanceToMove, origin);
    return (DWORD)ftell(f);
}

static inline DWORD GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh) {
    FILE* f = _lfh_fp(hFile);
    long cur = ftell(f);
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, cur, SEEK_SET);
    if (lpFileSizeHigh) *lpFileSizeHigh = 0;
    return (DWORD)size;
}

static inline DWORD GetLastError(void) { return (DWORD)errno; }

/* ====================================================================
 *  Thread API stubs
 * ==================================================================== */
#define CREATE_SUSPENDED 0x00000004
#define STILL_ACTIVE 259
#define THREAD_PRIORITY_HIGHEST 2
#define THREAD_PRIORITY_NORMAL 0
#define THREAD_PRIORITY_LOWEST (-2)

static inline HANDLE GetCurrentThread(void) { return (HANDLE)(uintptr_t)pthread_self(); }
static inline BOOL SetThreadPriority(HANDLE hThread, int nPriority) { (void)hThread; (void)nPriority; return TRUE; }
static inline BOOL GetExitCodeThread(HANDLE hThread, LPDWORD lpExitCode) { (void)hThread; if (lpExitCode) *lpExitCode = 0; return TRUE; }
static inline DWORD ResumeThread(HANDLE hThread) { (void)hThread; return 0; }

/* ====================================================================
 *  Event API stubs
 * ==================================================================== */
static inline HANDLE CreateEvent(void* lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCSTR lpName) {
    (void)lpEventAttributes; (void)lpName;
    _LinuxEventHandle* ev = (_LinuxEventHandle*)malloc(sizeof(_LinuxEventHandle));
    if (!ev) return NULL;
    ev->magic = _LINUX_EVENT_HANDLE_MAGIC;
    ev->manualReset = bManualReset ? TRUE : FALSE;
    ev->signaled = bInitialState ? TRUE : FALSE;
    pthread_mutex_init(&ev->mutex, NULL);
    pthread_cond_init(&ev->cond, NULL);
    return (HANDLE)ev;
}
static inline BOOL SetEvent(HANDLE hEvent) {
    if (!hEvent || hEvent == INVALID_HANDLE_VALUE) return FALSE;
    _LinuxEventHandle* ev = (_LinuxEventHandle*)hEvent;
    if (ev->magic != _LINUX_EVENT_HANDLE_MAGIC) return FALSE;
    pthread_mutex_lock(&ev->mutex);
    ev->signaled = TRUE;
    if (ev->manualReset) pthread_cond_broadcast(&ev->cond);
    else pthread_cond_signal(&ev->cond);
    pthread_mutex_unlock(&ev->mutex);
    return TRUE;
}
static inline BOOL ResetEvent(HANDLE hEvent) {
    if (!hEvent || hEvent == INVALID_HANDLE_VALUE) return FALSE;
    _LinuxEventHandle* ev = (_LinuxEventHandle*)hEvent;
    if (ev->magic != _LINUX_EVENT_HANDLE_MAGIC) return FALSE;
    pthread_mutex_lock(&ev->mutex);
    ev->signaled = FALSE;
    pthread_mutex_unlock(&ev->mutex);
    return TRUE;
}

static inline DWORD WaitForMultipleObjects(DWORD nCount, const HANDLE* lpHandles, BOOL bWaitAll, DWORD dwMilliseconds) {
    DWORD start = GetTickCount();
    for (;;) {
        if (bWaitAll) {
            BOOL all = TRUE;
            for (DWORD i = 0; i < nCount; i++) {
                HANDLE h = lpHandles[i];
                if (!h || h == INVALID_HANDLE_VALUE) return WAIT_FAILED;
                _LinuxEventHandle* ev = (_LinuxEventHandle*)h;
                if (ev->magic != _LINUX_EVENT_HANDLE_MAGIC) return WAIT_FAILED;
                pthread_mutex_lock(&ev->mutex);
                BOOL signaled = ev->signaled;
                pthread_mutex_unlock(&ev->mutex);
                if (!signaled) { all = FALSE; break; }
            }
            if (all) {
                for (DWORD i = 0; i < nCount; i++) {
                    _LinuxEventHandle* ev = (_LinuxEventHandle*)lpHandles[i];
                    pthread_mutex_lock(&ev->mutex);
                    if (!ev->manualReset) ev->signaled = FALSE;
                    pthread_mutex_unlock(&ev->mutex);
                }
                return WAIT_OBJECT_0;
            }
        } else {
            for (DWORD i = 0; i < nCount; i++) {
                HANDLE h = lpHandles[i];
                if (!h || h == INVALID_HANDLE_VALUE) continue;
                _LinuxEventHandle* ev = (_LinuxEventHandle*)h;
                if (ev->magic != _LINUX_EVENT_HANDLE_MAGIC) continue;
                pthread_mutex_lock(&ev->mutex);
                BOOL signaled = ev->signaled;
                if (signaled && !ev->manualReset) ev->signaled = FALSE;
                pthread_mutex_unlock(&ev->mutex);
                if (signaled) return WAIT_OBJECT_0 + i;
            }
        }
        if (dwMilliseconds != INFINITE) {
            DWORD elapsed = GetTickCount() - start;
            if (elapsed >= dwMilliseconds) return WAIT_TIMEOUT;
        }
        usleep(1000);
    }
}

/* ====================================================================
 *  Memory status
 * ==================================================================== */
typedef struct {
    DWORD dwLength;
    DWORD dwMemoryLoad;
    SIZE_T dwTotalPhys;
    SIZE_T dwAvailPhys;
    SIZE_T dwTotalPageFile;
    SIZE_T dwAvailPageFile;
    SIZE_T dwTotalVirtual;
    SIZE_T dwAvailVirtual;
} MEMORYSTATUS;

static inline void GlobalMemoryStatus(MEMORYSTATUS* lpBuffer) {
    memset(lpBuffer, 0, sizeof(*lpBuffer));
    lpBuffer->dwLength = sizeof(*lpBuffer);
    lpBuffer->dwTotalPhys = 4ULL * 1024 * 1024 * 1024; // 4GB default
    lpBuffer->dwAvailPhys = 2ULL * 1024 * 1024 * 1024;
}

/* ====================================================================
 *  GetSystemTime
 * ==================================================================== */
static inline void GetSystemTime(SYSTEMTIME* st) { GetLocalTime(st); } // simplified

/* ====================================================================
 *  CreateFileMapping stub
 * ==================================================================== */
static inline HANDLE CreateFileMapping(HANDLE hFile, void* lpAttributes, DWORD flProtect, DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow, LPCSTR lpName) {
    (void)hFile; (void)lpAttributes; (void)flProtect; (void)dwMaximumSizeHigh; (void)lpName;
    void* p = malloc(dwMaximumSizeLow ? dwMaximumSizeLow : 1);
    return p ? (HANDLE)p : INVALID_HANDLE_VALUE;
}

/* ====================================================================
 *  Filesystem attribute defines
 * ==================================================================== */
/* sys/stat.h and dirent.h already included at the top */

#ifndef FILE_ATTRIBUTE_DIRECTORY
#define FILE_ATTRIBUTE_DIRECTORY 0x10
#endif
#ifndef FILE_FLAG_SEQUENTIAL_SCAN
#define FILE_FLAG_SEQUENTIAL_SCAN 0x08000000
#endif
#ifndef INVALID_FILE_ATTRIBUTES
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#endif
#ifndef GetFileExInfoStandard
#define GetFileExInfoStandard 0
#endif

/* ====================================================================
 *  DeleteFile / MoveFile / CreateDirectory
 * ==================================================================== */
static inline BOOL DeleteFile(LPCSTR lpFileName) {
    return remove(lpFileName) == 0;
}
static inline BOOL DeleteFileW(LPCWSTR lpFileName) {
    char buf[MAX_PATH];
    wcstombs(buf, lpFileName, MAX_PATH);
    return remove(buf) == 0;
}

static inline BOOL MoveFile(LPCSTR lpExistingFileName, LPCSTR lpNewFileName) {
    return rename(lpExistingFileName, lpNewFileName) == 0;
}
static inline BOOL MoveFileW(LPCWSTR lpExisting, LPCWSTR lpNew) {
    char a[MAX_PATH], b[MAX_PATH];
    wcstombs(a, lpExisting, MAX_PATH);
    wcstombs(b, lpNew, MAX_PATH);
    return rename(a, b) == 0;
}

static inline BOOL CreateDirectory(LPCSTR lpPathName, void* lpSecurityAttributes) {
    (void)lpSecurityAttributes;
    return mkdir(lpPathName, 0755) == 0 || errno == EEXIST;
}
static inline BOOL CreateDirectoryW(LPCWSTR lpPathName, void* lpSecurityAttributes) {
    char buf[MAX_PATH];
    wcstombs(buf, lpPathName, MAX_PATH);
    return CreateDirectory(buf, lpSecurityAttributes);
}

/* ====================================================================
 *  GetFileAttributes / GetFileAttributesEx
 * ==================================================================== */
static inline DWORD GetFileAttributes(LPCSTR lpFileName) {
    struct stat st;
    if (stat(lpFileName, &st) != 0) return INVALID_FILE_ATTRIBUTES;
    DWORD attrs = FILE_ATTRIBUTE_NORMAL;
    if (S_ISDIR(st.st_mode)) attrs |= FILE_ATTRIBUTE_DIRECTORY;
    return attrs;
}
static inline DWORD GetFileAttributesW(LPCWSTR lpFileName) {
    char buf[MAX_PATH];
    wcstombs(buf, lpFileName, MAX_PATH);
    return GetFileAttributes(buf);
}

typedef struct {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
} WIN32_FILE_ATTRIBUTE_DATA;

static inline BOOL GetFileAttributesEx(LPCSTR lpFileName, int fInfoLevelId, LPVOID lpFileInformation) {
    (void)fInfoLevelId;
    WIN32_FILE_ATTRIBUTE_DATA* data = (WIN32_FILE_ATTRIBUTE_DATA*)lpFileInformation;
    struct stat st;
    if (stat(lpFileName, &st) != 0) return FALSE;
    memset(data, 0, sizeof(*data));
    data->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
    if (S_ISDIR(st.st_mode)) data->dwFileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
    data->nFileSizeLow = (DWORD)(st.st_size & 0xFFFFFFFF);
    data->nFileSizeHigh = (DWORD)(st.st_size >> 32);
    return TRUE;
}
static inline BOOL GetFileAttributesExW(LPCWSTR lpFileName, int fInfoLevelId, LPVOID lpFileInformation) {
    char buf[MAX_PATH];
    wcstombs(buf, lpFileName, MAX_PATH);
    return GetFileAttributesEx(buf, fInfoLevelId, lpFileInformation);
}

/* ====================================================================
 *  FindFirstFile / FindNextFile / FindClose (directory enumeration)
 * ==================================================================== */
typedef struct {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    DWORD dwReserved0;
    DWORD dwReserved1;
    char cFileName[MAX_PATH];
    char cAlternateFileName[14];
} WIN32_FIND_DATA, *LPWIN32_FIND_DATA;

typedef WIN32_FIND_DATA WIN32_FIND_DATAW;

struct _LinuxFindData {
    DIR* dir;
    char basePath[MAX_PATH];
};

static inline HANDLE FindFirstFile(LPCSTR lpFileName, WIN32_FIND_DATA* lpFindFileData) {
    // lpFileName is like "path/*" - extract directory part
    char dirPath[MAX_PATH];
    strncpy(dirPath, lpFileName, MAX_PATH - 1);
    dirPath[MAX_PATH - 1] = 0;
    // Remove trailing wildcard
    char* star = strrchr(dirPath, '*');
    if (star) *star = 0;
    char* slash = strrchr(dirPath, '/');
    if (slash) *slash = 0;
    else strcpy(dirPath, ".");

    DIR* d = opendir(dirPath);
    if (!d) return INVALID_HANDLE_VALUE;

    _LinuxFindData* fd = (_LinuxFindData*)malloc(sizeof(_LinuxFindData));
    fd->dir = d;
    strncpy(fd->basePath, dirPath, MAX_PATH - 1);
    fd->basePath[MAX_PATH - 1] = 0;

    struct dirent* ent = readdir(d);
    if (!ent) { closedir(d); free(fd); return INVALID_HANDLE_VALUE; }

    memset(lpFindFileData, 0, sizeof(*lpFindFileData));
    strncpy(lpFindFileData->cFileName, ent->d_name, MAX_PATH - 1);
    lpFindFileData->cFileName[MAX_PATH - 1] = 0;

    char fullPath[MAX_PATH * 2];
    snprintf(fullPath, sizeof(fullPath), "%s/%s", fd->basePath, ent->d_name);
    struct stat st;
    if (stat(fullPath, &st) == 0) {
        if (S_ISDIR(st.st_mode)) lpFindFileData->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        else lpFindFileData->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
        lpFindFileData->nFileSizeLow = (DWORD)(st.st_size & 0xFFFFFFFF);
    }
    return (HANDLE)fd;
}

static inline BOOL FindNextFile(HANDLE hFindFile, WIN32_FIND_DATA* lpFindFileData) {
    _LinuxFindData* fd = (_LinuxFindData*)hFindFile;
    struct dirent* ent = readdir(fd->dir);
    if (!ent) return FALSE;
    memset(lpFindFileData, 0, sizeof(*lpFindFileData));
    strncpy(lpFindFileData->cFileName, ent->d_name, MAX_PATH - 1);
    lpFindFileData->cFileName[MAX_PATH - 1] = 0;

    char fullPath[MAX_PATH * 2];
    snprintf(fullPath, sizeof(fullPath), "%s/%s", fd->basePath, ent->d_name);
    struct stat st;
    if (stat(fullPath, &st) == 0) {
        if (S_ISDIR(st.st_mode)) lpFindFileData->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        else lpFindFileData->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
        lpFindFileData->nFileSizeLow = (DWORD)(st.st_size & 0xFFFFFFFF);
    }
    return TRUE;
}

static inline BOOL FindClose(HANDLE hFindFile) {
    _LinuxFindData* fd = (_LinuxFindData*)hFindFile;
    closedir(fd->dir);
    free(fd);
    return TRUE;
}

static inline HANDLE FindFirstFileW(LPCWSTR lpFileName, WIN32_FIND_DATA* lpFindFileData) {
    char buf[MAX_PATH];
    wcstombs(buf, lpFileName, MAX_PATH);
    return FindFirstFile(buf, lpFindFileData);
}
static inline BOOL FindNextFileW(HANDLE h, WIN32_FIND_DATA* d) { return FindNextFile(h, d); }

/* ====================================================================
 *  End of header
 * ==================================================================== */

/* _itow stub */
static inline wchar_t* _itow(int value, wchar_t* str, int radix) {
    (void)radix;
    swprintf(str, 32, L"%d", value);
    return str;
}
#endif /* _MC_LINUX_TYPES_H */

/* D3D11_VIEWPORT stub */
#ifndef _D3D11_VIEWPORT_DEFINED
#define _D3D11_VIEWPORT_DEFINED
typedef struct D3D11_VIEWPORT {
    FLOAT TopLeftX;
    FLOAT TopLeftY;
    FLOAT Width;
    FLOAT Height;
    FLOAT MinDepth;
    FLOAT MaxDepth;
} D3D11_VIEWPORT;
#endif

/* ERROR_SUCCESS */
#ifndef ERROR_SUCCESS
#define ERROR_SUCCESS 0
#endif

/* InterlockedCompareExchangeRelease64 */
#ifndef InterlockedCompareExchangeRelease64
#define InterlockedCompareExchangeRelease64(ptr, newval, oldval) \
    __sync_val_compare_and_swap((ptr), (oldval), (newval))
#endif

/* SystemTimeToFileTime stub */
static inline BOOL SystemTimeToFileTime(const SYSTEMTIME* lpSystemTime, FILETIME* lpFileTime) {
    struct tm t = {};
    t.tm_year = lpSystemTime->wYear - 1900;
    t.tm_mon = lpSystemTime->wMonth - 1;
    t.tm_mday = lpSystemTime->wDay;
    t.tm_hour = lpSystemTime->wHour;
    t.tm_min = lpSystemTime->wMinute;
    t.tm_sec = lpSystemTime->wSecond;
    time_t epoch = mktime(&t);
    /* Windows FILETIME: 100ns intervals since 1601-01-01 */
    uint64_t ft = ((uint64_t)epoch + 11644473600ULL) * 10000000ULL;
    ft += (uint64_t)lpSystemTime->wMilliseconds * 10000ULL;
    lpFileTime->dwLowDateTime = (DWORD)(ft & 0xFFFFFFFF);
    lpFileTime->dwHighDateTime = (DWORD)(ft >> 32);
    return TRUE;
}

/* Common game types */
typedef float F32;
typedef double F64;
typedef int32_t S32;
typedef int16_t S16;
typedef int8_t S8;
typedef uint32_t U32;
typedef uint16_t U16;
typedef uint8_t U8;

/* GetModuleHandle stub */
#ifndef GetModuleHandle
#define GetModuleHandle(x) ((HMODULE)NULL)
#endif
#ifndef GetModuleHandleA
#define GetModuleHandleA(x) ((HMODULE)NULL)
#endif
#ifndef GetModuleFileName
static inline DWORD GetModuleFileName(HMODULE hModule, char* lpFilename, DWORD nSize) {
    (void)hModule;
    ssize_t len = readlink("/proc/self/exe", lpFilename, nSize - 1);
    if (len < 0) { lpFilename[0] = 0; return 0; }
    lpFilename[len] = 0;
    return (DWORD)len;
}
#endif
#ifndef GetModuleFileNameA
#define GetModuleFileNameA GetModuleFileName
#endif

#define INVALID_SET_FILE_POINTER ((DWORD)0xFFFFFFFF)

/* DirectXMath stubs for Camera.cpp */
struct XMFLOAT4 { float x, y, z, w; };
struct XMFLOAT4X4 { float m[4][4]; };
typedef XMFLOAT4 XMVECTOR;
struct XMMATRIX {
    union {
        XMVECTOR r[4];
        float m[4][4];
    };
};

static inline XMMATRIX XMLoadFloat4x4(const float* m) {
    XMMATRIX r;
    memcpy(&r, m, sizeof(XMMATRIX));
    return r;
}
static inline XMMATRIX XMMatrixMultiply(XMMATRIX a, XMMATRIX b) {
    XMMATRIX r;
    memset(&r, 0, sizeof(r));
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            for (int k = 0; k < 4; k++)
                r.m[i][j] += a.m[i][k] * b.m[k][j];
    return r;
}
static inline XMVECTOR XMMatrixDeterminant(XMMATRIX m) {
    (void)m;
    XMVECTOR v = {1,0,0,0};
    return v;
}
static inline XMMATRIX XMMatrixInverse(XMVECTOR* det, XMMATRIX m) {
    (void)det;
    return m; /* stub */
}
static inline void XMStoreFloat4(XMFLOAT4* pDest, XMVECTOR v) {
    *pDest = v;
}
static inline XMVECTOR XMVector3TransformCoord(XMVECTOR v, XMMATRIX m) {
    (void)m;
    return v; /* stub */
}
static inline XMVECTOR XMVectorSet(float x, float y, float z, float w) {
    XMVECTOR v = {x, y, z, w};
    return v;
}
#define GetFileAttributesA GetFileAttributes

/* Additional Windows API stubs */
#define ERROR_IO_PENDING 997
#define ERROR_CANCELLED 1223
#define _TRUNCATE ((size_t)-1)

/* _vsnprintf_s: MSVC has template overloads. For Linux, just use vsnprintf. */
#define _vsnprintf_s(buf, ...) _linux_vsnprintf_s_impl(buf, sizeof(buf), __VA_ARGS__)
static inline int _linux_vsnprintf_s_impl(char* buffer, size_t bufSize, size_t countOrTrunc, const char* format, va_list argptr) {
    size_t sz = (countOrTrunc == (size_t)-1) ? bufSize : countOrTrunc;
    return vsnprintf(buffer, sz, format, argptr);
}

static inline int _i64toa_s(int64_t value, char* buffer, size_t sizeInCharacters, int radix) {
    (void)radix;
    snprintf(buffer, sizeInCharacters, "%lld", (long long)value);
    return 0;
}

static inline int _itoa_s(int value, char* buffer, size_t sizeInCharacters, int radix) {
    (void)radix;
    snprintf(buffer, sizeInCharacters, "%d", value);
    return 0;
}

#ifndef swscanf_s
#define swscanf_s swscanf
#endif

#ifndef sscanf_s
#define sscanf_s sscanf
#endif

/* MSVC SAL annotation stubs */
#ifndef _In_z_
#define _In_z_
#endif
#ifndef _Printf_format_string_
/* Not defined on Linux - the #ifdef check in Console_Utils.cpp will use the non-annotated path */
#endif
#ifndef CDECL
#define CDECL
#endif
#ifndef __cdecl
#define __cdecl
#endif

#define strncpy_s(dest, destsz, src, count) strncpy(dest, src, (count) < (destsz) ? (count) : (destsz) - 1)
#define wcsncpy_s(dest, destsz, src, count) wcsncpy(dest, src, (count) < (destsz) ? (count) : (destsz) - 1)
#define THREAD_PRIORITY_ABOVE_NORMAL 1
typedef int64_t LONG64;

#define _strtoui64(str, endptr, base) strtoull(str, endptr, base)
#define fprintf_s fprintf
static inline void GetSystemTimeAsFileTime(FILETIME* ft) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    SystemTimeToFileTime(&st, ft);
}
static inline DWORD GetCurrentProcessId(void) { return (DWORD)getpid(); }

static inline BOOL FileTimeToSystemTime(const FILETIME* ft, SYSTEMTIME* st) {
    uint64_t t = ((uint64_t)ft->dwHighDateTime << 32) | ft->dwLowDateTime;
    t = t / 10000000ULL - 11644473600ULL;
    time_t epoch = (time_t)t;
    struct tm tm_buf;
    gmtime_r(&epoch, &tm_buf);
    st->wYear = (WORD)(tm_buf.tm_year + 1900);
    st->wMonth = (WORD)(tm_buf.tm_mon + 1);
    st->wDay = (WORD)tm_buf.tm_mday;
    st->wHour = (WORD)tm_buf.tm_hour;
    st->wMinute = (WORD)tm_buf.tm_min;
    st->wSecond = (WORD)tm_buf.tm_sec;
    st->wMilliseconds = 0;
    st->wDayOfWeek = (WORD)tm_buf.tm_wday;
    return TRUE;
}
