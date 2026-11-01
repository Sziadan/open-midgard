#pragma once

namespace basicInfoLayout {
inline constexpr int width = 320;
inline constexpr int height = 196;
inline constexpr int miniHeight = 58;
inline constexpr int menuWidth = 32;
inline constexpr int menuHeight = 28;
struct ButtonRect { int x, y, width, height; };
constexpr ButtonRect menuButton(int index)
{
    return {236 + 36 * (index % 2), 28 + 36 * (index / 2), menuWidth, menuHeight};
}
}
