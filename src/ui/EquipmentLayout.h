#pragma once
#include <algorithm>
#include <array>

namespace equipmentLayout {
inline constexpr int width = 624;
inline constexpr int miniHeight = 17;
inline constexpr int cardWidth = 236;
inline constexpr int iconSize = 32;
inline constexpr int nameWidth = 178;
inline constexpr int leftX = 12;
inline constexpr int rightX = 376;
inline constexpr int centerX = 260;
inline constexpr int centerWidth = 104;
inline constexpr std::array<int, 11> rows{{0,1,3,4,0,1,2,3,4,2,5}};
inline constexpr std::array<bool, 11> left{{true,true,true,true,false,false,false,false,false,true,false}};
inline constexpr std::array<const char*, 11> labels{{"Upper headgear", "Lower headgear", "Garment", "Accessory 1",
    "Middle headgear", "Armor", "Weapon / Shield", "Shoes", "Accessory 2", "Weapon", "Ammunition"}};
struct Rect { int x=0,y=0,width=0,height=0; };
struct Layout {
    int width=equipmentLayout::width;
    int height=0;
    std::array<Rect,11> cards{},icons{},names{};
    Rect preview,removeOption;
};
inline Layout Build(const std::array<int,11>& nameHeights = {}, bool compact=false) {
    Layout result;
    if(compact) {
        result.width=256;
        result.height=280;
        for(int i=0;i<11;++i){
            const int x=left[i]?12:208,y=32+40*rows[i];
            result.cards[i]={x,y,36,36};
            result.icons[i]={x+2,y+2,iconSize,iconSize};
        }
        result.preview={76,32,104,160};
        result.removeOption={76,202,104,24};
        return result;
    }
    std::array<int,6> heights{{52,52,52,52,52,52}};
    for(int i=0;i<11;++i)heights[rows[i]]=(std::max)(heights[rows[i]],nameHeights[i]+26);
    std::array<int,6> tops{};
    int y=40;
    for(int row=0;row<6;++row){tops[row]=y;y+=heights[row]+6;}
    result.height=y+6;
    for(int i=0;i<11;++i){
        const int x=left[i]?leftX:rightX;
        const int top=tops[rows[i]],height=heights[rows[i]];
        result.cards[i]={x,top,cardWidth,height};
        result.icons[i]={left[i]?x+8:x+cardWidth-40,top+(height-iconSize)/2,iconSize,iconSize};
        result.names[i]={left[i]?x+50:x+8,top+20,nameWidth,height-26};
    }
    const int centerBottom=tops[5]-6;
    result.preview={centerX,40,centerWidth,centerBottom-40-34};
    result.removeOption={centerX,centerBottom-24,centerWidth,24};
    return result;
}
}
