#pragma once
#include "UIChatRoomSettingsWnd.h"

class UIAutoMessageWnd : public UIWindow {
public:
    UIAutoMessageWnd();
    const std::string& GetPrompt() const { return m_prompt; }
    const std::string& GetInputText() const { return m_input; }
    bool IsSelected() const { return m_selected; }
    const RoomSettingsButton& GetButton() const { return m_ok; }
    int GetButtonState() const { return m_pressed?2:m_hover?1:0; }
    void SetInputText(const std::string& text);
    void FocusForDraw();
    void SubmitInput();
    void CancelInput();
    void CancelPointerCapture();
    void OnDraw() override;
    void OnLBtnDown(int x,int y) override;
    void OnLBtnUp(int x,int y) override;
    void OnMouseMove(int x,int y) override;
    void OnMouseHover(int x,int y) override;
    void OnChar(char c) override;
    void OnKeyDown(int key) override;
    bool CanReceiveKeyInput() const override { return true; }
    bool IsFrameWnd() override { return true; }
    msgresult_t SendMsg(UIWindow*,int,msgparam_t,msgparam_t,msgparam_t) override;
private:
    std::string m_prompt,m_input,m_pendingUtf8;
    RoomSettingsButton m_ok;
    bool m_selected=true,m_pressed=false,m_hover=false,m_dragging=false;
    int m_startX=0,m_startY=0,m_oldX=0,m_oldY=0;
};
