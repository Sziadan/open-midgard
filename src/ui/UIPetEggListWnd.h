#pragma once
#include "UIFriendOptionWnd.h"
#include "core/ChatScrollBar.h"
#include "item/Item.h"
#include <array>
#include <map>

class UIPetEggListWnd;
class UIPetEggScrollBar : public UIWindow {
public:
    explicit UIPetEggScrollBar(UIPetEggListWnd* owner);
    void OnDraw() override;
    void OnLBtnDown(int x,int y) override;
    void OnLBtnUp(int,int) override;
    void OnMouseMove(int x,int y) override;
    void OnWheel(int delta) override;
    void CancelPointerCapture();
private:
    UIPetEggListWnd* m_owner;
    bool m_dragging=false;
    ro::chat::ScrollDrag m_drag;
};
struct PetEggArtwork {
    int x=0,y=0,phase=0;
    shopui::BitmapPixels bitmap;
    std::string uri;
};
struct PetEggDisplayRow {
    int index=0,y=0;
    unsigned int itemIndex=0,itemId=0;
    bool selected=false;
    std::string text,artwork;
    shopui::BitmapPixels bitmap;
};
class UIPetEggListWnd : public UIFrameWnd {
public:
    UIPetEggListWnd();
    void Move(int x,int y) override;
    void OnDraw() override;
    void OnLBtnDown(int x,int y) override;
    void OnLBtnDblClk(int x,int y) override;
    void OnWheel(int delta) override;
    msgresult_t SendMsg(UIWindow*,int,msgparam_t,msgparam_t,msgparam_t) override;
    int GetCurrentItem() const { return m_current; }
    int GetOffset() const { return m_offset; }
    int GetCurItem(int localX,int localY) const;
    const std::vector<ITEM_INFO>& GetItems() const { return m_items; }
    std::string GetTitle() const;
    std::vector<PetEggDisplayRow> GetVisibleRows();
    std::vector<PetEggArtwork> GetFramePieces() const;
    const std::array<UIFriendOptionButton*,2>& GetButtons() const { return m_buttons; }
    UIPetEggScrollBar* GetScrollControl() const { return m_scroll; }
    ro::chat::ScrollBar GetScrollBar() const;
    int ScrollBy(int delta);
    void CancelPointerCapture();
private:
    void SubmitSelection();
    int m_current=-1,m_offset=0;
    std::vector<ITEM_INFO> m_items;
    UIPetEggScrollBar* m_scroll;
    std::array<UIFriendOptionButton*,2> m_buttons{};
    std::array<shopui::BitmapPixels,6> m_frame;
    std::array<std::string,6> m_frameUris;
    std::map<unsigned long long,std::pair<shopui::BitmapPixels,std::string>> m_icons;
};
