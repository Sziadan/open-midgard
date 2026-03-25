//===========================================================================
// Timer.cpp  –  Frame-timing implementation
// Clean C++17 rewrite.
//===========================================================================
#include "Timer.h"
#include "Types.h"
#include <mmsystem.h>
#include <windows.h>

// ---------------------------------------------------------------------------
// Module-local state
// ---------------------------------------------------------------------------
static DWORD s_frameTime = 0;   // timeGetTime() snapshot of last frame
static DWORD s_frameFreq = 16;  // milliseconds per frame tick

// ---------------------------------------------------------------------------

void InitTimer(unsigned int framesPerSecond)
{
    s_frameTime = timeGetTime();
    // Compute ms-per-frame from target FPS (1000 / fps)
    s_frameFreq = (framesPerSecond > 0) ? (1000u / framesPerSecond) : 16u;
}

void ResetTimer()
{
    s_frameTime = timeGetTime();
}

unsigned long GetSkipFrameCount()
{
    DWORD now    = timeGetTime();
    DWORD frames = (now - s_frameTime) / s_frameFreq;
    s_frameTime += frames * s_frameFreq;

    if (!g_frameskip)
        return 1;
    return frames;
}

int SkipSceneRendering()
{
    // Original implementation always returned 1 (do not skip rendering).
    return 1;
}
