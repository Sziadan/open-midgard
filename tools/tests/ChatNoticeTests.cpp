#include "../../src/core/ChatNotice.h"
#include <cstdlib>
#include <iostream>

int main()
{
    const auto require = [](bool ok) {
        if (!ok) { std::cerr << "source notice literal mismatch\n"; std::exit(1); }
    };
    using namespace ro::chat;
    require(SplitNotice("").empty());
    require(SplitNotice("short") == std::vector<std::string>{"short"});
    require(SplitNotice(std::string(95, 'x')) == std::vector<std::string>{std::string(94, 'x'), "x"});
    require(SplitNotice(std::string(90, 'x') + " bbbbb")
        == std::vector<std::string>{std::string(90, 'x') + " ", "bbbbb"});
    require(SplitNotice(std::string(93, 'x') + "   y")
        == std::vector<std::string>{std::string(93, 'x') + "   ", "y"});
    std::string unicode;
    for (int i = 0; i < 31; ++i) unicode += "\xe4\xb8\x80";
    unicode += "\xc3\xa9";
    require(SplitNotice(unicode) == std::vector<std::string>{unicode});
    require(IsNoticeSentinel("NO MSG") && IsNoticeSentinel("No Msg"));
    require(!IsNoticeSentinel("No MSG") && !IsNoticeSentinel("no msg"));
    require(ColorRefToUiRgb(16776960) == 0x0000ffff && ColorRefToUiRgb(255) == 0x00ff0000);
    require(static_cast<int>(NoticeRoute::Normal) == 1 && static_cast<int>(NoticeRoute::Command) == 20);
    require(static_cast<int>(NoticeType::ObtainItem) == 5 && static_cast<int>(NoticeType::Equip) == 6);
}
