#pragma once

namespace statusLayout {
inline constexpr int width = 432;
inline constexpr int height = 252;
inline constexpr int miniHeight = 17;
inline constexpr int firstRowY = 54;
inline constexpr int rowStep = 26;
inline constexpr int increaseX = 142;
inline constexpr int increaseSize = 22;
constexpr int rowY(int index) { return firstRowY + rowStep * index; }
}
