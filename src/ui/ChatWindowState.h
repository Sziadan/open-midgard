#pragma once

namespace ro::chat {

constexpr int NormalizeState(int requested) noexcept
{
    if (requested < 0) {
        return 15;
    }
    if (requested > 15) {
        return 0;
    }
    return requested;
}

constexpr int HeightForState(int requested) noexcept
{
    const int state = NormalizeState(requested);
    return state == 0 ? 24 : 16 + 24 + 14 * (state + 3);
}

// Ref/UIFrameWnd3.cpp UINewChatWnd::SendMsg child 2, confirmed against PE
// instructions at 0x49E52C: SBB EAX,EAX sets the negative candidate high bits; AND AL
// preserves them, so decrementing from state 0 requests -1.
constexpr int CycleState(int current, bool decrease) noexcept
{
    const int state = NormalizeState(current);
    const int candidate = decrease
        ? (state == 0 ? -1 : state - 3)
        : (state == 0 ? 1 : state + 3);

    if (candidate <= 0) {
        return state == 0 ? NormalizeState(candidate) : 0;
    }

    return NormalizeState(candidate + 1 - candidate % 3);
}

constexpr int StateFromRequestedHeight(int totalHeight) noexcept
{
    if (totalHeight < 72) {
        return 0;
    }
    const int state = (totalHeight - 72) / 14 + 1;
    return state > 15 ? 15 : state;
}

} // namespace ro::chat
