#pragma once

namespace ro::cursor {
struct PickRect { float left, top, right, bottom; };

// Ref Item::SetRenderInfo (34), GameActor::SetRenderInfo (40): integer
// half-padding grows small pick records without inflating large sprites.
inline PickRect ExpandPickRect(PickRect rect, float screenXFactor, int minimum) {
    const int size = static_cast<int>(screenXFactor * minimum);
    const int width = static_cast<int>(rect.right - rect.left);
    const int height = static_cast<int>(rect.bottom - rect.top);
    if (width < size) {
        const int padding = (size - width) / 2;
        rect.left -= padding; rect.right += padding;
    }
    if (height < size) {
        const int padding = (size - height) / 2;
        rect.top -= padding; rect.bottom += padding;
    }
    return rect;
}
}
