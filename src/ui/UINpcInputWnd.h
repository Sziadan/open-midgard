#pragma once

#include "UIFrameWnd.h"

#include <string>

class UIEditCtrl;

class UINpcInputWnd : public UIFrameWnd {
public:
    enum class InputMode {
        Number = 0,
        String,
    };

    UINpcInputWnd();
    ~UINpcInputWnd() override;

    void SetShow(int show) override;
    void Move(int x, int y) override;
    void StoreInfo() override;
    void OnDraw() override;
    void OnLBtnDown(int x, int y) override;
    void OnMouseMove(int x, int y) override;
    void OnLBtnUp(int x, int y) override;

    void OpenNumber(u32 npcId);
    void OpenString(u32 npcId);
    void OpenGameNumberPrompt(const char* label, int gameMessage, msgparam_t wparam, u32 initialValue, u32 maxValue);
    void HideInput();
    bool HandleKeyDown(int virtualKey);
    UIEditCtrl* GetEditCtrl() const;
    u32 GetNpcId() const;
    InputMode GetInputMode() const;
    const char* GetInputText() const;
    const char* GetDialogLabel() const;
    bool IsOkPressed() const;
    bool IsCancelPressed() const;
    bool GetOkRectForQt(RECT* outRect) const;
    bool GetCancelRectForQt(RECT* outRect) const;

private:
    enum class SubmitAction {
        None = 0,
        NpcNumber,
        NpcString,
        GameMessage,
    };

    enum class ClickTarget {
        None = 0,
        Ok,
        Cancel,
    };

    void LayoutControls();
    RECT GetOkRect() const;
    RECT GetCancelRect() const;
    bool IsPointInRect(const RECT& rect, int x, int y) const;
    void StartDragging(int x, int y);
    void StopDragging();
    bool SubmitCurrentText();
    void CancelInput();
    void DrawButton(HDC hdc, const RECT& rect, const char* label, bool pressed) const;
    void OpenForMode(u32 npcId, InputMode mode, SubmitAction action, const char* label);

    u32 m_npcId;
    InputMode m_mode;
    UIEditCtrl* m_editCtrl;
    ClickTarget m_pressedTarget;
    SubmitAction m_submitAction;
    int m_submitGameMessage;
    msgparam_t m_submitWparam;
    u32 m_maxNumberValue;
    std::string m_dialogLabel;
};
