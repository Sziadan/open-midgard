#pragma once

// Background watchdog for the main loop. It runs on its own thread and writes
// to the debug log independently of the main thread, so a frozen client leaves
// evidence of *where* it froze:
//   - "[Heartbeat]" lines keep appearing while frames stop advancing: the main
//     thread is stuck (the stage name says in which step).
//   - "[Heartbeat]" lines stop as well: the whole process (or the machine) was
//     no longer running.
// Stage names must be string literals; only the pointer is stored.
void MainLoopWatchdogStart();
void MainLoopWatchdogStop();
void MainLoopWatchdogMarkFrame(int modeType);
void MainLoopWatchdogSetStage(const char* stage);
