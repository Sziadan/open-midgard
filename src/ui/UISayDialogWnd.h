#pragma once

#include "UIFrameWnd.h"

#include <string>
#include <vector>

class UISayDialogWnd : public UIFrameWnd {
public:
    UISayDialogWnd();
    ~UISayDialogWnd() override;

    void SetShow(int show) override;
    void Move(int x, int y) override;
    void StoreInfo() override;
    void OnDraw() override;
    void OnLBtnDown(int x, int y) override;
    void OnMouseMove(int x, int y) override;
    void OnLBtnUp(int x, int y) override;

    void AppendText(u32 npcId, const std::string& text);
    void ShowNext(u32 npcId);
    void ShowClose(u32 npcId);
    void ClearAction();
    void HideConversation();
    bool HandleKeyDown(int virtualKey);
    u32 GetNpcId() const;
    std::string GetDisplayText() const;
    bool HasActionButton() const;
    bool IsNextAction() const;
    bool IsHoveringAction() const;
    bool IsPressingAction() const;
    bool GetActionRectForQt(RECT* outRect) const;

private:
    void SubmitAction();
    enum class ActionButton {
        None = 0,
        Next,
        Close,
    };

    RECT GetActionRect() const;
    RECT GetTextRect() const;
    bool IsPointInRect(const RECT& rect, int x, int y) const;
    void StartDragging(int x, int y);
    void StopDragging();
    void DrawActionButton(HDC hdc, const RECT& rect) const;
    std::string BuildDisplayText() const;

    u32 m_npcId;
    std::vector<std::string> m_textLines;
    ActionButton m_actionButton;
    bool m_clearOnNextText;
    bool m_hoverAction;
    bool m_pressAction;
};
