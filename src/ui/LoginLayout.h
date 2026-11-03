#pragma once
#include <algorithm>
#include <array>

namespace loginLayout {
struct Rect {
    int x = 0, y = 0, width = 0, height = 0;
    bool Contains(int px, int py) const {
        return px >= x && py >= y && px < x + width && py < y + height;
    }
};
struct Button { int id; Rect rect; const char* label; };
struct Layout {
    int x = 0, y = 0, width = 0, height = 0;
    bool compact = false;
    Rect account, password, remember;
    std::array<Button, 4> buttons{};
};
inline Layout Build(int width, bool compact) {
    Layout result;
    result.width = width;
    result.height = compact ? 228 : 338;
    result.compact = compact;
    const int padding = compact ? 16 : 20;
    const int content = (std::max)(0, width - padding * 2);
    result.account = {padding, compact ? 42 : 112, content, compact ? 28 : 32};
    result.password = {padding, compact ? 94 : 172, content, compact ? 28 : 32};
    result.remember = {padding, compact ? 130 : 216, content, 20};
    const int exitWidth = 76;
    const int actionY = compact ? 162 : 254;
    const int actionHeight = compact ? 28 : 32;
    const int footerY = compact ? 198 : 304;
    const int footerWidth = (std::max)(0, (content - 8) / 2);
    result.buttons = {{
        {120, {padding, actionY, (std::max)(0, content - exitWidth - 8), actionHeight}, "Connect"},
        {155, {width - padding - exitWidth, actionY, exitWidth, actionHeight}, "Exit"},
        {201, {padding, footerY, footerWidth, 22}, "Account request"},
        {219, {width - padding - footerWidth, footerY, footerWidth, 22}, "Introduction"}
    }};
    return result;
}
inline Layout Place(int clientWidth, int clientHeight, int reservedTop = 0) {
    const int width = (std::max)(0, (std::min)(360, clientWidth - 24));
    const bool compact = clientHeight - reservedTop < 370;
    Layout result = Build(width, compact);
    result.x = (std::max)(0, (clientWidth - result.width) / 2);
    result.y = (std::max)(0, (clientHeight - result.height - reservedTop) / 2 + reservedTop);
    result.y = (std::min)(result.y, (std::max)(0, clientHeight - result.height - 6));
    return result;
}
}
