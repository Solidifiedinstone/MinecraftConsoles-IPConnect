#include "stdafx.h"
#include "Filesystem.h"

#ifdef _WINDOWS64
#include <windows.h>
#endif

#include <stdio.h>

#if defined(_LINUX64)
// Linux: use our POSIX wrappers from LinuxTypes.h
#define GetFileAttributesA GetFileAttributes
#define FindFirstFileA FindFirstFile
#define FindNextFileA FindNextFile
typedef WIN32_FIND_DATA WIN32_FIND_DATAA;
#endif

bool FileOrDirectoryExists(const char* path)
{
#if defined(_WINDOWS64) || defined(_LINUX64)
    DWORD attribs = GetFileAttributesA(path);
    return (attribs != INVALID_FILE_ATTRIBUTES);
#else
    #error "FileOrDirectoryExists not implemented for this platform"
    return false;
#endif
}

bool FileExists(const char* path)
{
#if defined(_WINDOWS64) || defined(_LINUX64)
    DWORD attribs = GetFileAttributesA(path);
    return (attribs != INVALID_FILE_ATTRIBUTES && !(attribs & FILE_ATTRIBUTE_DIRECTORY));
#else
    #error "FileExists not implemented for this platform"
    return false;
#endif
}

bool DirectoryExists(const char* path)
{
#if defined(_WINDOWS64) || defined(_LINUX64)
    DWORD attribs = GetFileAttributesA(path);
    return (attribs != INVALID_FILE_ATTRIBUTES && (attribs & FILE_ATTRIBUTE_DIRECTORY));
#else
    #error "DirectoryExists not implemented for this platform"
    return false;
#endif
}

bool GetFirstFileInDirectory(const char* directory, char* outFilePath, size_t outFilePathSize)
{
#if defined(_WINDOWS64) || defined(_LINUX64)
    char searchPath[MAX_PATH];
    snprintf(searchPath, MAX_PATH, "%s/*", directory);

    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath, &findData);

    if (hFind == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    do
    {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            snprintf(outFilePath, outFilePathSize, "%s/%s", directory, findData.cFileName);
            FindClose(hFind);
            return true;
        }
    } while (FindNextFileA(hFind, &findData) != 0);

    FindClose(hFind);
    return false;
#else
    #error "GetFirstFileInDirectory not implemented for this platform"
    return false;
#endif
}
