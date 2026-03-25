#pragma once
//===========================================================================
// Timer.h  –  Frame-timing utilities
// Clean C++17 rewrite.
//===========================================================================

// Initialize the frame timer.
// freq – target frames per second (typically 60).
void InitTimer(unsigned int freq);

// Reset the frame timer base to the current time.
void ResetTimer();

// Return the number of frames that should be processed this update.
// Returns 1 when frame-skipping is disabled (g_frameskip == 0).
unsigned long GetSkipFrameCount();

// Returns 1 (stub: was originally meant to suppress scene rendering
// if behind schedule, but the original implementation always returned 1).
int SkipSceneRendering();
