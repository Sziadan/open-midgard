// Signed recovery packets rendered through the native and QML basic-info views.
// Included by ItemColourRenderChecks.cpp.
#include "world/GameActor.h"
#include "ui/UIBasicInfoWnd.h"
#include "render/DC.h"

QQuickItem* RecoveryDeltaFindGauge(QQuickItem* root,const QString& label)
{
    if(!root)return nullptr;
    if(root->metaObject()->indexOfProperty("label")>=0
        &&root->metaObject()->indexOfProperty("valueText")>=0
        &&(root->property("label").toString()==label
            ||(label=="HP"&&root->property("label").toString().startsWith("HP"))))return root;
    for(auto* child:root->childItems())if(auto* found=RecoveryDeltaFindGauge(child,label))return found;
    return nullptr;
}

void CheckRecoveryDeltaUi(QGuiApplication& app,QQuickView& view,QtUiState* uiState,
    CGameMode& mode,UIItemWnd& inventory,QtUiStateAdapter& adapter,const QString& output)
{
    Require(uiState!=nullptr,"recovery-delta Qt state missing");
    const CharacterVitals savedVitals=g_session.GetCharacterVitals();
    const CHARACTER_INFO* const selectedInfo=g_session.GetSelectedCharacterInfo();
    const bool hadSelectedInfo=selectedInfo!=nullptr;
    const CHARACTER_INFO savedCharacter=selectedInfo?*selectedInfo:CHARACTER_INFO{};
    const int savedEffects=g_session.m_isEffectOn;
    const u32 savedAid=g_session.m_aid,savedGid=g_session.m_gid;
    CWorld* const modeWorld=mode.m_world;
    CPlayer* const modePlayer=mode.m_world?mode.m_world->m_player:nullptr;
    CPlayer* const globalPlayer=g_world.m_player;
    const bool hadBasic=g_windowMgr.m_basicInfoWnd!=nullptr;
    UIBasicInfoWnd* const previousBasic=g_windowMgr.m_basicInfoWnd;
    const int previousBasicShow=previousBasic?previousBasic->m_show:0;
    const int previousBasicX=previousBasic?previousBasic->m_x:0;
    const int previousBasicY=previousBasic?previousBasic->m_y:0;
    const bool previousBasicMini=previousBasic?previousBasic->IsMiniMode():false;
    const auto previousChildren=g_windowMgr.m_children;
    const int inventoryShow=inventory.m_show;
    const int storageShow=g_windowMgr.m_storageWnd?g_windowMgr.m_storageWnd->m_show:0;
    const bool cartVisible=ro::commerce::state.cart.visible;
    const auto savedMessageEffects=[&]{std::vector<CMsgEffect*> values;if(globalPlayer)
        values.assign(globalPlayer->m_msgEffectList.begin(),globalPlayer->m_msgEffectList.end());return values;}();

    auto* player=new CPlayer();
    player->m_gid=0x4a1b2c3du; // This client exposes the local actor GID here; it has no actor m_aid member.
    player->m_MaxHp=100;player->m_Hp=95;player->m_MaxSp=80;player->m_Sp=20;
    struct Restore {
        CGameMode& mode; CWorld* modeWorld; CPlayer* oldModePlayer; CPlayer* oldGlobalPlayer; CPlayer* player;
        CharacterVitals vitals; int effects; u32 aid,gid; bool hadBasic; UIBasicInfoWnd* oldBasic;
        bool hadSelectedInfo; CHARACTER_INFO selectedCharacter;
        int basicShow,basicX,basicY; bool basicMini; std::list<UIWindow*> children;
        UIItemWnd& inventory; int inventoryShow; int storageShow; bool cartVisible;
        std::vector<CMsgEffect*> oldMessages;
        void cleanMessages() {
            for(auto it=player->m_msgEffectList.begin();it!=player->m_msgEffectList.end();) {
                CMsgEffect* effect=*it;
                if(std::find(oldMessages.begin(),oldMessages.end(),effect)!=oldMessages.end()){++it;continue;}
                it=player->m_msgEffectList.erase(it);
                g_world.m_gameObjectList.remove(effect);
                delete effect;
            }
        }
        ~Restore() {
            cleanMessages();
            if(modeWorld)modeWorld->UnregisterActor(player);
            mode.m_world=modeWorld;
            if(modeWorld)modeWorld->m_player=oldModePlayer;
            g_world.m_player=oldGlobalPlayer;
            delete player;
            g_session.UpdateCharacterVital(5,vitals.hp);g_session.UpdateCharacterVital(6,vitals.maxHp);
            g_session.UpdateCharacterVital(7,vitals.sp);g_session.UpdateCharacterVital(8,vitals.maxSp);
            if(hadSelectedInfo)if(auto* info=g_session.GetMutableSelectedCharacterInfo())*info=selectedCharacter;
            g_session.m_isEffectOn=effects;g_session.m_aid=aid;g_session.m_gid=gid;
            inventory.SetShow(inventoryShow);
            if(g_windowMgr.m_storageWnd)g_windowMgr.m_storageWnd->SetShow(storageShow);
            ro::commerce::state.cart.visible=cartVisible;
            if(hadBasic&&oldBasic) {
                g_windowMgr.m_basicInfoWnd=oldBasic;oldBasic->Move(basicX,basicY);oldBasic->SetShow(basicShow);
                if(oldBasic->IsMiniMode()!=basicMini)oldBasic->OnLBtnDblClk(oldBasic->m_x+10,oldBasic->m_y+5);
            } else if(g_windowMgr.m_basicInfoWnd)g_windowMgr.DeleteWindow(g_windowMgr.m_basicInfoWnd);
            g_windowMgr.m_children=children;
        }
    } restore{mode,modeWorld,modePlayer,globalPlayer,player,savedVitals,savedEffects,savedAid,savedGid,
        hadBasic,previousBasic,hadSelectedInfo,savedCharacter,previousBasicShow,previousBasicX,previousBasicY,previousBasicMini,previousChildren,
        inventory,inventoryShow,storageShow,cartVisible,savedMessageEffects};

    inventory.SetShow(0);
    if(g_windowMgr.m_storageWnd)g_windowMgr.m_storageWnd->SetShow(0);
    ro::commerce::state.cart.visible=false;
    auto* basic=static_cast<UIBasicInfoWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_BASICINFOWND));
    Require(basic!=nullptr,"managed basic-info window could not be created");
    basic->Move(400,100);
    if(basic->IsMiniMode())basic->OnLBtnDblClk(basic->m_x+10,basic->m_y+5);
    player->m_Hp=95;player->m_MaxHp=100;player->m_Sp=20;player->m_MaxSp=80;
    mode.m_world=modeWorld;
    Require(modeWorld!=nullptr,"recovery-delta gameplay world missing");
    modeWorld->m_player=player;modeWorld->RegisterActor(player);g_world.m_player=player;
    g_session.m_aid=player->m_gid;g_session.m_gid=player->m_gid;g_session.m_isEffectOn=false;
    const auto setVitals=[](u32 hp,u32 maxHp,u32 sp,u32 maxSp){
        g_session.UpdateCharacterVital(5,hp);g_session.UpdateCharacterVital(6,maxHp);
        g_session.UpdateCharacterVital(7,sp);g_session.UpdateCharacterVital(8,maxSp);
    };
    setVitals(95,100,20,80);
    const auto put16=[](std::vector<u8>& p,size_t o,u16 v){p[o]=static_cast<u8>(v);p[o+1]=static_cast<u8>(v>>8);};
    const auto put32=[](std::vector<u8>& p,size_t o,u32 v){for(int i=0;i<4;++i)p[o+i]=static_cast<u8>(v>>(8*i));};
    const auto recovery=[&](u16 type,u16 raw){std::vector<u8> p(6);put16(p,0,0x013d);put16(p,2,type);put16(p,4,raw);return p;};
    const auto status=[&](u16 type,u32 value){std::vector<u8> p(8);put16(p,0,0x00b0);put16(p,2,type);put32(p,4,value);return p;};
    const auto sync=[&]{
        Require(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,
            RenderBackendType::LegacyDirect3D7,0,0),"recovery-delta gameplay sync failed");
        Frames(app,view);
    };
    const auto verifyValues=[&](u32 hp,u32 maxHp,u32 sp,u32 maxSp,const char* label){
        UIBasicInfoWnd::DisplayData native{};
        Require(basic->GetDisplayDataForQt(&native),"native basic-info display data unavailable");
        Require(native.hp==hp&&native.maxHp==maxHp&&native.sp==sp&&native.maxSp==maxSp,
            label);
        const auto& data=uiState->basicInfoData();
        Require(data.value("hp").toULongLong()==hp&&data.value("maxHp").toULongLong()==maxHp
            &&data.value("sp").toULongLong()==sp&&data.value("maxSp").toULongLong()==maxSp,
            "Qt basic-info data differs from expected recovery values");
        QQuickItem* hpGauge=RecoveryDeltaFindGauge(view.rootObject(),QStringLiteral("HP"));
        QQuickItem* spGauge=RecoveryDeltaFindGauge(view.rootObject(),QStringLiteral("SP"));
        Require(hpGauge&&spGauge&&hpGauge->isVisible()&&spGauge->isVisible(),
            "actual HP/SP RoGauge controls missing or hidden");
        Require(hpGauge->property("valueText").toString()==QStringLiteral("%1 / %2").arg(hp).arg(maxHp)
            &&spGauge->property("valueText").toString()==QStringLiteral("%1 / %2").arg(sp).arg(maxSp),
            "actual RoGauge labels do not show the expected values");
        const QImage qml=view.grabWindow().convertToFormat(QImage::Format_RGB32);
        Require(!qml.isNull(),"recovery-delta QML frame capture failed");
        const auto inkCount=[&](QQuickItem* gauge,QRgb ink) {
            const QRect rect=gauge->mapRectToScene(gauge->boundingRect()).toAlignedRect().intersected(qml.rect());
            return PixelsNear(qml,rect,ink);
        };
        Require(inkCount(hpGauge,QColor(hp*4u<maxHp?"#ab4249":"#bd6266").rgb())>12&&inkCount(spGauge,QColor("#5788b9").rgb())>8,
            "actual QML HP/SP gauge rectangles contain no painted bar ink");
        Require(qml.save(output+QString("/recovery-delta-%1.qml.png").arg(label)),
            "save recovery-delta QML frame failed");

        ArgbDibSurface surface;Require(surface.EnsureSize(view.width(),view.height()),"native basic-info DIB allocation failed");
        std::fill_n(surface.GetPixels(),view.width()*view.height(),0xff181b20u);
        basic->DrawNativeToHdc(surface.GetDC());
        const QImage nativeImage=QImage(reinterpret_cast<uchar*>(surface.GetPixels()),view.width(),view.height(),
            view.width()*4,QImage::Format_RGB32).copy();
        const QRect rect(basic->m_x,basic->m_y,basic->m_w,basic->m_h);
        int nativeInk=0;
        for(int y=rect.top();y<rect.bottom()&&y<nativeImage.height();++y)for(int x=rect.left();x<rect.right()&&x<nativeImage.width();++x)
            if((nativeImage.pixel(x,y)&0xffffffu)!=0x181b20u)++nativeInk;
        Require(nativeInk>100,"native basic-info window rectangle contains no rendered ink");
        Require(PixelsNear(nativeImage,QRect(basic->m_x+95,basic->m_y+30,135,15),qRgb(0,0,0))>20
            &&PixelsNear(nativeImage,QRect(basic->m_x+95,basic->m_y+51,135,15),qRgb(0,0,0))>20,
            "native HP/SP numeric rows contain no painted text");
        Require(nativeImage.save(output+QString("/recovery-delta-%1.native.png").arg(label)),
            "save recovery-delta native frame failed");
    };

    QJsonArray cases;int frames=0;
    Dispatch(mode,recovery(5,12));restore.cleanMessages();sync();
    verifyValues(107,100,20,80,"hp-plus-12-no-clamp");frames+=2;
    cases.push_back(QJsonObject{{"case","013D HP +12 from 95 exceeds max 100"},{"expected_hp",107},{"outcome","PASS"}});

    Dispatch(mode,recovery(7,0xffff));restore.cleanMessages();sync();
    verifyValues(107,100,19,80,"sp-minus-1");frames+=2;
    cases.push_back(QJsonObject{{"case","013D SP raw 0xffff is signed -1"},{"expected_sp",19},{"outcome","PASS"}});

    Dispatch(mode,recovery(5,0));restore.cleanMessages();
    Require(g_session.GetCharacterVitals().hp==107&&player->m_Hp==107,"zero recovery changed HP");
    Dispatch(mode,recovery(4,12));restore.cleanMessages();
    Require(g_session.GetCharacterVitals().hp==107&&g_session.GetCharacterVitals().sp==19
        &&player->m_Hp==107&&player->m_Sp==19,"unknown recovery status changed vitals");
    cases.push_back(QJsonObject{{"case","zero delta retains value and unknown status is quiet"},{"outcome","PASS"}});

    Dispatch(mode,status(5,77));sync();
    verifyValues(77,100,19,80,"authoritative-00b0");frames+=2;
    cases.push_back(QJsonObject{{"case","authoritative 00B0 replaces recovery delta"},{"expected_hp",77},{"outcome","PASS"}});

    player->m_Hp=40000;player->m_MaxHp=50000;setVitals(40000,50000,19,80);
    Dispatch(mode,recovery(5,0x8000));restore.cleanMessages();sync();
    verifyValues(7232,50000,19,80,"signed-32768-from-40000");frames+=2;
    cases.push_back(QJsonObject{{"case","013D raw 0x8000 is signed -32768 from 40000"},{"expected_hp",7232},{"outcome","PASS"}});

    QFile result(output+QStringLiteral("/recovery-delta-render-results.json"));
    Require(result.open(QIODevice::WriteOnly),"open recovery-delta render results failed");
    result.write(QJsonDocument(QJsonObject{{"cases",cases},{"generated_frames",frames},
        {"qml_checks","PASS"},{"errors",0},{"outcome","PASS"}}).toJson(QJsonDocument::Indented));
}
