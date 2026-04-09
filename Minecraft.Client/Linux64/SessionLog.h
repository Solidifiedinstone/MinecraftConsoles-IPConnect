#pragma once

// Session-based log file for the Linux64 build.
// Opens a timestamped log file in a "logs/" directory next to the executable.
// All OutputDebugString / fprintf(stderr) output is mirrored to the log file.

#ifdef __cplusplus
extern "C" {
#endif

// Call once at startup (before any logging). Creates the logs/ directory and
// opens a file like "logs/session_2026-03-29_14-30-45.log".
void SessionLog_Init(void);

// Flush and close the log file. Safe to call even if Init was not called.
void SessionLog_Shutdown(void);

// Write a narrow-string message. Thread-safe. Also echoes to stderr.
void SessionLog_Write(const char* msg);

// Write a wide-string message. Thread-safe. Also echoes to stderr.
void SessionLog_WriteW(const wchar_t* msg);

// printf-style convenience. Thread-safe. Also echoes to stderr.
void SessionLog_Printf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

#ifdef __cplusplus
}
#endif
