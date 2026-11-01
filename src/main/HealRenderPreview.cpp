#include "world/World.h"
#include "world/GameActor.h"
#include "core/Timer.h"
#include "world/RagEffect.h"
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
    const bool room=std::getenv("RO_HEAL_PREVIEW_ROOM")!=nullptr;
    const bool avatar=std::getenv("RO_HEAL_PREVIEW_PLAYER")!=nullptr;
    CPlayer player;player.m_gid=1;player.m_pos=room?vector3d{-232.5f,-10,-12.5f}:vector3d{};g_world.m_player=&player;
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
            if(auto* world=g_resMgr.GetAs<C3dWorldRes>("data\\new_zone01.rsw")) {
                g_world.BuildBackgroundObjects(*world,world->m_lightDir,world->m_diffuseCol,world->m_ambientCol);
                if (std::getenv("RO_HEAL_PREVIEW_GROUND")) {
                    const std::string ground="data\\"+world->m_gndFile;
                    if(auto* gnd=g_resMgr.GetAs<CGndRes>(ground.c_str())) {
                        g_world.BuildGroundFromGnd(*gnd,world->m_lightDir,world->m_diffuseCol,world->m_ambientCol,
                            world->m_waterLevel,world->m_waterType,world->m_waterAnimSpeed,world->m_wavePitch,world->m_waveSpeed,world->m_waveHeight);
                        g_world.m_ground->FlushGround(view);
                    }
                }
                g_world.RenderBackgroundObjects(view);
            }
        }
        if(avatar) {
            vector3d eye{player.m_pos.x,player.m_pos.y-353.553f,player.m_pos.z-353.553f};
            vector3d at=player.m_pos,up{0,1,0};g_renderer.SetLookAt(eye,at,up);
            g_world.RenderActors(view,0);
        } else for (CGameObject* object : g_world.m_gameObjectList) {
            auto* primitive=dynamic_cast<CEffectPrim*>(object);
            if (primitive && primitive->m_type==PP_HEAL) primitive->Render(&view);
        }
        g_renderer.DrawScene();GetRenderDevice().Present(false);
        DbgLog("[HealPreview] rendered isolated Heal with installed assets\n");
    }
    g_world.UpdateGameObjects();g_world.m_player=nullptr;
    g_world.ClearBackgroundObjects();g_world.ClearGround();
}
