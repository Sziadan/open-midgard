#pragma once
// Writes to a per-process log file in the working directory.
#include "platform/WindowsCompat.h"
#include <atomic>
#include <cstdio>
#include <cstdarg>
#include <cstring>

enum DebugLogFlag : unsigned int {
    DebugLogInfo = 1u << 0,
    DebugLogActors = 1u << 1,
    DebugLogEffects = 1u << 2,
    DebugLogGameplay = 1u << 3,
    DebugLogPerformance = 1u << 4,
    DebugLogReplay = 1u << 5,
};

inline std::atomic<unsigned int> g_debugLogFlags{0};

inline void SetDebugLogFlags(unsigned int flags)
{
    g_debugLogFlags.store(flags, std::memory_order_relaxed);
}

inline void EnableDebugLogFlags(unsigned int flags)
{
    g_debugLogFlags.fetch_or(flags, std::memory_order_relaxed);
}

inline bool DebugLogEnabled(unsigned int flag)
{
    const unsigned int flags = g_debugLogFlags.load(std::memory_order_relaxed);
    return (flags & (DebugLogInfo | flag)) != 0;
}

inline bool DebugLogMessageEnabled(const char* fmt)
{
    const unsigned int flags = g_debugLogFlags.load(std::memory_order_relaxed);
    if (flags & DebugLogInfo) return true;
    if (flags == 0) return false;
    if (std::strncmp(fmt, "[ActorTrace]", 12) == 0
        || std::strncmp(fmt, "[ActorMove]", 11) == 0) return (flags & DebugLogActors) != 0;
    if (std::strncmp(fmt, "[WorldEffect]", 13) == 0
        || std::strncmp(fmt, "[SkillWave]", 11) == 0) return (flags & DebugLogEffects) != 0;
    if (std::strncmp(fmt, "[GameMode] frame=", 17) == 0) return (flags & DebugLogPerformance) != 0;
    if (std::strncmp(fmt, "[GameMode]", 10) == 0
        || std::strncmp(fmt, "[Net]", 5) == 0) return (flags & DebugLogGameplay) != 0;
    if (std::strncmp(fmt, "[ViewPerfHiRes]", 15) == 0
        || std::strncmp(fmt, "[GameModePerfHiRes]", 19) == 0
        || std::strncmp(fmt, "[OverlayPerfHiRes]", 18) == 0
        || std::strncmp(fmt, "[QtUiPerf]", 10) == 0
        || std::strncmp(fmt, "[View]", 6) == 0) return (flags & DebugLogPerformance) != 0;
    if (std::strncmp(fmt, "[REPLAY]", 8) == 0) return (flags & DebugLogReplay) != 0;
    return false;
}

inline void DbgLogWrite(const char* fmt, va_list args)
{
    char msg[1024];
    _vsnprintf_s(msg, sizeof(msg), _TRUNCATE, fmt, args);

    const DWORD pid = GetCurrentProcessId();
    const DWORD tick = GetTickCount();

    char line[1152];
    _snprintf_s(line, sizeof(line), _TRUNCATE, "[pid:%lu t:%lu] %s",
        static_cast<unsigned long>(pid),
        static_cast<unsigned long>(tick),
        msg);

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

inline void DbgLog(const char* fmt, ...)
{
    if (!DebugLogMessageEnabled(fmt)) return;
    va_list args;
    va_start(args, fmt);
    DbgLogWrite(fmt, args);
    va_end(args);
}

// For diagnostics whose call sites already check their own opt-in setting.
inline void DbgAlways(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    DbgLogWrite(fmt, args);
    va_end(args);
}

inline void DbgError(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    DbgLogWrite(fmt, args);
    va_end(args);
}
