#pragma once
#include "UIFriendOptionWnd.h"
#include "core/ChatScrollBar.h"

struct TipTextSpan {std::string text;u32 color=0;};
using TipTextRow=std::vector<TipTextSpan>;

class UITipTextViewer : public UIWindow {
public:
    UITipTextViewer();
    void SetLines(const std::vector<std::string>& lines);
    const std::vector<std::string>& GetItems() const {return m_items;}
    const std::vector<TipTextRow>& GetRows() const {return m_rows;}
    int GetOffset() const {return m_offset;}
    ro::chat::ScrollBar GetScrollBar() const;
    void CancelPointerCapture();
    void OnDraw() override;
    void OnLBtnDown(int x,int y) override;
    void OnLBtnUp(int,int) override {m_scrollPressed=false;}
    void OnMouseMove(int x,int y) override;
    void OnWheel(int delta) override;
    msgresult_t SendMsg(UIWindow*,int,msgparam_t,msgparam_t,msgparam_t) override;
private:
    int Scroll(int delta);
    std::vector<std::string> m_items;
    std::vector<TipTextRow> m_rows;
    int m_offset=0;
    bool m_scrollPressed=false;
    ro::chat::ScrollDrag m_drag;
};

class UITipOfTheDayWnd : public UIFrameWnd {
public:
    UITipOfTheDayWnd();
    UITipTextViewer* GetViewer() const {return m_viewer;}
    UIFriendOptionCheckBox* GetStartupCheckBox() const {return m_startup;}
    const std::array<UIFriendOptionButton*,3>& GetButtons() const {return m_buttons;}
    int GetTipIndex() const {return m_index;}
    void OnLBtnDown(int x,int y) override;
    void OnKeyDown(int key) override;
    bool CanReceiveKeyInput() const override {return true;}
    void Move(int x,int y) override;
    void CancelPointerCapture();
    void OnDraw() override;
    msgresult_t SendMsg(UIWindow*,int,msgparam_t,msgparam_t,msgparam_t) override;
private:
    void ShowCurrentTip();
    UITipTextViewer* m_viewer;
    UIFriendOptionCheckBox* m_startup;
    std::array<UIFriendOptionButton*,3> m_buttons{};
    int m_index=0;
};
