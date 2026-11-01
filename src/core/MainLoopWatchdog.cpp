#include "MainLoopWatchdog.h"
#include "DebugLog.h"

#include <atomic>
#include <windows.h>
#ifndef _WIN32
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <system_error>
#include <thread>
#endif

namespace {

constexpr DWORD kPollIntervalMs = 1000;
constexpr DWORD kHeartbeatIntervalMs = 5000;
constexpr DWORD kStallThresholdMs = 3000;
constexpr DWORD kStallRepeatMs = 10000;

std::atomic<unsigned long long> g_frameCount{0};
std::atomic<DWORD> g_lastFrameTick{0};
std::atomic<int> g_modeType{-1};
std::atomic<const char*> g_stage{"startup"};
std::atomic<bool> g_running{false};
#ifdef _WIN32
HANDLE g_thread = nullptr;
HANDLE g_stopEvent = nullptr;
#else
std::thread g_thread;
std::mutex g_stopMutex;
std::condition_variable g_stopCondition;
#endif

bool WaitForNextPoll()
{
#ifdef _WIN32
    return WaitForSingleObject(g_stopEvent, kPollIntervalMs) == WAIT_TIMEOUT;
#else
    std::unique_lock<std::mutex> lock(g_stopMutex);
    return !g_stopCondition.wait_for(lock, std::chrono::milliseconds(kPollIntervalMs),
        [] { return !g_running.load(); });
#endif
}

DWORD WINAPI WatchdogThreadMain(LPVOID)
{
    DWORD lastHeartbeatTick = GetTickCount();
    DWORD lastStallLogTick = 0;
    unsigned long long lastSeenFrames = g_frameCount.load();
    bool stalled = false;

    while (WaitForNextPoll()) {
        const DWORD now = GetTickCount();
        const unsigned long long frames = g_frameCount.load();
        const DWORD lastFrameTick = g_lastFrameTick.load();
        const char* stage = g_stage.load();
        const int mode = g_modeType.load();

        if (frames == lastSeenFrames && lastFrameTick != 0 && now - lastFrameTick >= kStallThresholdMs) {
            if (!stalled || now - lastStallLogTick >= kStallRepeatMs) {
                DbgLog("[Watchdog] main loop stalled %lu ms at stage='%s' frame=%llu mode=%d\n",
                    static_cast<unsigned long>(now - lastFrameTick), stage, frames, mode);
                lastStallLogTick = now;
                stalled = true;
            }
        } else if (frames != lastSeenFrames && stalled) {
            DbgLog("[Watchdog] main loop resumed at stage='%s' frame=%llu mode=%d\n", stage, frames, mode);
            stalled = false;
        }

        if (now - lastHeartbeatTick >= kHeartbeatIntervalMs) {
            const double seconds = static_cast<double>(now - lastHeartbeatTick) / 1000.0;
            const double fps = seconds > 0.0 ? static_cast<double>(frames - lastSeenFrames) / seconds : 0.0;
            DbgLog("[Heartbeat] frames=%llu fps=%.1f stage='%s' mode=%d\n", frames, fps, stage, mode);
            lastHeartbeatTick = now;
            lastSeenFrames = frames;
        }
    }
    return 0;
}

} // namespace

void MainLoopWatchdogStart()
{
    if (g_running.exchange(true)) {
        return;
    }
#ifdef _WIN32
    g_stopEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    if (!g_stopEvent) {
        g_running = false;
        return;
    }
    g_lastFrameTick = GetTickCount();
    g_thread = CreateThread(nullptr, 0, WatchdogThreadMain, nullptr, 0, nullptr);
    if (!g_thread) {
        CloseHandle(g_stopEvent);
        g_stopEvent = nullptr;
        g_running = false;
        return;
    }
#else
    g_lastFrameTick = GetTickCount();
    try {
        g_thread = std::thread([] { WatchdogThreadMain(nullptr); });
    } catch (const std::system_error& error) {
        g_running = false;
        DbgLog("[Watchdog] failed to start: %s\n", error.what());
        return;
    }
#endif
    DbgLog("[Watchdog] started (stall threshold %lu ms, heartbeat every %lu ms)\n",
        static_cast<unsigned long>(kStallThresholdMs), static_cast<unsigned long>(kHeartbeatIntervalMs));
}

void MainLoopWatchdogStop()
{
    if (!g_running.exchange(false)) {
        return;
    }
#ifdef _WIN32
    if (g_stopEvent) {
        SetEvent(g_stopEvent);
    }
    if (g_thread) {
        WaitForSingleObject(g_thread, 2000);
        CloseHandle(g_thread);
        g_thread = nullptr;
    }
    if (g_stopEvent) {
        CloseHandle(g_stopEvent);
        g_stopEvent = nullptr;
    }
#else
    {
        std::lock_guard<std::mutex> lock(g_stopMutex);
        g_stopCondition.notify_all();
    }
    if (g_thread.joinable()) {
        g_thread.join();
    }
#endif
    DbgLog("[Watchdog] stopped after %llu frames\n", g_frameCount.load());
}

void MainLoopWatchdogMarkFrame(int modeType)
{
    g_frameCount.fetch_add(1);
    g_lastFrameTick = GetTickCount();
    g_modeType = modeType;
}

void MainLoopWatchdogSetStage(const char* stage)
{
    g_stage = stage ? stage : "";
}
