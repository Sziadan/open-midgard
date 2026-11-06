#pragma once
#include <algorithm>
#include <array>

namespace ro::chat {
// Selected PE constructor0x4A0630: siege is not contiguous with the first12.
inline constexpr std::array<unsigned,13> BattleOptionMessageIds = {
    1277,1278,1279,1280,1281,1282,1283,1284,1285,1286,1287,1288,1331
};
inline constexpr unsigned BattleOptionTitleId = 1289;
struct BattleOptionsView {
    int first=0, selected=-1, rows=8;
    int LastFirst() const { return (std::max)(0,14-rows); } // source includes one blank row
    int Scroll(int delta) {
        const int old=first;
        first=static_cast<int>(std::clamp(static_cast<long long>(first)+delta,0LL,static_cast<long long>(LastFirst())));
        return first-old;
    }
    int Hit(int x,int y,int width) const {
        if (width<=0 || x<0 || y<17 || y>=17+20*rows) return -1;
        const long long index=static_cast<long long>(first)+x/width+(y-17)/20;
        return index<13 ? static_cast<int>(index) : -1;
    }
};
} // namespace ro::chat
