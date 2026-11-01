#include "core/MainLoopWatchdog.h"
#include <cassert>
#include <chrono>
#include <thread>

int main()
{
    MainLoopWatchdogStop();
    for (int cycle = 0; cycle < 20; ++cycle) {
        MainLoopWatchdogStart();
        MainLoopWatchdogStart();
        MainLoopWatchdogSetStage("regression");
        MainLoopWatchdogMarkFrame(cycle);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        const auto start = std::chrono::steady_clock::now();
        MainLoopWatchdogStop();
        assert(std::chrono::steady_clock::now() - start < std::chrono::milliseconds(900));
        MainLoopWatchdogStop();
    }
    MainLoopWatchdogSetStage(nullptr);
}
