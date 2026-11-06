#pragma once
#include "UIWindow.h"
#include "UISubChatWnd.h"
#include "core/ChatRoomPackets.h"
#include "UIChatRoomEntry.h"
#include "core/ChatHistoryView.h"
#include "core/ChatScrollBar.h"
#include <map>

struct ChatRoomLayout {
    RECT history{},members{},edit{},close{},resizer{};
    RECT scrollTrack{},scrollThumb{},scrollUp{},scrollDown{};
    RECT memberTrack{},memberThumb{},memberUp{},memberDown{};
};
struct RoomMenuItem { int action=0;std::string label; };

class UIChatRoomWnd : public UIWindow {
public:
    UIChatRoomWnd();
    const ro::chat::RoomInfo& GetInfo() const { return m_info; }
    void SetInfo(const ro::chat::RoomInfo& info);
    void ClearMembers();
    void AddMember(const std::string& name,u32 role);
    void RemoveMember(const std::string& name);
    void ChangeRole(const std::string& name,u32 role);
    const std::map<std::string,u32>& GetMembers() const { return m_members; }
    const std::vector<ro::chat::RoomMember>& GetMemberRows() const { return m_memberRows; }
    int GetMemberFirst() const { return m_memberView.first; }
    int GetSelectedMember() const { return m_memberView.selected; }
    const std::vector<SubChatRow>& GetRows() const { return m_rows; }
    std::vector<SubChatRow> GetVisibleRows() const;
    int GetFirst() const { return m_view.first; }
    void Append(const char* text,u32 uiRgb);
    void ScrollHistory(int delta);
    void ResizeSource(int width,int height);
    void RestoreInfo();
    void StoreInfo() override;
    ChatRoomLayout GetLayout() const;
    ro::chat::ScrollBar GetScrollModel(bool members=false) const;
    std::string GetTitle() const;
    const std::string& GetInputText() const { return m_input; }
    void SetInputText(const std::string& text);
    bool SubmitInput();
    void OpenMemberMenu(int x,int y);
    const std::vector<RoomMenuItem>& GetMenuItems() const { return m_menuItems; }
    bool WriteChatToFile();
    bool IsFrameWnd() override { return true; }
    bool CanReceiveKeyInput() const override { return true; }
    void OnDraw() override;
    void OnLBtnDown(int x,int y) override;
    void OnMouseMove(int x,int y) override;
    void OnLBtnUp(int,int) override;
    void OnChar(char value) override;
    void OnKeyDown(int key) override;
    void OnWheel(int delta) override;
    void OnRBtnDown(int x,int y) override;
    void OnRBtnUp(int x,int y) override;
    void CancelPointerCapture();
    msgresult_t SendMsg(UIWindow*,int,msgparam_t,msgparam_t,msgparam_t) override;
private:
    void RebuildMembers();
    int m_childWidth=280,m_childHeight=120;
    ro::chat::RoomInfo m_info;
    std::vector<SubChatRow> m_rows;
    std::map<std::string,u32> m_members;
    std::vector<ro::chat::RoomMember> m_memberRows;
    std::vector<RoomMenuItem> m_menuItems;
    ro::chat::HistoryView m_view,m_memberView;
    std::string m_input,m_publicLabel,m_privateLabel,m_pendingUtf8;
    bool m_dragging=false,m_resizing=false,m_scrollDragging=false,m_scrollMembers=false;
    bool m_memberRightCapture=false;
    int m_startX=0,m_startY=0,m_oldX=0,m_oldY=0,m_oldW=0,m_oldH=0;
    ro::chat::ScrollDrag m_scrollDrag;
};

// Source menu18 has a conflicting port NPC owner ID. Keep a dedicated owner;
// logical entries are paired 7px source rows (14px per action), without separators.
class UIChatRoomMenuWnd : public UIWindow {
public:
    UIChatRoomMenuWnd(const std::vector<RoomMenuItem>& items,int x,int y);
    const std::vector<RoomMenuItem>& GetItems() const { return m_items; }
    int GetHoverIndex() const { return m_hover; }
    void OnDraw() override;
    void OnLBtnDown(int,int) override {}
    void OnLBtnUp(int x,int y) override;
    void OnMouseMove(int x,int y) override;
    void OnMouseHover(int x,int y) override { OnMouseMove(x,y); }
private:
    std::vector<RoomMenuItem> m_items;
    int m_hover=-1;
};
