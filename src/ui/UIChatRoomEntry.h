#pragma once

#include "UIWindow.h"
#include "core/ChatRoomPackets.h"
#include <string>

class CGameActor;
shopui::BitmapPixels LoadChatRoomSkinBitmap(const char* name);
std::string ChatRoomBitmapUri(const shopui::BitmapPixels& bitmap);

// Managed by the UI manager, with a non-owning actor link cleared on either
// teardown path. This keeps bulk UI deletion and actor deletion independent.
class UIChatRoomTitle : public UIWindow {
public:
    explicit UIChatRoomTitle(CGameActor* owner);
    ~UIChatRoomTitle() override;
    ro::chat::RoomInfo m_roomInfo;
    void SetInfo(const ro::chat::RoomInfo& info);
    std::string GetTitle() const;
    std::string GetDisplayTitle() const;
    const shopui::BitmapPixels& GetArtwork() const { return m_artwork; }
    const std::string& GetArtworkUri() const { return m_artworkUri; }
    void OnDraw() override;
    void OnLBtnDblClk(int x, int y) override;
    void OnMouseShape(int x, int y) override;
    void OnMouseMove(int x,int y) override { OnMouseShape(x,y); }
    void OnMouseHover(int x,int y) override { OnMouseShape(x,y); }
    bool CanGetFocus() override { return false; }
private:
    void LoadArtwork();
    CGameActor* m_owner;
    shopui::BitmapPixels m_artwork;
    std::string m_artworkUri;
};

class UIChatRoomPasswordWnd : public UIWindow {
public:
    UIChatRoomPasswordWnd();
    void SetRoomId(u32 roomId) { m_roomId=roomId; }
    u32 GetRoomId() const { return m_roomId; }
    const std::string& GetInputText() const { return m_input; }
    const std::string& GetPrompt() const { return m_prompt; }
    bool IsOkPressed() const { return m_pressedOk; }
    RECT GetOkBounds() const;
    const std::string& GetOkArtworkUri() const;
    int GetOkArtworkWidth() const;
    int GetOkArtworkHeight() const;
    void SetInputText(const std::string& text);
    bool SubmitInput();
    void OnDraw() override;
    void OnLBtnDown(int x,int y) override;
    void OnLBtnUp(int x,int y) override;
    void OnMouseMove(int x,int y) override;
    void OnMouseHover(int x,int y) override;
    void CancelPointerCapture();
    void OnChar(char c) override;
    void OnKeyDown(int key) override;
    bool CanReceiveKeyInput() const override { return true; }
    bool IsFrameWnd() override { return true; }
    msgresult_t SendMsg(UIWindow*,int,msgparam_t,msgparam_t,msgparam_t) override;
private:
    u32 m_roomId=0;
    std::string m_input,m_pendingUtf8,m_prompt;
    shopui::BitmapPixels m_okBitmaps[3];
    std::string m_okUris[3];
    bool m_dragging=false,m_pressedOk=false,m_hoverOk=false;
    int m_startX=0,m_startY=0,m_oldX=0,m_oldY=0;
};
