#pragma once

#include "Types.h"

struct HDC__;
typedef HDC__* HDC;

void DrawModeCursor(int cursorActNum, u32 mouseAnimStartTick);
bool DrawModeCursorToHdc(HDC hdc, int cursorActNum, u32 mouseAnimStartTick);
bool DrawModeCursorAtToHdc(HDC hdc, int x, int y, int cursorActNum, u32 mouseAnimStartTick);
bool DrawModeCursorAtToArgb(unsigned int* dest, int destW, int destH, int x, int y, int cursorActNum, u32 mouseAnimStartTick);
bool GetModeCursorClientPos(POINT* outPoint);
u32 GetModeCursorVisualFrame(int cursorActNum, u32 mouseAnimStartTick);