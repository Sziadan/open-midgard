#include "../../src/core/ChatHistoryView.h"

#include <climits>
#include <cstdio>

int main()
{
    int checks = 0;
    int failures = 0;
    auto check = [&](bool condition, const char* name) {
        ++checks;
        if (!condition) {
            std::fprintf(stderr, "FAIL: %s\n", name);
            ++failures;
        }
    };

    ro::chat::HistoryView view;
    check(view.rows == 5 && view.first == 0 && view.selected == -1,
        "default view starts at top with no selection");
    check(view.LastFirst(10) == 5,
        "five visible rows over ten items has bottom first index five");

    view.first = 5;
    view.Scroll(-3, 10);
    check(view.first == 2, "scrolling up three rows moves first index to two");
    view.AfterAppend(10, 11);
    check(view.first == 2, "append while scrolled away from bottom preserves offset");

    view.Resize(9, 200);
    check(view.rows == 9 && view.first == 191,
        "resize snaps scrollable history to its new bottom");

    view.Resize(9, 240);
    view.first = 200;
    view.AfterAppend(240, 121);
    check(view.first == 111,
        "trim clamps an away-from-bottom offset to the retained 120 rows");

    view.Resize(9, 240);
    view.first = 231;
    view.selected = 230;
    view.AfterAppend(240, 121);
    check(view.first == 112,
        "append at bottom remains at bottom after trimming the oldest half");
    check(view.selected == 230,
        "trim leaves the numeric selection unchanged for consumer bounds checks");

    view.Scroll(INT_MAX, 200);
    check(view.first == view.LastFirst(200), "large positive scroll clamps to bottom");
    view.Scroll(INT_MIN, 200);
    check(view.first == 0, "large negative scroll clamps to top without overflow");

    view.Resize(0, 200);
    check(view.rows == 0 && view.first == 200,
        "zero-row resize is safe and sets the first index to item count");
    view.Scroll(INT_MIN, 200);
    check(view.first == 0, "zero-row view remains safely scrollable");

    std::printf("Chat history view: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
