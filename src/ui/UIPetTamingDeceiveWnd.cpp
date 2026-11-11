#include "UIPetTamingDeceiveWnd.h"
#include "UIWindowMgr.h"
#include "UIChatRoomEntry.h"
#include "core/Globals.h"
#include "gamemode/Mode.h"
#include "network/Connection.h"
#include "res/Res.h"
#include "res/ActRes.h"
#include "res/Sprite.h"
#include <algorithm>
#include <limits>

UIPetTamingDeceiveWnd::UIPetTamingDeceiveWnd()
{
    m_id=UIWindowMgr::WID_PETTAMINGDECEIVEWND;m_w=196;m_h=256;
    const auto client=g_windowMgr.GetChatClientRect();
    Move(client.right/2-98,client.bottom/2-128);
}

msgresult_t UIPetTamingDeceiveWnd::SendMsg(UIWindow*,int message,msgparam_t value1,msgparam_t value2,msgparam_t)
{
    if(message==80){
        if(!value1)m_monGid=static_cast<u32>(value2);
        else{m_destination=static_cast<int>(value2);m_delay=m_maxDelay=7;}
    }
    return 0;
}

void UIPetTamingDeceiveWnd::OnLBtnDown(int,int)
{
    if(m_selected || CRagConnection::instance()->IsWaitingForPetCaptureResult())return;
    m_action=1;m_destination=7;m_selected=true;
    m_awaitingResult = g_modeMgr.SendMsg(162,m_monGid,0,0) != 0;
}

void UIPetTamingDeceiveWnd::ApplyCaptureResult(bool captured)
{
    if (!m_awaitingResult) return;
    m_awaitingResult = false;
    SendMsg(nullptr, 80, 1, captured ? 7 : 3, 0);
}

void UIPetTamingDeceiveWnd::ProcessAt(u32 now)
{
    Invalidate();
    const bool stopped=m_motionTick/m_delay==m_destination && m_delay<=m_maxDelay;
    if(m_startTick==0xffffffffu){if(stopped)m_startTick=now;}
    else if(stopped && static_cast<u32>(now-m_startTick)>=2000)g_windowMgr.DeleteWindow(this);
}
void UIPetTamingDeceiveWnd::OnProcess(){ProcessAt(timeGetTime());}

void UIPetTamingDeceiveWnd::PrepareArtwork(int frame)
{
    auto* act=g_resMgr.GetAs<CActRes>("SlotMachine.act");
    auto* spr=g_resMgr.GetAs<CSprRes>("SlotMachine.spr");
    if(m_artAction==m_action && m_artFrame==frame && m_artActResource==act && m_artSprResource==spr)return;
    m_artAction=m_action;m_artFrame=frame;m_artActResource=act;m_artSprResource=spr;
    m_artwork.width=m_w;m_artwork.height=m_h;m_artwork.pixels.assign(static_cast<std::size_t>(m_w)*m_h,0);
    const auto* motion=act?act->GetMotion(m_action,frame):nullptr;
    if(motion && spr)for(const auto& clip:motion->sprClips){
        // The selected owner uses SPR list0 and treats any flags value as
        // horizontal flip. It does not apply ACT color/zoom/rotation fields.
        const auto* image=spr->GetSprite(0,clip.sprIndex);
        if(!image || image->width<=0 || image->height<=0)continue;
        const std::int64_t left=static_cast<std::int64_t>(clip.x)+m_w/2;
        const std::int64_t top=static_cast<std::int64_t>(clip.y)+m_h/2;
        const int x0=static_cast<int>((std::max)(std::int64_t(0), (std::min)(std::int64_t(m_w),left)));
        const int y0=static_cast<int>((std::max)(std::int64_t(0), (std::min)(std::int64_t(m_h),top)));
        const int x1=static_cast<int>((std::max)(std::int64_t(0), (std::min)(std::int64_t(m_w),left+image->width)));
        const int y1=static_cast<int>((std::max)(std::int64_t(0), (std::min)(std::int64_t(m_h),top+image->height)));
        for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x){
            const auto column=static_cast<std::size_t>(x-left);
            const auto sx=clip.flags?static_cast<std::size_t>(image->width-1)-column:column;
            const auto source=static_cast<std::size_t>(y-top)*image->width+sx;
            if(source>=image->indices.size())continue;
            const auto index=image->indices[source];
            if(index)m_artwork.pixels[static_cast<std::size_t>(y)*m_w+x]=spr->m_pal[index]|0xff000000u;
        }
    }
    m_uri=ChatRoomBitmapUri(m_artwork);
}

void UIPetTamingDeceiveWnd::PrepareDrawFrame()
{
    if(!m_show)return;
    const auto frame=m_motionTick/m_delay;
    // Keep the source empty boundary frame: draw before checking count.
    PrepareArtwork(static_cast<int>((std::min)(frame,std::int64_t((std::numeric_limits<int>::max)()))));
    auto* act=g_resMgr.GetAs<CActRes>("SlotMachine.act");
    const int count=act?act->GetMotionCount(m_action):0;
    if(frame!=m_destination || m_delay>m_maxDelay)++m_motionTick;
    if(frame>=count){
        m_motionTick=0;
        // Signed overflow in the original is undefined. Saturate only that
        // unsafe boundary; normal resource/frame sequencing remains exact.
        if(m_destination!=-1 && m_delay<(std::numeric_limits<int>::max)())++m_delay;
    }
}

void UIPetTamingDeceiveWnd::OnDraw()
{
    auto dc=AcquireDrawTarget();if(!dc)return;
    if(m_artwork.IsValid())AlphaBlendArgbToHdc(dc,m_x,m_y,m_w,m_h,m_artwork.pixels.data(),m_w,m_h);
    ReleaseDrawTarget(dc);
}
