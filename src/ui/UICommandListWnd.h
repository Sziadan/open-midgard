#pragma once
#include "UIFriendOptionWnd.h"
#include "core/ChatScrollBar.h"
#include "core/UiMessageStrings.h"
#include <array>

class UICommandListWnd;
class UICommandListTab : public UIWindow {
public:
    UICommandListTab();
    const shopui::BitmapPixels& GetBitmap() const;
    std::string GetBitmapUri() const;
    void OnDraw() override;
    void OnLBtnDown(int x,int y) override;
    msgresult_t SendMsg(UIWindow*,int,msgparam_t,msgparam_t,msgparam_t) override;
private:
    std::array<shopui::BitmapPixels,3> m_bitmaps;
    std::array<std::string,3> m_uris;
};
class UICommandListScrollBar : public UIWindow {
public:
    explicit UICommandListScrollBar(UICommandListWnd* owner);
    void CancelPointerCapture();
    void OnDraw() override;
    void OnLBtnDown(int x,int y) override;
    void OnLBtnUp(int,int) override;
    void OnMouseMove(int x,int y) override;
    void OnWheel(int delta) override;
private:
    UICommandListWnd* m_owner;
    bool m_dragging=false;
    ro::chat::ScrollDrag m_drag;
};
struct CommandListDisplayRow {
    int index=0,value=0,x=0,y=0,width=0,height=20,textOffset=15;
    std::string caption;
    bool selected=false,hasIndicator=false,on=false;
};
struct CommandListFramePiece {
    int x=0,y=0,width=0,height=0,phase=0;
    shopui::BitmapPixels bitmap;
    std::string uri;
};
class UICommandListWnd : public UIFrameWnd {
public:
    UICommandListWnd();
    UICommandListTab* GetTab() const {return m_tab;}
    UICommandListScrollBar* GetScrollControl() const {return m_scroll;}
    const std::array<UIFriendOptionButton*,2>& GetButtons() const {return m_buttons;}
    int GetCurrentItem() const {return m_currentItem;}
    int GetOffset() const {return m_offset;}
    int GetVisibleRowCount() const {return m_visibleRows;}
    int GetCurItem(int localX,int localY) const;
    std::string GetTitle() const;
    std::vector<CommandListDisplayRow> GetVisibleRows() const;
    std::vector<CommandListFramePiece> GetFramePieces() const;
    const shopui::BitmapPixels& GetIndicatorBitmap(bool on) const {return m_indicators[on?1:0];}
    const std::string& GetIndicatorUri(bool on) const {return m_indicatorUris[on?1:0];}
    ro::chat::ScrollBar GetScrollBar() const;
    void CancelPointerCapture();
    void Move(int x,int y) override;
    void OnDraw() override;
    void OnLBtnDown(int x,int y) override;
    void OnMouseHover(int x,int y) override;
    void OnMouseMove(int x,int y) override;
    void OnWheel(int delta) override;
    void OnKeyDown(int key) override;
    bool CanReceiveKeyInput() const override {return true;}
    msgresult_t SendMsg(UIWindow*,int,msgparam_t,msgparam_t,msgparam_t) override;
private:
    int Scroll(msgparam_t delta);
    std::string HoverHelp(int index) const;
    UICommandListTab* m_tab;
    UICommandListScrollBar* m_scroll;
    std::array<UIFriendOptionButton*,2> m_buttons{};
    std::array<shopui::BitmapPixels,2> m_indicators;
    std::array<std::string,2> m_indicatorUris;
    std::array<shopui::BitmapPixels,8> m_frame;
    std::array<std::string,8> m_frameUris;
    int m_currentItem=-1,m_offset=0,m_visibleRows=8;
    mutable ro::UiMessageStrings m_helpStrings;
    mutable int m_helpService=-1,m_helpCodePage=-1,m_helpFolder=-1;
};
