#pragma once
#include "UIWindow.h"
#include "UISubChatWnd.h"
#include "UIChatRoomWnd.h"

struct WhisperWindowLayout {
    RECT history{},edit{},close{},resizer{},track{},thumb{},up{},down{};
};

class UIWhisperWnd : public UIWindow {
public:
    UIWhisperWnd(std::string name,std::string title);
    const std::string& GetName() const { return m_name; }
    const std::string& GetTitle() const { return m_title; }
    const std::string& GetInputText() const { return m_input; }
    const std::vector<SubChatRow>& GetRows() const { return m_rows; }
    const std::vector<std::string>& GetInputHistory() const { return m_inputHistory; }
    int GetFirst() const { return m_view.first; }
    int GetSelectedRow() const { return m_view.selected; }
    std::vector<SubChatRow> GetVisibleRows() const;
    WhisperWindowLayout GetLayout() const;
    ro::chat::ScrollBar GetScrollModel() const;
    void Append(const char* text,u32 uiRgb);
    void SetInputText(const std::string& text);
    bool SubmitInput();
    void ScrollHistory(int delta);
    void ResizeSource(int width,int height);
    void RestoreInfo();
    void StoreInfo() override;
    void OpenMenu(int x,int y);
    bool WriteChatToFile(bool report=false);
    const std::vector<RoomMenuItem>& GetMenuItems() const { return m_menuItems; }
    void CancelPointerCapture();
    bool IsFrameWnd() override { return true; }
    bool CanReceiveKeyInput() const override { return true; }
    void OnDraw() override;
    void OnLBtnDown(int x,int y) override;
    void OnMouseMove(int x,int y) override;
    void OnLBtnUp(int x,int y) override;
    void OnWheel(int delta) override;
    void OnRBtnUp(int x,int y) override { OpenMenu(x,y); }
    void OnChar(char c) override;
    void OnKeyDown(int key) override;
    msgresult_t SendMsg(UIWindow*,int,msgparam_t,msgparam_t,msgparam_t) override;
private:
    std::string m_name,m_title,m_input,m_pendingUtf8;
    std::vector<SubChatRow> m_rows;
    std::vector<std::string> m_inputHistory;
    std::vector<RoomMenuItem> m_menuItems;
    int m_historyBrowse=0,m_childWidth=280,m_childHeight=120;
    ro::chat::HistoryView m_view;
    ro::chat::ScrollDrag m_scrollDrag;
    bool m_dragging=false,m_resizing=false,m_scrollDragging=false,m_closePressed=false;
    int m_startX=0,m_startY=0,m_oldX=0,m_oldY=0,m_oldW=0,m_oldH=0;
};
