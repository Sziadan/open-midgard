#pragma once
#include "UIWindow.h"

struct SubChatWindowInfo {
    int x = 0, y = 0, width = 0, height = 0;
};

struct SubChatRow {
    std::string text;
    u32 color;
    std::string talker;
};

// The source kind (0=ST, 1=BT) is deliberately separate from manager IDs137/138.
class UISubChatHisWnd : public UIWindow {
public:
    explicit UISubChatHisWnd(int kind);
    int GetKind() const { return m_kind; }
    const std::vector<SubChatRow>& GetRows() const { return m_rows; }
    void RestoreInfo();
    void StoreInfo() override;
    void Append(const char* text, u32 color, const char* talker);
    msgresult_t SendMsg(UIWindow*, int, msgparam_t, msgparam_t, msgparam_t) override;
    void OnDraw() override;
    void OnLBtnDown(int x, int y) override;
    void OnMouseMove(int x, int y) override;
    void OnLBtnUp(int x, int y) override;
    void CancelPointerCapture();
    bool IsFrameWnd() override { return true; }
private:
    int m_kind;
    std::vector<SubChatRow> m_rows;
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
