#include "../../src/core/ChatScrollBar.h"

#include <climits>
#include <cstdlib>
#include <iostream>

namespace {
void Require(bool ok, const char* name)
{
    if (!ok) {
        std::cerr << "ChatScrollBarTests failed: " << name << '\n';
        std::exit(1);
    }
}
}

int main()
{
    using namespace ro::chat;
    ScrollBar history{126, 10, 231, 0, 9, 240};
    Require(history.ThumbSize() == 8, "history thumb minimum");
    Require(history.Travel() == 98 && history.ThumbStart() == 10,
        "history thumb track at position zero");

    ScrollBar options{162, 13, 6, 0, 8, 14};
    Require(options.ThumbSize() == 77 && options.Travel() == 59
        && options.ThumbStart() == 13, "options proportional thumb at zero");
    options.position = 6;
    Require(options.ThumbStart() == 72, "options thumb at maximum position");
    options.position = 3;
    Require(options.ThumbStart() == 42, "options middle thumb start");

    bool fixedWidthAccepted = true;
    for (int x = 0; x <= 12; ++x) fixedWidthAccepted &= options.Hit(x, 12) == ScrollHit::Up;
    Require(fixedWidthAccepted, "scrollbar x pixels zero through twelve are accepted");
    Require(options.Hit(0, 12) == ScrollHit::Up, "top arrow starts before button edge");
    Require(options.Hit(0, 13) == ScrollHit::PageUp, "page-up begins at button edge");
    Require(options.Hit(0, 41) == ScrollHit::PageUp, "page-up ends before thumb");
    Require(options.Hit(0, 42) == ScrollHit::Thumb, "thumb includes its start");
    Require(options.Hit(0, 118) == ScrollHit::Thumb, "thumb ends exclusively");
    Require(options.Hit(0, 119) == ScrollHit::PageDown, "page-down begins after thumb");
    Require(options.Hit(0, 148) == ScrollHit::PageDown, "page-down ends before down arrow");
    Require(options.Hit(0, 149) == ScrollHit::Down, "down arrow begins at bottom button edge");
    Require(options.Hit(13, 149) == ScrollHit::None, "x13 lies outside fixed scrollbar width");
    Require(options.Hit(0, 162) == ScrollHit::None, "y at axis end is outside scrollbar");
    Require(options.Step(ScrollHit::Up) == -1 && options.Step(ScrollHit::Down) == 1,
        "arrow step deltas");
    Require(options.Step(ScrollHit::PageUp) == -7 && options.Step(ScrollHit::PageDown) == 7,
        "page deltas are page minus one");

    ScrollDrag drag;
    drag.Begin(100);
    Require(drag.Request(101, history) == 2,
        "relative one-pixel drag scales by maximum plus one");
    Require(drag.Request(100, history) == 0, "same pointer position adds no movement");
    Require(drag.Request(99, history) == -2, "signed drag reversal truncates toward zero");

    ScrollBar nearEnd{126, 10, 231, 230, 9, 240};
    drag.Begin(0);
    Require(drag.Request(1, nearEnd) == 2, "drag requests more than remaining range");
    drag.Applied(230, 231);
    Require(drag.Request(0, nearEnd) == -1,
        "clamped drag residual tracks actual applied movement on reversal");
    drag.Applied(231, 230);
    Require(drag.Request(0, nearEnd) == 0, "reversal consumes actual movement residual");

    ScrollBar fullRail{50, 13, 1, 0, 1, 1};
    Require(fullRail.ThumbSize() == 24 && fullRail.Travel() == 0,
        "full-rail thumb has zero drag travel");
    drag.Begin(0);
    Require(drag.Request(100, fullRail) == 0, "zero-travel drag is inert");
    ScrollBar noRange{126, 10, 0, 0, 9, 240};
    Require(noRange.Hit(0, 0) == ScrollHit::None, "no-range scrollbar has no hit region");

    ScrollBar narrow{20, 13, 1, 0, 1, 1};
    Require(narrow.ThumbSize() == 8 && narrow.Travel() == -14,
        "undersized viewport preserves source minimum-thumb geometry");

    ScrollBar extremes{INT_MAX, 0, INT_MAX, 0, 0, 1};
    drag.Begin(INT_MIN);
    Require(drag.Request(INT_MAX, extremes) == INT_MAX,
        "extreme pointer displacement is bounded safely");
    Require(extremes.Hit(13, 0) == ScrollHit::None, "extreme axis retains fixed-width hit bounds");
    return 0;
}
