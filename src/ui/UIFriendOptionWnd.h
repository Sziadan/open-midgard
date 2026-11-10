#pragma once
#include "UIFrameWnd.h"
#include "UIChatRoomSettingsWnd.h"
#include "UISubChatWnd.h"

// Bitmap controls used by the source messenger/options family. Capture and
// logical press are distinct so leaving and re-entering retains the press.
class UIFriendOptionButton : public UIButton {
public:
    UIFriendOptionButton(int id,const char* normal,const char* hover,const char* pressed,int width,int height);
    const RoomSettingsButton& GetButton() const { return m_button; }
    int GetVisualState() const { return m_hover?(m_pressed?2:1):0; }
    void CancelPointerCapture();
    void OnDraw() override;
    void OnLBtnDown(int x,int y) override;
    void OnLBtnUp(int x,int y) override;
    void OnMouseMove(int x,int y) override;
    void OnMouseHover(int x,int y) override { OnMouseMove(x,y); }
private:
    RoomSettingsButton m_button;
    bool m_pressed=false,m_hover=false;
};

class UIFriendOptionCheckBox : public UIButton {
public:
    UIFriendOptionCheckBox(std::string label,bool checked,int maximumWidth=265);
    const std::string& GetLabel() const { return m_text; }
    bool IsChecked() const { return m_checked; }
    const shopui::BitmapPixels& GetBitmap() const { return m_checked?m_on:m_off; }
    const std::string& GetBitmapUri() const { return m_checked?m_onUri:m_offUri; }
    void OnLBtnDown(int,int) override;
    void OnLBtnUp(int,int) override {}
    void OnDraw() override;
private:
    bool m_checked;
    shopui::BitmapPixels m_on,m_off;
    std::string m_onUri,m_offUri;
};

class UIFriendOptionWnd : public UIFrameWnd {
public:
    UIFriendOptionWnd();
    const std::string& GetCaption() const { return m_caption; }
    UIFriendOptionCheckBox* GetCheckBox(int index) const { return index>=0 && index<3?m_rows[index]:nullptr; }
    UIFriendOptionButton* GetCloseButton() const { return m_close; }
    void Move(int x,int y) override;
    void StoreInfo() override;
    void CancelPointerCapture();
    void OnDraw() override;
    bool IsFrameWnd() override { return true; }
    msgresult_t SendMsg(UIWindow*,int,msgparam_t,msgparam_t,msgparam_t) override;
private:
    std::string m_caption;
    std::array<UIFriendOptionCheckBox*,3> m_rows{};
    UIFriendOptionButton* m_close;
    SubChatWindowInfo* m_info=nullptr;
};
