#pragma once
#include "UIFrameWnd.h"
#include "UIShopCommon.h"
#include "item/Item.h"

class UINotifyItemObtainWnd : public UIFrameWnd {
public:
    struct TextRun { std::string text; int x=0,y=0,width=0; u32 color=0xff000000u; };
    struct DisplayData {
        unsigned int itemId=0;
        bool identified=false;
        std::string sizingText,text;
        RECT iconRect{13,5,37,29};
        std::vector<TextRun> runs;
    };
    UINotifyItemObtainWnd();
    bool ShouldDoHitTest() override { return false; }
    void OnDraw() override;
    void OnProcess() override;
    void DrawNativeToHdc(HDC hdc);
    void SetItem(const ITEM_INFO& item,u32 now);
    void RefreshContent();
    void ProcessAtTick(u32 now);
    u32 GetMadeTick() const { return m_madeTick; }
    const ITEM_INFO& GetItemSnapshot() const { return m_item; }
    bool GetDisplayDataForQt(DisplayData* data) const;
private:
    void BuildRuns();
    ITEM_INFO m_item{};
    u32 m_madeTick=0;
    std::string m_sizingText;
    std::vector<ItemTextSegment> m_segments;
    std::vector<TextRun> m_runs;
    shopui::BitmapPixels m_icon;
};
