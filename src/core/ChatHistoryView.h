#pragma once
#include <algorithm>

namespace ro::chat {
// UIChatHisBox state: offsets are from the first row, not distance from bottom.
struct HistoryView {
    int rows = 5;
    int first = 0;
    int selected = -1;
    int scrollPosition = 0;

    int LastFirst(int count) const { return (std::max)(0, count - rows); }
    void Resize(int visibleRows, int count) {
        rows = (std::max)(0, visibleRows);
        first = scrollPosition = LastFirst(count);
    }
    void Scroll(int delta, int count) {
        first = scrollPosition = static_cast<int>(std::clamp(static_cast<long long>(first) + delta, 0LL,
            static_cast<long long>(LastFirst(count))));
    }
    void AfterAppend(int before, int after) {
        const bool bottom = before <= rows || first == LastFirst(before);
        // RecalcScrbarPos clamps the BAR, not the parent's text view offset.
        // An upward-scrolled view can remain beyond the surviving rows until
        // Scroll/Resize reconciles it. Preserve that source blank-view quirk.
        if (before >= 240) scrollPosition = std::clamp(scrollPosition, 0, LastFirst(before - before / 2));
        scrollPosition = std::clamp(scrollPosition, 0, LastFirst(after));
        if (bottom && after > rows) first = scrollPosition = LastFirst(after);
        // Source leaves the selected numeric index unchanged during main trim.
        // Consumers must bounds-check it rather than reproduce its unsafe read.
    }
};
} // namespace ro::chat
