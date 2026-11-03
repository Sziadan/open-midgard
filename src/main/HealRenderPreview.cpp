#include "world/World.h"
#include "world/GameActor.h"
#include "world/Gr2Monster.h"
#include "core/Timer.h"
#include "world/RagEffect.h"
#include "world/MsgEffect.h"
#include "world/Emotion.h"
#include "world/OverheadEffect.h"
#include "res/ActRes.h"
#include "res/Sprite.h"
#include "session/Session.h"
#include "gamemode/GameMode.h"
#include "gamemode/GameModePacket.h"
#include "gamemode/Commerce.h"
#include "network/Connection.h"
#include "gamemode/View.h"
#include "network/MapSendProfile.h"
#include "qtui/QtUiRuntime.h"
#include "ui/UIWindowMgr.h"
#include "ui/UINewChatWnd.h"
#include "ui/UIGuildWnd.h"
#include "ui/UIItemWnd.h"
#include "ui/UIPlayerContextMenuWnd.h"
#include "ui/UiScale.h"
#include "main/WinMain.h"
#include "render/Renderer.h"
#include "render3d/RenderDevice.h"
#include "res/Res.h"
#include "res/WorldRes.h"
#include "res/GndRes.h"
#include <cstdlib>
#include <algorithm>
#include <memory>
#include <functional>
#include <fstream>
#include <iterator>
#include "Types.h"
#include "DebugLog.h"
#include <windows.h>
#if RO_ENABLE_QT6_UI
#include "qtui/QtUiStateAdapter.h"
#include "qtui/QtUiState.h"
#include "render3d/RenderBackend.h"
#include <QImage>
#include <QBuffer>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QQuickItem>
#endif

// Opt-in native render fixture. Uses installed assets and the selected graphics
// backend, without entering a game mode, logging in, or saving settings.
void RunHealRenderPreview()
{
    if (std::getenv("RO_HEAL_PREVIEW_MONSTER")) WaitForGr2Monsters();
    const char* previewMap=std::getenv("RO_HEAL_PREVIEW_MAP");
    const bool room=std::getenv("RO_HEAL_PREVIEW_ROOM")!=nullptr || previewMap;
    const bool avatar=std::getenv("RO_HEAL_PREVIEW_PLAYER")!=nullptr;
    CPlayer player;player.m_gid=1;player.m_pos=room?vector3d{-232.5f,-10,-12.5f}:vector3d{};g_world.m_player=&player;
    if (const char* pos=std::getenv("RO_HEAL_PREVIEW_POS")) std::sscanf(pos,"%f,%f,%f",&player.m_pos.x,&player.m_pos.y,&player.m_pos.z);
    if (std::getenv("RO_HEAL_PREVIEW_CONFUSION")) player.m_healthState |= 0x08;
    if (std::getenv("RO_HEAL_PREVIEW_HALLUCINATION")) g_session.SetActiveStatusIcon(34,true,0);
    if (std::getenv("RO_HEAL_PREVIEW_BLIND")) player.m_healthState |= 0x10;
    player.m_job=8;player.m_head=1;player.m_sex=1;player.m_isPc=1;player.m_isVisible=1;
    {
        const int effect=std::getenv("RO_HEAL_PREVIEW_CAST")?58:312;
        auto heal=std::make_unique<CRagEffect>();heal->Init(&player,effect,{});
        const int refresh=std::getenv("RO_HEAL_PREVIEW_REFRESH")?std::clamp(std::atoi(std::getenv("RO_HEAL_PREVIEW_REFRESH")),1,16):1;
        for(int frame=0;frame<(refresh>1?60*refresh:30);++frame) {
            g_isSkipFrame=(frame%refresh)==0?1:0;
            if(heal&&!heal->OnProcess())heal.reset();
            g_world.UpdateGameObjects();
        }
        DbgLog("[HealPreview] refresh=%d parentAlive=%d parentState=%d\n",refresh,heal?1:0,heal?heal->GetStateCount():-1);
        matrix view{};
        view.m[0][0]=1;view.m[1][1]=0.70710678f;view.m[2][1]=-0.70710678f;
        view.m[1][2]=0.70710678f;view.m[2][2]=0.70710678f;view.m[3][2]=500;view.m[3][3]=1;
        view.m[3][0]=-player.m_pos.x;view.m[3][1]=(-player.m_pos.y+player.m_pos.z)*0.70710678f;
        view.m[3][2]-=(player.m_pos.y+player.m_pos.z)*0.70710678f;
        g_renderer.m_nClearColor=0xFF202020;
        g_renderer.ClearBackground();g_renderer.Clear(0);
        if(room) {
            const std::string mapPath=previewMap ? std::string("data\\")+previewMap+".rsw" : "data\\new_zone01.rsw";
            if(auto* world=g_resMgr.GetAs<C3dWorldRes>(mapPath.c_str())) {
                DbgLog("[MapPreview] %s waterLevel=%.3f waterType=%d wave=%.3f\n",mapPath.c_str(),world->m_waterLevel,world->m_waterType,world->m_waveHeight);
                g_world.BuildBackgroundObjects(*world,world->m_lightDir,world->m_diffuseCol,world->m_ambientCol);
                if (std::getenv("RO_HEAL_PREVIEW_GROUND")) {
                    const std::string ground="data\\"+world->m_gndFile;
                    if(auto* gnd=g_resMgr.GetAs<CGndRes>(ground.c_str())) {
                        g_world.BuildGroundFromGnd(*gnd,world->m_lightDir,world->m_diffuseCol,world->m_ambientCol,
                            world->m_waterLevel,world->m_waterType,world->m_waterAnimSpeed,world->m_wavePitch,world->m_waveSpeed,world->m_waveHeight);
                        if(std::getenv("RO_HEAL_PREVIEW_NIGHT")) {
                            g_world.SetNightMode(true);
                            for(int tick=0;tick<120;++tick)g_world.UpdateNightLighting();
                            if(std::getenv("RO_HEAL_PREVIEW_DAY_AFTER_NIGHT")) {
                                g_world.SetNightMode(false);
                                for(int tick=0;tick<120;++tick)g_world.UpdateNightLighting();
                            }
                        }
                        g_world.m_ground->FlushGround(view);
                        DbgLog("[MapPreview] liquid texture loaded=%d set=%d dimensions=%ux%u\n",
                            g_world.m_ground->m_waterTex?1:0,g_world.m_ground->m_waterSet,
                            g_world.m_ground->m_waterTex?g_world.m_ground->m_waterTex->m_w:0,
                            g_world.m_ground->m_waterTex?g_world.m_ground->m_waterTex->m_h:0);
                    }
                }
                g_world.RenderBackgroundObjects(view);
            }
        }
        if(const char* weatherId=std::getenv("RO_HEAL_PREVIEW_WEATHER")) {
            CRagEffect weather;weather.Init(&player,std::atoi(weatherId),{});
            weather.OnProcess();
            const char* age=std::getenv("RO_HEAL_PREVIEW_WEATHER_FRAMES");
            const int frames=age?std::clamp(std::atoi(age),0,5000):200;
            int clouds=0,maxAlpha=0;
            for(CGameObject* object:g_world.m_gameObjectList) {
                auto* cloud=dynamic_cast<CEffectPrim*>(object);
                if(!cloud || cloud->m_type!=PP_CLOUD)continue;
                for(int frame=0;frame<frames;++frame)cloud->ProcessPrimitiveFrame();
                // This fixture has no gameplay view; match its fixed camera tilt.
                cloud->m_gi[0].alphaT=0;cloud->m_gi[1].alphaT=45;
                for(const auto& gi:cloud->m_gi)maxAlpha=(std::max)(maxAlpha,static_cast<int>(gi.alphaB));
                cloud->Render(&view);++clouds;
            }
            DbgLog("[WeatherPreview] effect=%s clouds=%d frames=%d maxAlpha=%d\n",weatherId,clouds,frames,maxAlpha);
        }
        if(avatar) {
            vector3d eye{player.m_pos.x,player.m_pos.y-353.553f,player.m_pos.z-353.553f};
            vector3d at=player.m_pos,up{0,1,0};g_renderer.SetLookAt(eye,at,up);
            CPc monster;
            if (const char* job=std::getenv("RO_HEAL_PREVIEW_MONSTER")) {
                monster.m_gid=17;monster.m_job=std::atoi(job);monster.m_isPc=0;monster.m_isVisible=1;
                if(const char* rotation=std::getenv("RO_HEAL_PREVIEW_MONSTER_ROTATION"))monster.m_roty=std::atof(rotation);
                if(const char* zoom=std::getenv("RO_HEAL_PREVIEW_MONSTER_ZOOM"))monster.m_zoom=std::atof(zoom);
                monster.m_pos=player.m_pos;monster.m_pos.x+=25;
                monster.m_stateId=std::getenv("RO_HEAL_PREVIEW_MONSTER_STATE")?std::atoi(std::getenv("RO_HEAL_PREVIEW_MONSTER_STATE")):0;
                monster.m_stateStartTick=timeGetTime()-(std::getenv("RO_HEAL_PREVIEW_MONSTER_AGE")?std::atoi(std::getenv("RO_HEAL_PREVIEW_MONSTER_AGE")):0);
                g_world.m_actorList.push_back(&monster);
                g_world.m_bgLightDir={0.5f,0.70710678f,0.5f};g_world.m_bgDiffuseCol={1,1,1};g_world.m_bgAmbientCol={0.5f,0.5f,0.5f};
                if(monster.m_job==722 && std::getenv("RO_HEAL_PREVIEW_FLAG_EMBLEM")) {
                    CGameMode packetMode;packetMode.m_world=&g_world;
                    packetMode.m_runtimeActors[17]=&monster;
                    CGameModePacketRouter router;RegisterDefaultGameModePacketHandlers(router);
                    // Legacy flag idle uses accessory/head words for ownership.
                    std::array<u8,55> spawn{};spawn[0]=0x78;spawn[2]=6;
                    auto put=[&](int at,u32 value,int bytes){for(int i=0;i<bytes;++i)spawn[at+i]=static_cast<u8>(value>>(8*i));};
                    put(3,17,4);put(7,150,2);put(15,722,2);put(21,0,2);put(25,1,2);put(27,7,2);spawn[46]=1;
                    spawn[47]=1;spawn[48]=0x40;spawn[49]=0x50;
                    const auto pos=monster.m_pos;const float rotation=monster.m_roty;
                    const bool dispatched=router.Dispatch(packetMode,{0x0078,55,spawn.data()});
                    const bool ownership=dispatched && monster.m_gdid==65543 && monster.m_emblemVersion==0;
                    std::array<u8,42> legacy{};legacy[0]=0x7c;legacy[2]=6;legacy[3]=17;legacy[7]=150;
                    legacy[21]=0xd2;legacy[22]=2;legacy[23]=4;legacy[25]=2;legacy[27]=8;legacy[36]=1;
                    legacy[37]=1;legacy[38]=0x40;legacy[39]=0x50;
                    const bool legacyOwnership=router.Dispatch(packetMode,{0x007c,42,legacy.data()}) && monster.m_gdid==131080 && monster.m_emblemVersion==4;
                    std::array<u8,12> clear{};clear[0]=0xb4;clear[1]=1;clear[2]=17;
                    const bool cleared=router.Dispatch(packetMode,{0x01b4,12,clear.data()}) && monster.m_gdid==0 && monster.m_emblemVersion==0;
                    router.Dispatch(packetMode,{0x0078,55,spawn.data()});
                    monster.m_pos=pos;monster.m_roty=rotation;
                    std::ifstream input(std::getenv("RO_HEAL_PREVIEW_FLAG_EMBLEM"),std::ios::binary);
                    const std::vector<unsigned char> bitmap((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());
                    packetMode.m_guildEmblems[65543]={3,bitmap};
                    const u32 resolved=PrepareGr2FlagEmblem(packetMode,65543,0);
                    auto* emblemTexture=Gr2GuildEmblemTexture(65543,3);
                    const bool cached=resolved==3 && emblemTexture!=nullptr;
                    DbgLog("[GuildFlagPreview] referencePadding=%d\n",emblemTexture && emblemTexture->m_w==32 && emblemTexture->m_h==32 && emblemTexture->m_surfaceUpdateWidth==32 && emblemTexture->m_surfaceUpdateHeight==32);
                    const bool missing=PrepareGr2FlagEmblem(packetMode,65544,0)==0;
                    const bool newVersion=PrepareGr2FlagEmblem(packetMode,65543,4)==0;
                    monster.m_emblemVersion=static_cast<int>(resolved);
                    const bool isolated=!Gr2GuildEmblemTexture(65544,3) && !Gr2GuildEmblemTexture(65543,4);
                    // Populate the hit cache before the first model has bounds.
                    CGameActor* ignored=nullptr;g_world.FindHoveredActorScreen(view,0,0,0,&ignored,nullptr,nullptr,false);
                    DbgLog("[GuildFlagPreview] ownership=%d texture=%d isolated=%d legacy=%d clear=%d unspecified=%d\n",ownership,cached,isolated,legacyOwnership,cleared,missing && newVersion && resolved==3);
                    packetMode.m_runtimeActors.clear();
                    packetMode.m_world=nullptr;
                }
            }
            const int monsterFrames=std::getenv("RO_HEAL_PREVIEW_MONSTER_FRAMES")
                ? std::clamp(std::atoi(std::getenv("RO_HEAL_PREVIEW_MONSTER_FRAMES")),1,10) : 1;
            for(int frame=0;frame<monsterFrames;++frame) {
                g_world.RenderActors(view,0);
                if(frame+1<monsterFrames) {
                    g_renderer.DrawScene();GetRenderDevice().Present(false);
                    g_renderer.ClearBackground();g_renderer.Clear(0);
                }
            }
            if (std::getenv("RO_HEAL_PREVIEW_MONSTER")) {
                DbgLog("[MonsterPreview] proprietaryRuntimeLoaded=%d\n",
                    (GetModuleHandleA("granny2.dll") || GetModuleHandleA("granny2_x64.dll"))?1:0);
                RECT bounds{};const bool rendered=GetGr2MonsterBounds(monster.m_gid,&bounds);
                CGameActor* picked=nullptr;
                const bool pick=rendered && g_world.FindHoveredActorScreen(view,0,(bounds.left+bounds.right)/2,(bounds.top+bounds.bottom)/2,&picked,nullptr,nullptr,false);
                DbgLog("[MonsterPreview] job=%d rendered=%d pick=%d picked=%u bounds=%ld,%ld,%ld,%ld\n",monster.m_job,rendered?1:0,pick?1:0,picked?picked->m_gid:0,bounds.left,bounds.top,bounds.right,bounds.bottom);
                if(monster.m_job==111) {
                    int x=0,y=0,top=0;
                    const bool marker=g_world.GetActorScreenMarker(view,0,17,&x,&top,&y);
                    picked=nullptr;
                    const bool hiddenPick=marker && g_world.FindHoveredActorScreen(view,0,x,y-4,&picked,nullptr,nullptr,false);
                    DbgLog("[HiddenNpcPreview] pick=%d picked=%u sprite=%d\n",hiddenPick,picked?picked->m_gid:0,monster.m_billboardTexture?1:0);
                }
                monster.ProcessMotion();
                DbgLog("[MonsterPreview] motionFinished=%d\n",monster.m_isMotionFinished?1:0);
                g_world.m_actorList.remove(&monster);
            }
        } else for (CGameObject* object : g_world.m_gameObjectList) {
            auto* primitive=dynamic_cast<CEffectPrim*>(object);
            if (primitive && primitive->m_type==PP_HEAL) primitive->Render(&view);
        }
        if (std::getenv("RO_HEAL_PREVIEW_MONSTER")) {
            RECT bounds{};
            DbgLog("[MonsterPreview] instanceCleanup=%d\n",GetGr2MonsterBounds(17,&bounds)?0:1);
        }
        if (const char* status = std::getenv("RO_HEAL_PREVIEW_STATUS"))
            player.SendMsg(&player,std::atoi(status),0,0,0);
        if (const char* emotion = std::getenv("RO_HEAL_PREVIEW_EMOTION")) {
            auto* act = g_resMgr.GetAs<CActRes>("data\\sprite\\\xC0\xCC\xC6\xD1\xC6\xAE\\emotion.act");
            int supported=0;
            for (int id=0;id<=80;++id) if (act && act->GetMotionCount(EmotionAction(id))>0) ++supported;
            DbgLog("[StatusPreview] emotion actions available=%d/81\n",supported);
            if (std::getenv("RO_HEAL_PREVIEW_EMOTION_AUDIT")) {
                auto* sprite=g_resMgr.GetAs<CSprRes>("data\\sprite\\\xC0\xCC\xC6\xD1\xC6\xAE\\emotion.spr");
                for (int id=0;sprite && act && id<=80;++id) {
                    const int action=EmotionAction(id);
                    for (int frame=0;frame<act->GetMotionCount(action);++frame) {
                        const auto* motion=act->GetMotion(action,frame);
                        float bottom=-1e9f,top=1e9f;
                        for (const auto& clip : motion->sprClips) {
                            const auto* image=sprite->GetSprite(clip.clipType,clip.sprIndex);
                            if (!image || clip.a==0) continue;
                            for (const auto& point : OverheadClipQuad(clip.x,clip.y,image->width*(image->isHalfW+1),
                                image->height*(image->isHalfH+1),clip.zoomX,clip.zoomY,clip.angle)) {
                                bottom=(std::max)(bottom,point.y);top=(std::min)(top,point.y);
                            }
                        }
                        DbgLog("[EmotionAudit] id=%d frame=%d top=%.3f bottom=%.3f clips=%d\n",id,frame,top,bottom,static_cast<int>(motion->sprClips.size()));
                    }
                }
            }
            player.SendMsg(&player,21,std::atoi(emotion),0,0);
        }
        if (std::getenv("RO_HEAL_PREVIEW_EMOTION") || std::getenv("RO_HEAL_PREVIEW_STATUS")) {
            for (CGameObject* object : g_world.m_gameObjectList) {
                if (auto* message=dynamic_cast<CMsgEffect*>(object)) {
                    bool alive=true;
                    if (const char* age=std::getenv("RO_HEAL_PREVIEW_MESSAGE_AGE_MS")) {
                        message->m_stateStartTick=timeGetTime()-static_cast<unsigned int>(std::atoi(age));
                        alive=message->OnProcess()!=0;
                    }
                    const char* path=message->m_customActPath.empty()
                        ? "data\\sprite\\\xC0\xCC\xC6\xD1\xC6\xAE\\msg.act" : message->m_customActPath.c_str();
                    auto* act=g_resMgr.GetAs<CActRes>(path);
                    DbgLog("[StatusPreview] type=%d action=%d motion=%d delay=%.3f frames=%d alive=%d\n",
                        message->m_msgEffectType,message->m_spriteActionIndex,message->m_spriteMotionIndex,
                        act?act->GetDelay(message->m_spriteActionIndex):0,
                        act?act->GetMotionCount(message->m_spriteActionIndex):0,alive?1:0);
                    if (alive) message->Render(&view);
                }
            }
            QueueQueuedMsgEffectsQuads();
        }
        CTexture overlayTexture;
        CTexture lightingBase,lightingShadow,lightingColor;
        if(std::getenv("RO_HEAL_PREVIEW_LIGHTMAP_TEST")) {
            unsigned base=0xFF808080u,shadow=0x80000000u,color=0x80002080u;
            lightingBase.Create(1,1,PF_A8R8G8B8,false);lightingBase.Update(0,0,1,1,&base,true,4);
            lightingShadow.Create(1,1,PF_A8R8G8B8,false);lightingShadow.Update(0,0,1,1,&shadow,true,4);
            lightingColor.Create(1,1,PF_A8R8G8B8,false);lightingColor.Update(0,0,1,1,&color,true,4);
            for(int i=0;i<2;++i)if(auto* face=g_renderer.BorrowLmQuadRP()) {
                face->lmverts=face->m_lmverts;face->numVerts=4;face->numIndices=0;face->indices=nullptr;
                face->primType=D3DPT_TRIANGLESTRIP;face->tex=&lightingBase;face->tex2=i?&lightingColor:&lightingShadow;
                const float x=16+i*80.0f;
                for(int k=0;k<4;++k) {
                    face->m_lmverts[k].vert={x+(k/2)*64.0f,16+(k%2)*64.0f,0,1,0xFFFFFFFFu,0,0,0};
                    face->m_lmverts[k].tu2=0;face->m_lmverts[k].tv2=0;
                }
                g_renderer.AddLmRP(reinterpret_cast<RPLmFace*>(face),0x800);
            }
        }
        CTexture chatTexture;
        if (std::getenv("RO_HEAL_PREVIEW_CHAT")) {
            for(auto* window:g_windowMgr.m_children)window->SetShow(0);
            g_windowMgr.EnsureChatWindowVisible();
            auto* chat=g_windowMgr.m_chatWnd;
            chat->Move(12,170);chat->Resize(620,300);
            chat->AddChatLine(u8"Swedish: åäö ÅÄÖ",0x00E8DFCF,0,GetTickCount());
            chat->AddChatLine(u8"Cyrillic: Привет мир",0x008FD3EF,0,GetTickCount());
            chat->AddChatLine(u8"Japanese: ひらがな カタカナ 日本語 漢字",0x00AEDB9D,0,GetTickCount());
            chat->SetQtInputText(2,u8"åäö Привет 日本語 ");chat->FocusQtInput(2);
            CGameMode mode;mode.m_world=&g_world;
            const int width=g_renderer.m_width,height=g_renderer.m_height;
            bool rendered=chatTexture.Create(width,height,PF_A8R8G8B8,false);
            if(rendered)rendered=RenderQtUiGameplayOverlayTexture(mode,&chatTexture,width,height);
            ProcessQtUiRuntimeEvents();
            SendMessageW(g_hMainWnd,WM_CHAR,0x00F6,0);
            const bool dirty=QtUiRuntimeNeedsRender();
            if(rendered)rendered=RenderQtUiGameplayOverlayTexture(mode,&chatTexture,width,height);
            ProcessQtUiRuntimeEvents();
            if(rendered)rendered=RenderQtUiGameplayOverlayTexture(mode,&chatTexture,width,height);
            const bool typed=chat->GetInputText()==u8"åäö Привет 日本語 ö";
            DbgLog("[ChatPreview] nativeUnicode=%d rendered=%d typed=%d sceneDirty=%d settledDirty=%d\n",IsWindowUnicode(g_hMainWnd)?1:0,rendered?1:0,typed?1:0,dirty?1:0,QtUiRuntimeNeedsRender()?1:0);
            if(rendered)if(RPFace* face=g_renderer.BorrowNullRP()) {
                face->primType=D3DPT_TRIANGLESTRIP;face->verts=face->m_verts;face->numVerts=4;face->indices=nullptr;face->numIndices=0;
                face->tex=&chatTexture;face->mtPreset=0;face->cullMode=D3DCULL_NONE;
                face->srcAlphaMode=D3DBLEND_SRCALPHA;face->destAlphaMode=D3DBLEND_INVSRCALPHA;face->alphaSortKey=3;
                face->m_verts[0]={0,0,0,1,0xFFFFFFFFu,0,0,0};
                face->m_verts[1]={static_cast<float>(width),0,0,1,0xFFFFFFFFu,0,1,0};
                face->m_verts[2]={0,static_cast<float>(height),0,1,0xFFFFFFFFu,0,0,1};
                face->m_verts[3]={static_cast<float>(width),static_cast<float>(height),0,1,0xFFFFFFFFu,0,1,1};
                g_renderer.AddRP(face,1|8);
            }
        }
#if RO_ENABLE_QT6_UI
        if(std::getenv("RO_HEAL_PREVIEW_SELF_HOVER")) {
            for(auto* window:g_windowMgr.m_children)window->SetShow(0);
            CGameMode mode;mode.m_world=&g_world;
            CView camera;camera.SetWorld(&g_world);camera.SetInitialCamera(0,-45,500);camera.OnCalcViewInfo();mode.m_view=&camera;
            g_session.m_gid=player.m_gid;g_session.m_partyName="Test Party";
            std::strcpy(g_session.m_playerName,"Test Player");
            // Exercise real packet framing, decompression, version cache and Qt
            // decoding with a 24x24 emblem containing transparent magenta corners.
            CGameModePacketRouter router;RegisterDefaultGameModePacketHandlers(router);
            std::array<u8,43> belong{};belong[0]=0x6c;belong[1]=1;belong[2]=7;belong[6]=1;
            std::memcpy(belong.data()+19,"Test Guild",10);
            router.Dispatch(mode,{0x016c,43,belong.data()});
            std::array<u8,102> nameReply{};
            const u16 nameOpcode=ro::net::GetActiveMapReceiveProfile().actorNameAckFull;
            nameReply[0]=static_cast<u8>(nameOpcode);nameReply[1]=static_cast<u8>(nameOpcode>>8);nameReply[2]=static_cast<u8>(player.m_gid);
            std::memcpy(nameReply.data()+6,"Test Player",11);std::memcpy(nameReply.data()+30,"Test Party",10);
            std::memcpy(nameReply.data()+54,"Test Guild",10);std::memcpy(nameReply.data()+78,"Guild Master",12);
            router.Dispatch(mode,{nameOpcode,102,nameReply.data()});
            QImage icon(24,24,QImage::Format_RGB32);icon.fill(QColor(255,0,255));
            for(int y=3;y<21;++y)for(int x=3;x<21;++x)icon.setPixel(x,y,qRgb(30,190,220));
            QByteArray bmp;QBuffer buffer(&bmp);buffer.open(QIODevice::WriteOnly);icon.save(&buffer,"BMP");
            const QByteArray compressed=qCompress(bmp).mid(4);
            std::vector<u8> packet(12+compressed.size());packet[0]=0x52;packet[1]=1;
            packet[2]=static_cast<u8>(packet.size());packet[3]=static_cast<u8>(packet.size()>>8);packet[4]=7;packet[8]=1;
            std::memcpy(packet.data()+12,compressed.data(),compressed.size());
            PacketView decoded{};int consumed=0;
            const bool framed=TryReadPacket(packet.data(),static_cast<int>(packet.size()),decoded,consumed);
            mode.m_pendingEmblemGuild=7;mode.m_pendingEmblemVersion=0;
            mode.m_pendingEmblemData.assign(packet.begin()+12,packet.end());
            const bool handled=framed && router.Dispatch(mode,decoded);
            const bool uploadConfirmed=mode.m_pendingEmblemGuild==0 && mode.m_pendingEmblemData.empty();
            // Corrupt compressed data must not replace the valid cached emblem.
            mode.m_pendingEmblemGuild=7;mode.m_pendingEmblemData.assign(packet.begin()+12,packet.end());
            auto corrupt=packet;corrupt[12]^=0xff;router.Dispatch(mode,{0x0152,static_cast<u16>(corrupt.size()),corrupt.data()});
            const bool corruptIgnored=mode.m_guildEmblems.count(7) && mode.m_guildEmblems.at(7).bitmap.size()==static_cast<size_t>(bmp.size());
            DbgLog("[GuildEmblemPreview] confirmed=%d corruptUnconfirmed=%d\n",uploadConfirmed,mode.m_pendingEmblemGuild==7);
            mode.m_pendingEmblemGuild=0;mode.m_pendingEmblemData.clear();
            // Map changes discard name entries and recreate the self actor.
            // Membership and rank must remain usable before fresh replies arrive.
            mode.m_actorNameListByGID.clear();mode.m_actorNameList.clear();
            player.m_gdid=0;player.m_emblemVersion=0;
            const bool persisted=g_session.m_localGuild.guildId==7 && g_session.m_localGuild.emblemVersion==1
                && g_session.m_localGuild.name=="Test Guild" && g_session.m_localGuild.title=="Guild Master";
            if(const char* tab=std::getenv("RO_HEAL_PREVIEW_GUILD_WINDOW")) {
                auto make=[](u16 op,int size){std::vector<u8> p(size);p[0]=static_cast<u8>(op);p[1]=static_cast<u8>(op>>8);p[2]=static_cast<u8>(size);p[3]=static_cast<u8>(size>>8);return p;};
                auto put=[](std::vector<u8>& p,int at,u32 value){for(int i=0;i<4;++i)p[at+i]=static_cast<u8>(value>>(8*i));};
                auto text=[](std::vector<u8>& p,int at,const char* str){std::memcpy(p.data()+at,str,std::strlen(str));};
                bool allFramed=true;
                auto deliver=[&](const std::vector<u8>& p){PacketView view{};int consumed=0;const bool framed=TryReadPacket(p.data(),static_cast<int>(p.size()),view,consumed)&&consumed==static_cast<int>(p.size());allFramed &= framed;if(framed)router.Dispatch(mode,view);};
                g_session.m_localGuild.isMaster=true;
                auto basic=make(0x1b6,114);put(basic,2,7);put(basic,6,12);put(basic,10,3);put(basic,14,36);put(basic,18,88);put(basic,22,120000);put(basic,26,250000);put(basic,42,1);
                text(basic,46,"Test Guild");text(basic,70,"Test Player");text(basic,94,"No castles");deliver(basic);
                auto names=make(0x166,4+28*3);auto positions=make(0x160,4+16*3);
                const char* titles[]={"Guild Master","Officer","Member"};
                for(int i=0;i<3;++i){put(names,4+i*28,i);text(names,8+i*28,titles[i]);put(positions,4+i*16,i);put(positions,8+i*16,i<2?17:0);put(positions,16+i*16,5);}
                deliver(names);deliver(positions);
                auto members=make(0x154,4+104*4);const char* players[]={"Test Player","Kafra Enthusiast","Rune Knight","Merchant"};
                for(int i=0;i<4;++i){int at=4+i*104;put(members,at,100+i);put(members,at+4,i?200+i:player.m_gid);members[at+14]=8;members[at+16]=90+i;put(members,at+18,15000+i*3000);put(members,at+22,i!=3);put(members,at+26,i?2:0);text(members,at+80,players[i]);}deliver(members);
                auto notice=make(0x16f,182);text(notice,2,"Welcome to Test Guild");text(notice,62,"Meet in Prontera before tonight's hunt. Bring potions, and help our newer members!");deliver(notice);
                auto relations=make(0x14c,36);put(relations,8,22);text(relations,12,"Midgard Explorers");deliver(relations);
                auto bans=make(0x163,92);text(bans,4,"Former Member");text(bans,52,"Inactive");deliver(bans);
                auto skills=make(0x162,6+37*3);skills[4]=2;
                for(int i=0;i<3;++i){int at=6+37*i;const int id=10000+i;skills[at]=static_cast<u8>(id);skills[at+1]=static_cast<u8>(id>>8);skills[at+6]=1;skills[at+36]=1;}deliver(skills);
                if(std::strcmp(tab,"member")==0)g_session.m_localGuild.isMaster=false;
                if(std::strcmp(tab,"none")==0){g_session.m_localGuild={};g_session.m_guildWindow={};g_session.ClearGuildSkillItems();}
                const bool key=g_windowMgr.OnQtKeyDown('G',true,false,false);
                auto* guild=g_windowMgr.m_guildWnd;
                const bool opened=key && guild && guild->m_show;
                g_windowMgr.OnQtKeyDown('G',true,false,false);
                const bool closed=guild && !guild->m_show;
                g_windowMgr.OnQtKeyDown('G',true,false,false);
                if(guild)guild->tab=std::clamp(std::atoi(tab),0,5);
                DbgLog("[GuildWindowPreview] altOpen=%d altClose=%d loaded=%d members=%u positions=%u skills=%u tab=%s framed=%d\n",opened,closed,g_session.m_guildWindow.loaded,
                    static_cast<unsigned>(g_session.m_guildWindow.members.size()),static_cast<unsigned>(g_session.m_guildWindow.positions.size()),static_cast<unsigned>(g_session.GetGuildSkillItems().size()),tab,allFramed);
            }
            if(const char* commerce=std::getenv("RO_HEAL_PREVIEW_COMMERCE")) {
                using namespace ro::commerce;Reset();UpdateTradeContext(GetTickCount(),{{999,{"Other Player",1}}});g_session.ClearInventoryItems();
                ITEM_INFO potion;potion.SetItemId(501);potion.m_itemIndex=2;potion.m_itemType=0;potion.m_isIdentified=1;potion.m_num=8;g_session.SetInventoryItem(potion);
                ITEM_INFO armor;armor.SetItemId(2301);armor.m_itemIndex=3;armor.m_itemType=5;armor.m_isIdentified=1;armor.m_num=1;armor.m_location=16;g_session.SetInventoryItem(armor);
                auto* inventory=static_cast<UIItemWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_ITEMWND));inventory->Move(10,60);inventory->SetShow(1);
                state.hasCart=true;state.cart.visible=true;state.cart.x=385;state.cart.y=60;
                potion.SetItemId(502);potion.m_num=20;state.cartCount=1;state.cartMaxWeight=80000;
                std::array<u8,26> cartPacket{};cartPacket[0]=0xe9;cartPacket[1]=2;cartPacket[2]=26;cartPacket[4]=2;cartPacket[6]=0xf6;cartPacket[7]=1;cartPacket[9]=1;cartPacket[10]=20;
                PacketView framed{};int consumed=0;
                const bool cartRoute=TryReadPacket(cartPacket.data(),int(cartPacket.size()),framed,consumed)&&router.Dispatch(mode,framed)&&state.cartItems.count(2);
                std::array<u8,30> cartEquip{};cartEquip[0]=0xd2;cartEquip[1]=2;cartEquip[2]=30;cartEquip[4]=3;cartEquip[6]=0xfd;cartEquip[7]=8;cartEquip[8]=5;cartEquip[9]=1;cartEquip[10]=16;
                const bool equipRoute=TryReadPacket(cartEquip.data(),int(cartEquip.size()),framed,consumed)&&router.Dispatch(mode,framed)&&state.cartItems.count(3);
                const u8 removed[]={0x25,1,3,0,1,0,0,0};router.Dispatch(mode,{0x125,8,removed});
                std::array<u8,34> friendRequest{};friendRequest[0]=7;friendRequest[1]=2;friendRequest[2]=77;friendRequest[6]=88;std::memcpy(friendRequest.data()+10,"Test Friend",11);
                router.Dispatch(mode,{0x207,34,friendRequest.data()});const bool friendRoute=state.promptKind=="friend";state.promptKind.clear();
                DbgLog("[CommercePreview] timedCartRoute=%d equipmentCartRoute=%d friendPromptRoute=%d\n",cartRoute,equipRoute,friendRoute);
                std::string type=commerce;
                if(type=="shopboard"){
                    // Reproduce a buffed blacksmith entering another client's
                    // screen. OPTION=cart; OPT3=Overthrust/Energy Coat/Aura Blade.
                    std::array<u8,60> spawn{};
                    const auto op=ro::net::GetActiveMapReceiveProfile().actorSpawnModernIdleFont;
                    spawn[0]=u8(op);spawn[1]=u8(op>>8);spawn[2]=0xe7;spawn[3]=3;
                    spawn[6]=150;spawn[13]=4;spawn[16]=10;spawn[18]=1;spawn[44]=0x46;spawn[49]=1;
                    spawn[50]=12;spawn[51]=0x83;spawn[52]=0x20;spawn[53]=spawn[54]=5;spawn[56]=90;
                    router.Dispatch(mode,{op,60,spawn.data()});
                    auto found=mode.m_runtimeActors.find(999);
                    const bool visible=found!=mode.m_runtimeActors.end()&&found->second->m_isVisible&&found->second->m_isVisibleBody&&found->second->m_effectState==0x400;
                    if(visible)found->second->m_pos={40,0,0};
                    std::vector<u8> stream(10+33+9);
                    stream[0]=0xeb;stream[1]=1;stream[2]=0xe7;stream[3]=3;stream[6]=156;stream[8]=179;
                    const auto act=ro::net::GetActiveMapReceiveProfile().actorActionNotifyExtended;
                    stream[10]=u8(act);stream[11]=u8(act>>8);stream[12]=0xe7;stream[13]=3;stream[38]=2;
                    stream[43]=0xf5;stream[44]=1;stream[45]=3;
                    PacketView pending{};int bytes=0;const bool partial=!TryReadPacket(stream.data(),9,pending,bytes);
                    int at=0,packets=0;
                    while(at<int(stream.size())&&TryReadPacket(stream.data()+at,int(stream.size())-at,pending,bytes)){router.Dispatch(mode,pending);at+=bytes;++packets;}
                    const bool guildMarker=mode.m_guildPosList.count(999)&&mode.m_guildPosList.at(999).x==156&&mode.m_guildPosList.at(999).y==179;
                    std::array<u8,10> removeMarker{};removeMarker[0]=0xeb;removeMarker[1]=1;removeMarker[2]=0xe7;removeMarker[3]=3;std::fill(removeMarker.begin()+6,removeMarker.end(),0xff);
                    router.Dispatch(mode,{0x1eb,10,removeMarker.data()});
                    bool guildFraming=true;
                    for(const auto& spec:std::vector<std::pair<u16,int>>{{0x14c,-1},{0x14e,6},{0x150,110},{0x152,-1},{0x154,-1},{0x156,-1},{0x15a,66},{0x15c,90},{0x15e,6},{0x15f,42},{0x160,-1},{0x162,-1},{0x163,-1},{0x166,-1},{0x167,3},{0x169,3},{0x16a,30},{0x16c,43},{0x16d,14},{0x16f,182},{0x171,30},{0x173,3},{0x174,-1},{0x176,106},{0x17f,-1},{0x181,3},{0x182,106},{0x184,10},{0x185,34},{0x1b4,12},{0x1b6,114},{0x1eb,10}}){
                        std::vector<u8> framed(spec.second<0?4:spec.second);framed[0]=u8(spec.first);framed[1]=u8(spec.first>>8);if(spec.second<0)framed[2]=4;
                        PacketView value{};int count=0;guildFraming &= TryReadPacket(framed.data(),int(framed.size()),value,count)&&count==int(framed.size());
                    }
                    std::array<u8,10> guildChat{};guildChat[0]=0x7f;guildChat[1]=1;guildChat[2]=10;std::memcpy(guildChat.data()+4,"Hello",6);router.Dispatch(mode,{0x17f,10,guildChat.data()});
                    DbgLog("[CommercePreview] guildMarker=%d guildMarkerRemoved=%d guildFraming=%d guildChat=%d\n",guildMarker,!mode.m_guildPosList.count(999),guildFraming,mode.m_lastChat=="Hello");
                    const bool seated=visible&&found->second->m_isSitting;
                    const bool accepted=state.tradeActive&&state.trade.width==660;
                    stream[38]=3;router.Dispatch(mode,{act,33,stream.data()+10});
                    DbgLog("[CommercePreview] guildPositionStream=%d packets=%d sit=%d stand=%d tradeAccepted=%d\n",partial&&at==int(stream.size()),packets,seated,visible&&!found->second->m_isSitting,accepted);
                    const u8 cancel[]={0xee,0};router.Dispatch(mode,{0xee,2,cancel});
                    std::array<u8,86> board{};board[0]=0x31;board[1]=1;board[2]=0xe7;board[3]=3;std::memcpy(board.data()+6,"Midgard's forge",16);
                    router.Dispatch(mode,{0x131,86,board.data()});
                    CHARACTER_INFO selected{};selected.speed=150;g_session.m_speed=112;g_session.SetSelectedCharacterAppearance(selected);
                    FRIEND_INFO member{};member.AID=999;member.characterName="Test Vendor";member.partyHp=25;member.partyMaxHp=100;
                    g_session.AddMemberToParty(member);mode.m_actorNameListByGID[999].name=member.characterName;
                    bool cartDepth=false;
                    if(visible&&found->second->m_cartEffect){
                        auto* cart=found->second->m_cartEffect;cart->m_pos=cart->m_orgPos=found->second->m_pos;cart->m_followInitialized=1;cart->m_followRenderInitialized=0;
                        matrix view=camera.GetViewMatrix();const auto begin=g_renderer.m_rpAlphaList.size();
                        const bool drawn=RenderAttachedActorEffectPass(*cart,&view,0.001f,-10000,true,12345);
                        cartDepth=drawn&&g_renderer.m_rpAlphaList.size()>begin;
                        for(size_t i=begin;i<g_renderer.m_rpAlphaList.size();++i)for(int k=0;k<g_renderer.m_rpAlphaList[i].second->numVerts;++k)cartDepth &= g_renderer.m_rpAlphaList[i].second->verts[k].z>0;
                        g_renderer.m_rpAlphaList.resize(begin);
                    }
                    DbgLog("[CommercePreview] frontCartWorldDepth=%d\n",cartDepth);
                    DbgLog("[CommercePreview] buffedBlacksmithVisible=%d selectedSpeed=%u boardReceived=%d\n",visible,g_session.m_speed,state.shopBoards.count(999)!=0);
                    state.cart.visible=false;inventory->SetShow(0);
                }
                if(type=="vending"){const u8 open[]={0x2d,1,4,0};router.Dispatch(mode,{0x12d,4,open});state.vending.x=800;state.vending.y=60;state.draft.push_back({potion,3,250});}
                if(type=="trade"||type=="incoming"){state.cart.visible=false;state.trade.visible=true;state.trade.x=390;state.trade.y=60;state.partner="Trading Partner";state.tradeIncoming=type=="incoming";state.tradeActive=type=="trade";if(state.tradeActive){state.trade.width=660;state.trade.height=420;}state.localOffer.push_back({potion,2,0});state.remoteOffer.push_back({armor,1,0});state.remoteZeny=500;}
                if(type=="mail"){state.cart.visible=false;state.mail.visible=true;state.mailTo="Recipient";}
                if(type=="equipment"){
                    state.cart.visible=false;std::array<u8,69> viewEquip{};viewEquip[0]=0xd7;viewEquip[1]=2;viewEquip[2]=69;std::memcpy(viewEquip.data()+4,"Test Player",11);std::copy(cartEquip.begin()+4,cartEquip.end(),viewEquip.begin()+43);
                    router.Dispatch(mode,{0x2d7,69,viewEquip.data()});DbgLog("[CommercePreview] equipmentViewRoute=%d\n",state.equipment.visible&&state.viewedEquipment.size()==1);
                }
                if(type=="prompt"){state.cart.visible=false;state.promptKind="guild";state.promptName="Example Guild";state.promptId=7;}
                if(type=="context"){
                    auto* menu=static_cast<UIPlayerContextMenuWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_PLAYERCONTEXTMENUWND));
                    std::vector<PlayerContextMenuEntry> entries;for(int i=0;i<8;++i)entries.push_back({i,"Action"});menu->SetMenu(999,"Other Player",entries,700,70);
                }
                if(const char* port=std::getenv("RO_HEAL_PREVIEW_COMMERCE_PORT"))CRagConnection::instance()->Connect("127.0.0.1",std::atoi(port));
                DbgLog("[CommercePreview] case=%s cartItems=%u inventory=%u\n",commerce,unsigned(state.cartItems.size()),unsigned(g_session.GetInventoryItems().size()));
            }
            int labelX=0,labelY=0;g_world.GetPlayerScreenLabel(camera.GetViewMatrix(),0,&labelX,&labelY);
            const int mouseY=labelY-40;
            QtUiStateAdapter adapter;adapter.syncGameplay(mode,GetRenderDevice().GetBackendType(),GetRenderDevice().GetBackendType(),labelX,mouseY);
            const auto* state=static_cast<QtUiState*>(adapter.stateObject());
            if(const char* fixture=std::getenv("RO_HEAL_PREVIEW_COMMERCE");fixture&&std::strcmp(fixture,"shopboard")==0){
                const auto metrics=ResolvePlayerVitalsScreenMetrics(&camera);bool matching=false;
                for(const auto& value:state->anchors()){const auto a=value.toMap();if(a.value("kind")=="partyHpBar")matching=a.value("width").toInt()==metrics.barWidth+2*metrics.border&&a.value("height").toInt()==metrics.barHeight+2*metrics.border&&a.value("z").toInt()<0&&a.value("currentHp").toInt()==25;}
                DbgLog("[CommercePreview] partyHpMatchesPlayer=%d\n",matching);
                bool aligned=true;int samples=0;
                if(auto peer=mode.m_runtimeActors.find(999);peer!=mode.m_runtimeActors.end()){
                    for(float distance:{250.0f,500.0f,800.0f}){
                        camera.SetInitialCamera(0,-45,distance);camera.OnCalcViewInfo();
                        const auto metrics=ResolvePlayerVitalsScreenMetrics(&camera);
                        const auto saved=player.m_pos;player.m_pos=peer->second->m_pos;
                        int selfX=0,selfY=0;g_world.GetPlayerScreenLabel(camera.GetViewMatrix(),0,&selfX,&selfY);player.m_pos=saved;
                        int x=0,top=0,y=0;g_world.GetActorScreenMarker(camera.GetViewMatrix(),0,999,&x,&top,&y);
                        aligned &= x==selfX&&y==selfY;
                        adapter.syncGameplay(mode,GetRenderDevice().GetBackendType(),GetRenderDevice().GetBackendType(),x,top+4);
                        bool hp=false,name=false;
                        for(const auto& value:state->anchors()){
                            const auto a=value.toMap();
                            if(a.value("kind")=="partyHpBar")hp=a.value("y").toInt()==selfY+metrics.topDelta;
                            if(a.value("text").toString().startsWith("Test Vendor"))name=a.value("y").toInt()==selfY+metrics.topDelta+metrics.totalHeight+metrics.namePadding;
                        }
                        aligned &= hp&&name;++samples;
                    }
                }
                camera.SetInitialCamera(0,-45,500);camera.OnCalcViewInfo();
                adapter.syncGameplay(mode,GetRenderDevice().GetBackendType(),GetRenderDevice().GetBackendType(),labelX,mouseY);
                DbgLog("[CommercePreview] remotePlayerAnchorsAligned=%d zoomSamples=%d\n",aligned,samples);

            }
            if(std::getenv("RO_HEAL_PREVIEW_GUILD_WINDOW"))DbgLog("[GuildWindowPreview] visible=%d joined=%d canEdit=%d\n",state->guildWindow().value("visible").toBool(),state->guildWindow().value("joined").toBool(),state->guildWindow().value("isMaster").toBool());
            bool name=false,guild=false,emblem=false,belowBars=true;int nameplates=0;
            const auto metrics=ResolvePlayerVitalsScreenMetrics(&camera);
            for(const auto& value:state->anchors()) {
                const auto anchor=value.toMap();const QString text=anchor.value("text").toString();
                if(text.contains("Test Party")){name=true;++nameplates;}
                if(anchor.value("guildText").toString().contains("Guild Master")){guild=true;emblem=!anchor.value("emblemSource").toString().isEmpty();}
                if(text.contains("Test Party"))
                    belowBars &= anchor.value("y").toInt()>=labelY+metrics.topDelta+metrics.totalHeight;
            }
            adapter.syncGameplay(mode,GetRenderDevice().GetBackendType(),GetRenderDevice().GetBackendType(),0,0);
            bool cleared=true;
            for(const auto& value:state->anchors())if(value.toMap().value("text").toString().contains("Test Party"))cleared=false;
            DbgLog("[SelfHoverPreview] framed=%d handled=%d nameParty=%d guildRank=%d emblem=%d belowBars=%d corruptIgnored=%d cleared=%d persisted=%d singleBubble=%d\n",framed,handled,name,guild,emblem,belowBars,corruptIgnored,cleared,persisted,nameplates==1);
            SendMessageW(g_hMainWnd,WM_MOUSEMOVE,0,MAKELPARAM(labelX,mouseY));
            const int width=g_renderer.m_width,height=g_renderer.m_height;
            chatTexture.Create(width,height,PF_A8R8G8B8,false);
            for(int i=0;i<3;++i){RenderQtUiGameplayOverlayTexture(mode,&chatTexture,width,height);ProcessQtUiRuntimeEvents();}
            if(const char* input=std::getenv("RO_HEAL_PREVIEW_COMMERCE_INPUT")) {
                using namespace ro::commerce;
                auto& commerceState=ro::commerce::state;
                auto render=[&]{RenderQtUiGameplayOverlayTexture(mode,&chatTexture,width,height);ProcessQtUiRuntimeEvents();};
                QQuickItem* scene=nullptr;for(auto* w:QGuiApplication::allWindows())if(w->title()=="open-midgard-qt-ui")if(auto* quick=qobject_cast<QQuickWindow*>(w))scene=quick->contentItem();
                std::function<QQuickItem*(QQuickItem*,const QString&)> find=[&](QQuickItem* root,const QString& name)->QQuickItem* {
                    if(!root)return nullptr;if(root->objectName()==name && root->isVisible())return root;
                    for(auto* child:root->childItems())if(auto* result=find(child,name))return result;return nullptr;
                };
                auto item=[&](const QString& name){return find(scene,name);};
                auto point=[&](QQuickItem* target){return target->mapToScene(QPointF(target->width()/2,target->height()/2)).toPoint();};
                auto click=[&](const QString& name){auto* target=item(name);if(!target)return false;const auto p=point(target);const bool down=HandleQtUiRuntimeWindowMessage(WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(p.x(),p.y()));const bool up=HandleQtUiRuntimeWindowMessage(WM_LBUTTONUP,0,MAKELPARAM(p.x(),p.y()));render();return down&&up;};
                auto drag=[&](const QString& source,QPoint to){auto* target=item(source);if(!target)return false;const auto from=point(target);
                    HandleQtUiRuntimeWindowMessage(WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(from.x(),from.y()));
                    for(int step=1;step<=5;++step){const auto p=from+(to-from)*step/5;HandleQtUiRuntimeWindowMessage(WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(p.x(),p.y()));render();}
                    HandleQtUiRuntimeWindowMessage(WM_LBUTTONUP,0,MAKELPARAM(to.x(),to.y()));render();return true;
                };
                auto quantity=[&]{auto* field=item("commerceQuantity");if(!field)return false;HandleQtUiRuntimeWindowMessage(WM_CHAR,'3',0);render();return field->property("text").toString()=="3" && click("commerceQuantityConfirm");};
                bool result=false;const std::string type=input;
                if(type=="inventory"){
                    const bool toggled=click("commerceCartButton")&&!commerceState.cart.visible&&click("commerceCartButton")&&commerceState.cart.visible;
                    const bool there=drag("commerceTile_inventory_2",QPoint(550,260))&&quantity();
                    const bool back=drag("commerceTile_cart_2",QPoint(200,220))&&quantity();
                    DbgLog("[CommerceInputPreview] toggle=%d inventoryToCart=%d cartToInventory=%d\n",toggled,there,back);
                    result=toggled&&there&&back;
                }else if(type=="vending"){
                    commerceState.draft.clear();render();const bool added=drag("commerceTile_cart_2",QPoint(1000,270))&&commerceState.draft.size()==1;
                    const bool removed=drag("commerceTile_vending_2",QPoint(1250,650))&&commerceState.draft.empty();result=added&&removed;
                    DbgLog("[CommerceInputPreview] vendAdd=%d vendRemove=%d\n",added,removed);
                }else if(type=="trade"){
                    commerceState.localOffer.clear();render();const bool offered=drag("commerceTile_inventory_2",QPoint(550,220))&&quantity()&&commerceState.tradeAddPending;
                    const u8 ack[]={0xea,0,2,0,0};Receive(mode,{0xea,5,ack});render();
                    const bool locked=click("commerceLockTrade")&&commerceState.lockPending;
                    const u8 local[]={0xec,0,0},remote[]={0xec,0,1};Receive(mode,{0xec,3,local});Receive(mode,{0xec,3,remote});render();
                    const bool committed=click("commerceCommitTrade")&&commerceState.commitPending;result=offered&&locked&&committed;
                }else if(type=="context")result=click("commerceContext1")&&commerceState.tradeWaiting;
                else if(type=="prompt")result=click("commercePromptAccept")&&commerceState.promptKind.empty();
                else if(type=="shopboard"){
                    const bool browsed=click("commerceShopBoard_999");
                    std::array<u8,30> stock{};stock[0]=0x33;stock[1]=1;stock[2]=30;stock[4]=0xe7;stock[5]=3;
                    stock[8]=250;stock[12]=3;stock[14]=2;stock[17]=0xf5;stock[18]=1;stock[19]=1;
                    router.Dispatch(mode,{0x133,30,stock.data()});render();
                    result=browsed&&commerceState.shop.visible&&click("commerceBuy_2");
                }
                DbgLog("[CommerceInputPreview] case=%s passed=%d\n",input,result);
            }
            {
                std::function<QQuickItem*(QQuickItem*)> findEmblem=[&](QQuickItem* root)->QQuickItem* {
                    if(root->objectName()=="hoverGuildEmblem" && root->isVisible() && root->width()>0)return root;
                    for(auto* child:root->childItems())if(auto* found=findEmblem(child))return found;
                    return nullptr;
                };
                bool spansRows=false;
                for(auto* window:QGuiApplication::allWindows())if(window->title()=="open-midgard-qt-ui")if(auto* quick=qobject_cast<QQuickWindow*>(window))
                    if(auto* icon=findEmblem(quick->contentItem())) {
                        auto* name=icon->parentItem()->findChild<QQuickItem*>("hoverPlayerName");
                        auto* guild=icon->parentItem()->findChild<QQuickItem*>("hoverGuildName");
                        if(name && guild){
                            const auto bounds=icon->mapRectToScene(icon->boundingRect());
                            const auto nameBounds=name->mapRectToScene(name->boundingRect()),guildBounds=guild->mapRectToScene(guild->boundingRect());
                            spansRows=icon->width()>24 && icon->width()==icon->height() && std::abs(bounds.top()-nameBounds.top())<=1 && std::abs(bounds.bottom()-guildBounds.bottom())<=1
                                && bounds.right()<nameBounds.left() && bounds.right()<guildBounds.left();
                        }
                    }
                DbgLog("[GuildEmblemPreview] spansBothRows=%d\n",spansRows);
            }
            if(std::getenv("RO_HEAL_PREVIEW_GUILD_INPUT") && g_windowMgr.m_guildWnd) {
                auto render=[&](){RenderQtUiGameplayOverlayTexture(mode,&chatTexture,width,height);ProcessQtUiRuntimeEvents();};
                QQuickItem* scene=nullptr;
                for(auto* window:QGuiApplication::allWindows())if(window->title()=="open-midgard-qt-ui")if(auto* quick=qobject_cast<QQuickWindow*>(window))scene=quick->contentItem();
                std::function<QQuickItem*(QQuickItem*,const QString&)> find=[&](QQuickItem* root,const QString& name)->QQuickItem* {
                    if(!root)return nullptr;if(root->objectName()==name)return root;
                    for(auto* child:root->childItems())if(auto* match=find(child,name))return match;return nullptr;
                };
                auto item=[&](const QString& name){return find(scene,name);};
                auto point=[&](QQuickItem* target){return target->mapToScene(QPointF(target->width()/2,target->height()/2)).toPoint();};
                auto click=[&](const QString& name){auto* target=item(name);if(!target)return false;const auto p=point(target);
                    HandleQtUiRuntimeWindowMessage(WM_MOUSEMOVE,0,MAKELPARAM(p.x(),p.y()));
                    const bool down=HandleQtUiRuntimeWindowMessage(WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(p.x(),p.y()));
                    const bool up=HandleQtUiRuntimeWindowMessage(WM_LBUTTONUP,0,MAKELPARAM(p.x(),p.y()));render();return down&&up;};
                auto* guild=g_windowMgr.m_guildWnd;
                bool tabs=true;
                for(int tab:{1,2,3,4,5,0})tabs &= click("guildTab"+QString::number(tab)) && guild->tab==tab;
                bool dragged=false;
                if(auto* header=item("guildDragHeader")) {
                    const int oldX=guild->m_x,oldY=guild->m_y;const auto p=point(header);
                    HandleQtUiRuntimeWindowMessage(WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(p.x(),p.y()));
                    HandleQtUiRuntimeWindowMessage(WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(p.x()+20,p.y()+10));render();
                    HandleQtUiRuntimeWindowMessage(WM_LBUTTONUP,0,MAKELPARAM(p.x()+20,p.y()+10));render();
                    dragged=guild->m_x==oldX+20 && guild->m_y==oldY+10;
                }
                bool edited=click("guildNoticeSubject");
                HandleQtUiRuntimeWindowMessage(WM_KEYDOWN,VK_END,0);
                edited &= HandleQtUiRuntimeWindowMessage(WM_CHAR,0x00f6,0);
                if(auto* editor=item("guildNoticeSubject"))edited &= editor->property("text").toString().endsWith(QChar(0x00f6));else edited=false;
                HandleQtUiRuntimeWindowMessage(WM_KEYDOWN,VK_BACK,0);
                const auto savedMembers=g_session.m_guildWindow.members;
                if(!savedMembers.empty())for(int i=0;i<35;++i){auto member=savedMembers[0];member.character=1000+i;g_session.m_guildWindow.members.push_back(member);}
                click("guildTab1");render();bool scrolled=false;
                if(auto* scroll=item("guildScroll")) {
                    auto p=point(scroll);POINT screen{p.x(),p.y()};ClientToScreen(g_hMainWnd,&screen);
                    HandleQtUiRuntimeWindowMessage(WM_MOUSEWHEEL,MAKEWPARAM(0,static_cast<WORD>(-1200)),MAKELPARAM(screen.x,screen.y));
                    for(int frame=0;frame<12;++frame)render();
                    scrolled=scroll->property("contentY").toReal()>0;
                }
                g_session.m_guildWindow.members=savedMembers;
                click("guildTab0");
                const bool closed=click("guildCloseButton") && !guild->m_show;
                guild->SetShow(1);render();click("guildNoticeSubject");
                const bool escaped=HandleQtUiRuntimeWindowMessage(WM_KEYDOWN,VK_ESCAPE,0) && !guild->m_show;
                guild->SetShow(1);render();click("guildNoticeSubject");
                BYTE savedKeys[256]{},altKeys[256]{};GetKeyboardState(savedKeys);std::memcpy(altKeys,savedKeys,256);altKeys[VK_MENU]=0x80;SetKeyboardState(altKeys);
                SendMessageW(g_hMainWnd,WM_SYSKEYDOWN,'G',0);SetKeyboardState(savedKeys);
                const bool altShortcut=!guild->m_show;
                const bool title=item("guildWindowTitle") && item("guildWindowTitle")->property("text").toString()=="Guild";
                DbgLog("[GuildInputPreview] tabs=%d drag=%d unicodeEdit=%d wheel=%d close=%d title=%d escape=%d altShortcut=%d\n",tabs,dragged,edited,scrolled,closed,title,escaped,altShortcut);
                guild->Move(40,65);guild->SetShow(1);guild->tab=0;render();
            }
            if(RPFace* face=g_renderer.BorrowNullRP()) {
                face->primType=D3DPT_TRIANGLESTRIP;face->verts=face->m_verts;face->numVerts=4;face->indices=nullptr;face->numIndices=0;
                face->tex=&chatTexture;face->mtPreset=0;face->cullMode=D3DCULL_NONE;
                face->srcAlphaMode=D3DBLEND_SRCALPHA;face->destAlphaMode=D3DBLEND_INVSRCALPHA;face->alphaSortKey=3;
                face->m_verts[0]={0,0,0,1,0xFFFFFFFFu,0,0,0};face->m_verts[1]={static_cast<float>(width),0,0,1,0xFFFFFFFFu,0,1,0};
                face->m_verts[2]={0,static_cast<float>(height),0,1,0xFFFFFFFFu,0,0,1};face->m_verts[3]={static_cast<float>(width),static_cast<float>(height),0,1,0xFFFFFFFFu,0,1,1};
                g_renderer.AddRP(face,1|8);
            }
            mode.m_view=nullptr;mode.m_world=nullptr;
        }
#endif
        if (std::getenv("RO_HEAL_PREVIEW_GUI")) {
            unsigned int pixel=0xFF00FFFFu;
            overlayTexture.Create(1,1,PF_A8R8G8B8);
            overlayTexture.Update(0,0,1,1,&pixel,false,4);
            if (RPFace* face=g_renderer.BorrowNullRP()) {
                face->primType=D3DPT_TRIANGLESTRIP;face->verts=face->m_verts;face->numVerts=4;
                face->tex=&overlayTexture;face->mtPreset=0;face->cullMode=D3DCULL_NONE;
                face->srcAlphaMode=D3DBLEND_SRCALPHA;face->destAlphaMode=D3DBLEND_INVSRCALPHA;
                face->alphaSortKey=3;
                face->m_verts[0]={16,16,0,1,0xFFFFFFFFu,0,0,0};
                face->m_verts[1]={48,16,0,1,0xFFFFFFFFu,0,1,0};
                face->m_verts[2]={16,48,0,1,0xFFFFFFFFu,0,0,1};
                face->m_verts[3]={48,48,0,1,0xFFFFFFFFu,0,1,1};
                g_renderer.AddRP(face,1|8);
            }
        }
        g_renderer.DrawScene();GetRenderDevice().Present(false);
        DbgLog("[HealPreview] rendered isolated Heal with installed assets\n");
    }
    g_world.UpdateGameObjects();g_world.m_player=nullptr;
    g_world.ClearBackgroundObjects();g_world.ClearGround();
}
