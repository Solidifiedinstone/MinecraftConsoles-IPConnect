#include "SessionLog.h"

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <ctime>
#include <cwchar>
#include <climits>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>

static FILE*           s_logFile  = nullptr;
static pthread_mutex_t s_logMutex = PTHREAD_MUTEX_INITIALIZER;

// Prefix each line with a high-resolution timestamp.
static void WriteTimestamp(FILE* f)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm_buf;
    localtime_r(&ts.tv_sec, &tm_buf);
    fprintf(f, "[%04d-%02d-%02d %02d:%02d:%02d.%03ld] ",
            tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
            tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
            ts.tv_nsec / 1000000);
}

void SessionLog_Init(void)
{
    // Resolve the directory containing the executable.
    char exeDir[PATH_MAX] = {};
    ssize_t len = readlink("/proc/self/exe", exeDir, sizeof(exeDir) - 1);
    if (len <= 0)
        return;
    exeDir[len] = 0;
    char* lastSlash = strrchr(exeDir, '/');
    if (lastSlash)
        *(lastSlash + 1) = '\0';

    // Create "logs/" subdirectory (ignore EEXIST).
    char logsDir[PATH_MAX] = {};
    snprintf(logsDir, sizeof(logsDir), "%slogs", exeDir);
    mkdir(logsDir, 0755);

    // Build a filename with the current timestamp.
    time_t now = time(nullptr);
    struct tm tm_buf;
    localtime_r(&now, &tm_buf);

    char filePath[PATH_MAX] = {};
    snprintf(filePath, sizeof(filePath),
             "%s/session_%04d-%02d-%02d_%02d-%02d-%02d.log",
             logsDir,
             tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
             tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec);

    s_logFile = fopen(filePath, "w");
    if (s_logFile)
    {
        // Line-buffered so entries appear promptly.
        setvbuf(s_logFile, nullptr, _IOLBF, 0);

        // Write a header.
        fprintf(s_logFile, "=== Session log started: %04d-%02d-%02d %02d:%02d:%02d ===\n",
                tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
                tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec);

        fprintf(stderr, "[SessionLog] Logging to %s\n", filePath);
    }
}

void SessionLog_Shutdown(void)
{
    pthread_mutex_lock(&s_logMutex);
    if (s_logFile)
    {
        fprintf(s_logFile, "=== Session log closed ===\n");
        fclose(s_logFile);
        s_logFile = nullptr;
    }
    pthread_mutex_unlock(&s_logMutex);
}

void SessionLog_Write(const char* msg)
{
    if (!msg)
        return;

    // Always write to stderr.
    fprintf(stderr, "%s", msg);

    pthread_mutex_lock(&s_logMutex);
    if (s_logFile)
    {
        WriteTimestamp(s_logFile);
        fprintf(s_logFile, "%s", msg);
        // Ensure the line ends with a newline.
        size_t len = strlen(msg);
        if (len > 0 && msg[len - 1] != '\n')
            fputc('\n', s_logFile);
    }
    pthread_mutex_unlock(&s_logMutex);
}

void SessionLog_WriteW(const wchar_t* msg)
{
    if (!msg)
        return;

    // Always write to stderr.
    fwprintf(stderr, L"%ls", msg);

    pthread_mutex_lock(&s_logMutex);
    if (s_logFile)
    {
        WriteTimestamp(s_logFile);
        // Convert wide to narrow for the log file.
        fprintf(s_logFile, "%ls", msg);
        size_t len = wcslen(msg);
        if (len > 0 && msg[len - 1] != L'\n')
            fputc('\n', s_logFile);
    }
    pthread_mutex_unlock(&s_logMutex);
}

void SessionLog_Printf(const char* fmt, ...)
{
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    SessionLog_Write(buf);
}
