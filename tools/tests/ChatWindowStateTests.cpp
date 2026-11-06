#include "../../src/ui/ChatWindowState.h"

#include <cstdlib>
#include <iostream>
#include <climits>

int main()
{
    const auto require=[](bool condition) {
        if(!condition) {std::cerr << "chat state source literal mismatch\n"; std::exit(1);}
    };
    using ro::chat::HeightForState;
    using ro::chat::NormalizeState;
    using ro::chat::StateFromRequestedHeight;
    using ro::chat::CycleState;

    static_assert(NormalizeState(-2) == 15);
    static_assert(NormalizeState(-1) == 15);
    static_assert(NormalizeState(0) == 0);
    static_assert(NormalizeState(1) == 1);
    static_assert(NormalizeState(15) == 15);
    static_assert(NormalizeState(16) == 0);
    static_assert(NormalizeState(INT_MIN) == 15);
    static_assert(NormalizeState(INT_MAX) == 0);

    constexpr int expectedHeights[16] = {
        24, 96, 110, 124, 138, 152, 166, 180,
        194, 208, 222, 236, 250, 264, 278, 292,
    };
    for (int state = 0; state <= 15; ++state) {
        require(HeightForState(state) == expectedHeights[state]);
    }
    require(HeightForState(-1) == 292);
    require(HeightForState(INT_MIN) == 292);
    require(HeightForState(16) == 24);
    require(HeightForState(INT_MAX) == 24);

    constexpr int expectedIncrease[16] = {
        1, 4, 4, 7, 7, 7, 10, 10,
        10, 13, 13, 13, 0, 0, 0, 0,
    };
    constexpr int expectedDecrease[16] = {
        15, 0, 0, 0, 1, 1, 4, 4,
        4, 7, 7, 7, 10, 10, 10, 13,
    };
    for (int current = 0; current <= 15; ++current) {
        require(CycleState(current, false) == expectedIncrease[current]);
        require(CycleState(current, true) == expectedDecrease[current]);
    }

    require(StateFromRequestedHeight(71) == 0);
    require(StateFromRequestedHeight(72) == 1);
    require(StateFromRequestedHeight(85) == 1);
    require(StateFromRequestedHeight(86) == 2);
    require(StateFromRequestedHeight(99) == 2);
    require(StateFromRequestedHeight(100) == 3);
    require(StateFromRequestedHeight(267) == 14);
    require(StateFromRequestedHeight(268) == 15);
    require(StateFromRequestedHeight(281) == 15);
    require(StateFromRequestedHeight(282) == 15);
    require(StateFromRequestedHeight(INT_MAX) == 15);
    require(StateFromRequestedHeight(INT_MIN) == 0);
}
