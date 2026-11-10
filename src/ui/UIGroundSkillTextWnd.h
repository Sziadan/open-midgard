#pragma once

#include "UIChatRoomSettingsWnd.h"
#include <array>
#include <string>

class UIEditCtrl;

// Text prompt used by the two source ground-skill windows (IDs 84 and 101).
class UIGroundSkillTextWnd : public UIWindow {
public:
    explicit UIGroundSkillTextWnd(int windowId);
    ~UIGroundSkillTextWnd() override = default;

    const std::array<int,4>& GetValues() const { return m_values; }
    UIEditCtrl* GetEditCtrl() const { return m_editCtrl; }
    const std::string& GetCaption() const { return m_caption; }
    const std::string& GetPrompt() const { return m_prompt; }
    const std::string& GetFrameUri(int index) const { return m_frameUris.at(static_cast<std::size_t>(index)); }
    const shopui::BitmapPixels& GetFrameBitmap(int index) const { return m_frame.at(static_cast<std::size_t>(index)); }
    const shopui::BitmapPixels& GetColorChip() const { return m_colorChip; }
    u32 GetColorChipRgb(int x,int y) const;
    bool IsFrameDragging() const { return m_dragging; }
    const RoomSettingsButton* GetButton(int id) const;
    int GetButtonState(int id) const;

    void Move(int x,int y) override;
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
    friend class GroundSkillTextEditCtrl;
    void FocusEdit();
    void ActivateButton(int id);
    void CancelPointerCapture();

    std::array<int,4> m_values{};
    UIEditCtrl* m_editCtrl=nullptr; // Owned by UIWindow::m_children.
    RoomSettingsButton m_ok;
    RoomSettingsButton m_cancel;
    std::array<shopui::BitmapPixels,6> m_frame;
    std::array<std::string,6> m_frameUris;
    shopui::BitmapPixels m_colorChip;
    bool m_dragging=false;
    int m_dragStartX=0,m_dragStartY=0,m_dragOriginX=0,m_dragOriginY=0;
    std::string m_caption;
    std::string m_prompt;
    int m_pressed=0;
    int m_hover=0;
};
