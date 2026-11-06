#pragma once
#include "UIWindow.h"
#include "core/ChatHistoryView.h"
#include "core/ChatBattleOptions.h"
#include "core/ChatScrollBar.h"

struct SubChatWindowInfo {
    int x = 0, y = 0, width = 0, height = 0;
};

struct SubChatRow {
    std::string text;
    u32 color;
    std::string talker;
};

struct SubChatHistoryLayout {
    RECT history{}, scrollTrack{}, scrollThumb{}, scrollUp{}, scrollDown{};
    int first = 0, total = 0, visible = 0;
};

// The source kind (0=ST, 1=BT) is deliberately separate from manager IDs137/138.
class UISubChatHisWnd : public UIWindow {
public:
    explicit UISubChatHisWnd(int kind);
    int GetKind() const { return m_kind; }
    const std::vector<SubChatRow>& GetRows() const { return m_rows; }
    const std::vector<SubChatRow>& GetVisualRows() const { return m_visualRows; }
    std::vector<SubChatRow> GetVisibleRows() const;
    SubChatHistoryLayout GetHistoryLayout() const;
    ro::chat::ScrollBar GetScrollModel() const;
    RECT GetBattleOptionButton() const { return m_kind==1 ? RECT{m_titleWidth+15,5,m_titleWidth+27,17} : RECT{}; }
    int GetSelectedRow() const { return m_view.selected; }
    void ScrollHistory(int delta);
    void Resize(int width, int height) override;
    void RestoreInfo();
    void StoreInfo() override;
    void Append(const char* text, u32 color, const char* talker);
    msgresult_t SendMsg(UIWindow*, int, msgparam_t, msgparam_t, msgparam_t) override;
    void OnDraw() override;
    void OnLBtnDown(int x, int y) override;
    void OnLBtnDblClk(int x, int y) override;
    void OnWheel(int delta) override;
    void OnMouseMove(int x, int y) override;
    void OnLBtnUp(int x, int y) override;
    void CancelPointerCapture();
    bool IsFrameWnd() override { return true; }
private:
    int m_kind;
    int m_titleWidth=71;
    std::vector<SubChatRow> m_rows, m_visualRows;
    ro::chat::HistoryView m_view;
    void AppendVisual(const SubChatRow& row);
    void RebuildVisual();
    bool m_scrollDragging = false;
    ro::chat::ScrollDrag m_scrollDrag;
    bool m_dragging = false, m_resizing = false;
    int m_startX = 0, m_startY = 0, m_oldX = 0, m_oldY = 0, m_oldW = 0, m_oldH = 0;
};

class UISubChatMiniWnd : public UIWindow {
public:
    UISubChatMiniWnd();
    msgresult_t SendMsg(UIWindow*, int, msgparam_t, msgparam_t, msgparam_t) override;
    void OnLBtnDown(int x, int y) override;
    void OnDraw() override;
    void UpdateHistoryVisibility();
};

struct BattleOptionsLayout {
    RECT base{}, close{}, scrollTrack{}, scrollThumb{}, scrollUp{}, scrollDown{};
    int first=0, selected=-1, rows=8;
};
class UIBattleMsgOptionWnd : public UIWindow {
public:
    UIBattleMsgOptionWnd();
    const std::string& GetTitle() const { return m_title; }
    const std::string& GetLabel(int index) const { return m_labels.at(static_cast<std::size_t>(index)); }
    BattleOptionsLayout GetLayout() const;
    ro::chat::ScrollBar GetScrollModel() const;
    int GetSelectedRow() const { return m_view.selected; }
    int GetFirstRow() const { return m_view.first; }
    void ScrollRows(int delta);
    msgresult_t SendMsg(UIWindow*,int,msgparam_t,msgparam_t,msgparam_t) override;
    void OnDraw() override;
    void OnLBtnDown(int x,int y) override;
    void OnMouseMove(int x,int y) override;
    void OnLBtnUp(int,int) override;
    void OnWheel(int delta) override;
    void CancelPointerCapture();
    bool IsFrameWnd() override { return true; }
private:
    ro::chat::BattleOptionsView m_view;
    std::array<std::string,13> m_labels;
    std::string m_title;
    bool m_dragging=false,m_scrollDragging=false;
    int m_grabX=0,m_grabY=0;
    ro::chat::ScrollDrag m_scrollDrag;
};
