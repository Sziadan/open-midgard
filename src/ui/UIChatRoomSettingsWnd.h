#pragma once
#include <functional>
#include "UIChatRoomEntry.h"
#include <array>

struct RoomSettingsButton {
    int id=0,x=0,y=0;
    std::array<shopui::BitmapPixels,3> bitmaps;
    std::array<std::string,3> uris;
    RECT Bounds() const;
};

class UIChatRoomSettingsWnd : public UIWindow {
public:
    explicit UIChatRoomSettingsWnd(bool change);
    bool IsChange() const { return m_change; }
    const std::string& GetCaption() const { return m_caption; }
    const std::string& GetTitleText() const { return m_title; }
    const std::string& GetPasswordText() const { return m_password; }
    const std::string& GetPrivacyLabel(bool privacy) const { return privacy?m_privateLabel:m_publicLabel; }
    const std::string& GetClassLabel() const { return m_classLabel; }
    std::string GetPeopleLabel(int index=-1) const;
    const std::vector<RoomSettingsButton>& GetButtons() const { return m_buttons; }
    const std::string& GetFrameUri(bool title) const { return title?m_titlebarUri:m_backgroundUri; }
    const RoomSettingsButton* GetButton(int id) const;
    int GetPressedButton() const { return m_pressed; }
    int GetHoveredButton() const { return m_hover; }
    int GetPeople() const { return m_people; }
    bool IsPrivate() const { return m_private; }
    bool IsPasswordFocused() const { return m_passwordFocused; }
    bool HasSelectedTitle() const { return m_selectTitle; }
    int GetPopupKind() const { return m_popup; }
    int GetPopupFirst() const { return m_popupFirst; }
    RECT GetPopupBounds() const;
    void ChoosePeople(int index);
    void SetTitleText(const std::string& text);
    void SetPasswordText(const std::string& text);
    void SetPrivate(bool privacy);
    void SetInfo(const ro::chat::RoomInfo& info);
    bool SubmitInput();
    void CancelPointerCapture();
    void OnDraw() override;
    void OnLBtnDown(int x,int y) override;
    void OnLBtnUp(int x,int y) override;
    void OnMouseMove(int x,int y) override;
    void OnMouseHover(int x,int y) override;
    void OnWheel(int delta) override;
    void OnChar(char c) override;
    void OnKeyDown(int key) override;
    bool CanReceiveKeyInput() const override { return true; }
    bool IsFrameWnd() override { return true; }
    msgresult_t SendMsg(UIWindow*,int,msgparam_t,msgparam_t,msgparam_t) override;
private:
    void Activate(int id);
    bool m_change=false,m_private=false,m_passwordFocused=false,m_selectTitle=false,m_dragging=false;
    int m_people=20,m_displayPeople=5,m_popup=-1,m_popupFirst=0,m_pressed=0,m_hover=0,m_popupPressed=-1;
    int m_startX=0,m_startY=0,m_oldX=0,m_oldY=0;
    std::string m_caption,m_title,m_password,m_suffix,m_classLabel,m_publicLabel,m_privateLabel,m_pendingUtf8;
    shopui::BitmapPixels m_titlebar,m_background;
    std::string m_titlebarUri,m_backgroundUri;
    std::vector<RoomSettingsButton> m_buttons;
};

// Source ErrorMsg type0 used by room validation. Asynchronous ownership avoids
// a nested mode-update loop; the room draft stays intact until resubmission.
class UIChatRoomMessageBox : public UIWindow {
public:
    explicit UIChatRoomMessageBox(std::string text,int messageId);
    const std::string& GetText() const { return m_text; }
    int GetMessageId() const { return m_messageId; }
    const RoomSettingsButton& GetButton() const { return m_ok; }
    bool IsPressed() const { return m_pressed; }
    void Dismiss(bool accepted=true);
    void ConfigureConfirmation(UIWindow* owner,std::function<void(bool)> completion);
    UIWindow* GetSourceOwner() const {return m_sourceOwner;}
    void SetSourceOwner(UIWindow* owner){m_sourceOwner=owner;}
    bool HasCancel() const {return static_cast<bool>(m_completion);}
    const RoomSettingsButton& GetCancelButton() const {return m_cancel;}
    bool IsCancelPressed() const {return m_cancelPressed;}
    void CancelPointerCapture();
    void OnDraw() override;
    void OnLBtnDown(int x,int y) override;
    void OnLBtnUp(int x,int y) override;
    void OnMouseMove(int x,int y) override;
    void OnKeyDown(int key) override;
    bool IsFrameWnd() override { return true; }
    bool CanReceiveKeyInput() const override { return true; }
private:
    std::string m_text;
    int m_messageId;
    UIWindow* m_previousEdit;
    RoomSettingsButton m_ok;
    RoomSettingsButton m_cancel;
    UIWindow* m_sourceOwner=nullptr;
    std::function<void(bool)> m_completion;
    bool m_cancelPressed=false;
    bool m_pressed=false,m_dragging=false;
    int m_startX=0,m_startY=0,m_oldX=0,m_oldY=0;
};
