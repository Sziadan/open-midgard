#pragma once

#include <string>

#include "platform/WindowsCompat.h"

// Renders NPC script / packet text with ^RRGGBB color switches (six hex digits after ^).
// Say dialog: color persists across lines until the next page (new draw starts at black).
// Menu options: each row starts at default black.

void DrawNpcSayDialogColoredText(HDC hdc, const RECT& textRect, const std::string& text, int verticalOffset = 0);
int MeasureNpcSayDialogColoredTextHeight(HDC hdc, int width, const std::string& text);
void DrawNpcMenuOptionColoredText(HDC hdc, const RECT& textRect, const std::string& text);
