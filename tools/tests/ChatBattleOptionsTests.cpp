#include "../../src/core/ChatBattleOptions.h"
#include "../../src/core/UiMessageStrings.h"

#include <array>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {
void Require(bool ok, const char* name)
{
    if (!ok) {
        std::cerr << "ChatBattleOptionsTests failed: " << name << '\n';
        std::exit(1);
    }
}
}

int main()
{
    using namespace ro::chat;
    constexpr std::array<unsigned, 13> sourceMessageIds = {
        1277, 1278, 1279, 1280, 1281, 1282, 1283,
        1284, 1285, 1286, 1287, 1288, 1331
    };
    Require(BattleOptionMessageIds == sourceMessageIds, "13 source option message IDs");
    Require(BattleOptionTitleId == 1289, "source title message ID");

    BattleOptionsView view;
    Require(view.rows == 8 && view.LastFirst() == 6, "eight visible rows and last first index");
    Require(view.Hit(0, 17, 280) == 0, "top row starts at y17");
    Require(view.Hit(0, 176, 280) == 7, "bottom visible row includes y176");
    Require(view.Hit(0, 177, 280) == -1, "y177 is outside row area");
    Require(view.Hit(-1, 17, 280) == -1, "negative x is invalid");
    Require(view.Hit(280, 17, 280) == 1, "source x/width extra-index quirk");
    Require(view.Hit(0, 17, 0) == -1, "nonpositive width is invalid");

    Require(view.Scroll(INT_MIN) == 0 && view.first == 0, "large negative scroll clamps safely");
    Require(view.Scroll(INT_MAX) == 6 && view.first == 6, "large positive scroll clamps safely");
    Require(view.Hit(0, 137, 280) == 12, "last first index exposes item 12 at row six");
    Require(view.Hit(0, 157, 280) == -1, "last first index row seven is the blank row");
    Require(view.Scroll(1) == 0 && view.first == 6, "scroll stays at final endpoint");
    Require(view.Scroll(-7) == -6 && view.first == 0, "page-up delta clamps at start");
    Require(view.Scroll(7) == 6 && view.first == 6, "page-down delta reaches final index");
    Require(view.Scroll(-3) == -3 && view.first == 3, "wheel-up source delta");
    Require(view.Scroll(3) == 3 && view.first == 6, "wheel-down source delta");

    ro::UiMessageStrings strings;
    strings.Parse("\r\nalpha## spaced #unterminated");
    Require(strings.Get(0, "fallback") == "alpha", "leading CRLF is stripped from row");
    Require(strings.Get(1, "fallback").empty(), "empty positional row is preserved");
    Require(strings.Get(2, "fallback") == " spaced ", "row spaces are preserved");
    Require(strings.Get(3, "fallback") == "fallback", "unterminated tail is ignored and missing falls back");
    strings.Parse("#");
    Require(strings.Get(0, "fallback").empty(), "known empty row does not use fallback");
    Require(strings.Get(1, "fallback") == "fallback", "fallback is used only for missing row");
    return 0;
}
