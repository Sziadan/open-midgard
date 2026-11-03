#include "UIGuildWnd.h"
#include "UIWindowMgr.h"
#include "network/Connection.h"
#include "session/Session.h"

UIGuildWnd::UIGuildWnd(){Create(720,470);m_id=UIWindowMgr::WID_GUILDWND;int x=40,y=65;LoadUiWindowPlacement("GuildWnd",&x,&y);g_windowMgr.ClampWindowToClient(&x,&y,m_w,m_h);Move(x,y);m_show=0;}
void UIGuildWnd::SetShow(int show){const bool opening=show && !m_show;UIWindow::SetShow(show);if(opening)Refresh();}
void UIGuildWnd::OnKeyDown(int key){if(key==VK_ESCAPE)SetShow(0);}
void UIGuildWnd::StoreInfo(){SaveUiWindowPlacement("GuildWnd",m_x,m_y);}
void UIGuildWnd::Refresh(){
    const char permissions[2]={0x4d,0x01};CRagConnection::instance()->SendPacket(permissions,2);
    if(!g_session.m_localGuild.guildId)return;
    for(unsigned char type=0;type<=4;++type){const char request[6]={0x4f,0x01,static_cast<char>(type),0,0,0};CRagConnection::instance()->SendPacket(request,6);}
}
