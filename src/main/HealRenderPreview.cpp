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
#include "qtui/QtUiRuntime.h"
#include "ui/UIWindowMgr.h"
#include "ui/UINewChatWnd.h"
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
#include "Types.h"
#include "DebugLog.h"
#include <windows.h>

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
