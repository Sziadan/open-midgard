#pragma once

#include "Types.h"

struct HDC__;
typedef HDC__* HDC;

void DrawModeCursor(int cursorActNum, u32 mouseAnimStartTick);
bool DrawModeCursorToHdc(HDC hdc, int cursorActNum, u32 mouseAnimStartTick);
u32 GetModeCursorVisualFrame(int cursorActNum, u32 mouseAnimStartTick);