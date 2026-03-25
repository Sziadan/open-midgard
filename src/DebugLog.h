#pragma once
// Minimal debug logger – writes to a per-process log file in the working directory.
// Include and call DbgLog(...) wherever needed. Remove file when done debugging.
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <windows.h>

inline void DbgLog(const char* fmt, ...)
{
    char msg[1024];
    va_list args;
    va_start(args, fmt);
    _vsnprintf_s(msg, sizeof(msg), _TRUNCATE, fmt, args);
    va_end(args);

    const DWORD pid = GetCurrentProcessId();
    const DWORD tick = GetTickCount();

    char line[1152];
    if (std::strncmp(msg, "[", 1) == 0) {
        _snprintf_s(line, sizeof(line), _TRUNCATE, "[pid:%lu t:%lu] %s",
            static_cast<unsigned long>(pid),
            static_cast<unsigned long>(tick),
            msg);
    } else {
        _snprintf_s(line, sizeof(line), _TRUNCATE, "[pid:%lu t:%lu] %s",
            static_cast<unsigned long>(pid),
            static_cast<unsigned long>(tick),
            msg);
    }

    // Write to debugger output (visible in VS Output window)
    OutputDebugStringA(line);

    // Also append to a per-process log file in the CWD.
    char logPath[MAX_PATH];
    _snprintf_s(logPath, sizeof(logPath), _TRUNCATE, "debug_hp_%lu.log",
        static_cast<unsigned long>(pid));

    FILE* f = nullptr;
    if (fopen_s(&f, logPath, "a") == 0 && f)
    {
        fputs(line, f);
        fclose(f);
    }
}
