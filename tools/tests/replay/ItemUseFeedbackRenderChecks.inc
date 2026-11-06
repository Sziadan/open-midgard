// Local 01C8 experience-use notices through the production packet route and
// native/Qt chat views. Included by ItemColourRenderChecks.cpp.
#include "world/GameActor.h"
void CheckItemUseFeedbackUi(QGuiApplication& app,QQuickView& view,QtUiState* uiState,
    CGameMode& mode,UIItemWnd& inventory,QtUiStateAdapter& adapter,const QString& output)
{
    (void)inventory;
    Require(uiState!=nullptr,"item-use feedback Qt state missing");
    SetRuntimeUiScalePercent(100);
    if(g_windowMgr.m_storageWnd)g_windowMgr.m_storageWnd->SetShow(0);
    ro::commerce::state.cart.visible=false;
    if(g_windowMgr.m_itemInfoWnd)g_windowMgr.DeleteWindow(g_windowMgr.m_itemInfoWnd);

    struct ActorRestore {
        u32 aid=g_session.m_aid,gid=g_session.m_gid;
        ~ActorRestore(){g_session.m_aid=aid;g_session.m_gid=gid;}
    } actorRestore;
    g_session.m_aid=0x10101010u;
    g_session.m_gid=0x20202020u;
    auto* player=new CPlayer();
    player->m_gid=g_session.m_gid;
    struct PlayerRestore {
        CWorld* world;CPlayer* previous;CPlayer* player;
        ~PlayerRestore(){world->UnregisterActor(player);world->m_player=previous;delete player;}
    } playerRestore{mode.m_world,mode.m_world->m_player,player};
    mode.m_world->m_player=player;
    mode.m_world->RegisterActor(player);

    const auto sync=[&] {
        Require(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,
            RenderBackendType::LegacyDirect3D7,0,0),"item-use feedback gameplay sync failed");
        Frames(app,view);
    };
    const auto packet=[&](u32 actor,u16 item,bool success=true) {
        std::vector<u8> bytes(13,0);bytes[0]=0xc8;bytes[1]=1;
        bytes[2]=1; // Inventory index; use-notice behavior is independent of it.
        bytes[4]=u8(item);bytes[5]=u8(item>>8);
        for(int i=0;i<4;++i)bytes[6+i]=u8(actor>>(8*i));
        bytes[10]=0;bytes[11]=0;bytes[12]=success?1:0;
        Dispatch(mode,bytes);
    };
    const auto captureNotice=[&](u16 item,const QString& expected,const QString& stem) {
        const auto& events=g_windowMgr.GetChatEvents();
        Require(!events.empty(),"successful local EXP item produced no chat event");
        const auto& event=events.back();
        Require(QString::fromUtf8(event.text.c_str())==expected
            &&event.source.route==ro::chat::NoticeRoute::Normal
            &&event.source.type==ro::chat::NoticeType::Status
            &&event.color==0x0000ffffu,"local EXP notice text/channel/color differs from source mapping");
        auto* chat=g_windowMgr.m_chatWnd;
        Require(chat!=nullptr,"item-use feedback chat window missing");
        ChatTabDisplay bt{};Require(chat->GetTabDisplay(1,&bt)&&bt.id==1
            &&QString::fromUtf8(bt.label)=="BT","actual BT source tab metadata differs");
        chat->SetState(7);chat->Resize(600,200);chat->Move(320,510);
        chat->SetWindowFocused(true);chat->SelectSourceTab(1);
        const auto layout=chat->GetLayoutDisplay();
        Require(layout.tabs[1].right>layout.tabs[1].left,"native BT tab geometry is empty");
        Require(chat->GetSourceTab()==1&&chat->GetTabDisplay(1,&bt)&&bt.active!=0,
            "native BT source tab selection failed");sync();
        bool nativeRow=false;
        for(const auto& row:chat->GetSourceHistory(true))if(QString::fromUtf8(row.text.c_str())==expected
            &&row.color==0x0000ffffu)nativeRow=true;
        Require(nativeRow,"native BT history omitted cyan EXP status notice");
        bool qmlRow=false;
        const auto projected=uiState->chatWindowLines();
        for(int i=0;i<projected.size();++i)if(projected[i].toMap().value("text").toString()==expected) {
            auto* row=Find(view.rootObject(),QString("sourceChatRow%1").arg(i));
            qmlRow=row&&row->isVisible()&&row->property("text").toString()==expected;
            if(qmlRow) {
                const auto pos=row->mapToScene(QPointF(0,0));
                qmlRow=qRound(pos.y())==chat->m_y+layout.history.top+3+i*14;
            }
        }
        Require(qmlRow,"actual sourceChatRow Text omitted local EXP notice");
        auto* pane=Find(view.rootObject(),"chatHistoryPane");
        Require(pane,"actual chat viewport missing for EXP notice");
        const QRect expectedPane(chat->m_x+layout.history.left,chat->m_y+layout.history.top,
            layout.history.right-layout.history.left,layout.history.bottom-layout.history.top);
        Require(pane->isVisible()&&pane->mapRectToScene(pane->boundingRect()).toAlignedRect()==expectedPane,
            "actual source-history viewport geometry diverged from native BT layout");
        const QRect visible=pane->mapRectToScene(pane->boundingRect()).toAlignedRect()
            .intersected(QRect(0,0,view.width(),view.height()));
        const QImage qt=view.grabWindow().convertToFormat(QImage::Format_RGB32);
        int cyan=0;
        for(int y=visible.top();y<=visible.bottom();++y)for(int x=visible.left();x<=visible.right();++x) {
            const QColor c=qt.pixelColor(x,y);
            if(c.red()<90&&c.green()>150&&c.blue()>150)++cyan;
        }
        Require(cyan>20,"actual Qt chat viewport did not paint cyan EXP notice");
        Require(qt.save(output+"/item-use-feedback-"+stem+".qml.png"),"save EXP notice QML frame failed");

        ArgbDibSurface surface;Require(surface.EnsureSize(view.width(),view.height()),"EXP notice native surface failed");
        std::fill_n(surface.GetPixels(),view.width()*view.height(),0xff181b20u);
        g_windowMgr.m_chatWnd->DrawNativeToHdc(surface.GetDC());
        const QImage native=QImage(reinterpret_cast<uchar*>(surface.GetPixels()),view.width(),view.height(),
            view.width()*4,QImage::Format_RGB32).copy();
        const QRect nativeBounds=QRect(g_windowMgr.m_chatWnd->m_x,g_windowMgr.m_chatWnd->m_y,
            g_windowMgr.m_chatWnd->m_w,g_windowMgr.m_chatWnd->m_h).intersected(native.rect());
        int nativeCyan=0;
        for(int y=nativeBounds.top();y<=nativeBounds.bottom();++y)for(int x=nativeBounds.left();x<=nativeBounds.right();++x) {
            const QColor c=native.pixelColor(x,y);
            if(c.red()<90&&c.green()>150&&c.blue()>150)++nativeCyan;
        }
        Require(nativeCyan>20,"native chat did not paint cyan EXP notice");
        Require(native.save(output+"/item-use-feedback-"+stem+".native.png"),"save EXP notice native frame failed");
        (void)item;
    };

    const std::array<std::pair<u16,const char*>,4> notices{{
        {12208,"You will receive 1.5 times more EXP from hunting monsters for the next 30 minutes."},
        {14532,"[USE MSG 1384]"},{14533,"[USE MSG 1385]"},{12312,"[USE MSG 1386]"}
    }};
    g_windowMgr.EnsureChatWindowVisible();
    Require(g_windowMgr.m_chatWnd!=nullptr,"item-use feedback main chat window was not created");
    g_windowMgr.m_chatWnd->SetState(7);
    QJsonArray cases;int frames=0;
    for(const auto& [item,text]:notices) {
        const size_t before=g_windowMgr.GetChatEvents().size();
        packet(g_session.m_aid,item);sync();
        Require(g_windowMgr.GetChatEvents().size()==before+1,"local EXP item did not append exactly one notice");
        captureNotice(item,QString::fromUtf8(text),QString::number(item));frames+=2;
        cases.push_back(QJsonObject{{"case",QString("local item %1 emits mapped cyan EXP notice").arg(item)},{"outcome","PASS"}});
    }

    const auto eventCount=[&]{return g_windowMgr.GetChatEvents().size();};
    size_t before=eventCount();packet(g_session.m_gid,12208);sync();
    Require(eventCount()==before,"GID-only identity incorrectly emitted local EXP notice");
    before=eventCount();packet(g_session.m_aid^0x01010101u,12208);sync();
    Require(eventCount()==before,"remote actor incorrectly emitted local EXP notice");
    before=eventCount();packet(g_session.m_aid,12208,false);sync();
    Require(eventCount()==before,"failed item-use acknowledgement emitted EXP notice");
    before=eventCount();packet(g_session.m_aid,9999);sync();
    Require(eventCount()==before,"unknown item incorrectly emitted EXP notice");
    cases.push_back(QJsonObject{{"case","GID-only, remote actor, failed acknowledgement, and unknown item stay quiet"},{"outcome","PASS"}});

    QFile result(output+"/item-use-feedback-render-results.json");
    Require(result.open(QIODevice::WriteOnly),"open item-use feedback render result failed");
    result.write(QJsonDocument(QJsonObject{{"cases",cases},{"generated_frames",frames},
        {"qml_checks","PASS"},{"errors",0},{"outcome","PASS"}}).toJson(QJsonDocument::Indented));
}
