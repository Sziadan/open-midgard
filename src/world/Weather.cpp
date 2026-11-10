#include "Weather.h"
#include "GameActor.h"
#include "RagEffect.h"
#include "World.h"
#include "gamemode/GameMode.h"
#include <algorithm>
#include <cstring>

CWeather g_Weather;

bool CWeather::IsSnowMap(const char* map)
{
    // Symbol labels aXmas* do not reflect the case of the selected PE bytes.
    return map && (!std::strcmp(map,"xmas.rsw") || !std::strcmp(map,"xmas_fild01.rsw"));
}

void CWeather::LaunchSnow()
{
    // Clear a handle invalidated between simulation and command dispatch before
    // dereferencing it. Defined source behavior is otherwise unchanged.
    Process();
    if(m_Effect){
        const u32 remaining=static_cast<u32>(m_Effect->GetDuration())-static_cast<u32>(m_Effect->GetStateCount());
        s32 signedRemaining;std::memcpy(&signedRemaining,&remaining,sizeof(signedRemaining));
        // Selected PE649AD4 is JLE return. Do not invert this surprising rule.
        if(signedRemaining<=300)return;
    }
    auto* mode=g_modeMgr.GetCurrentGameMode();
    if(!mode || !mode->m_world || !mode->m_world->m_player)return;
    m_pPlayer=dynamic_cast<CAbleToMakeEffect*>(mode->m_world->m_player);
    if(!m_pPlayer)return;
    m_Effect=m_pPlayer->LaunchEffect(162,vector3d{},0.0f);
}

void CWeather::StopSnow()
{
    Process();
    if(m_Effect){
        const u32 end=static_cast<u32>(m_Effect->GetStateCount())+300u;
        s32 signedEnd;std::memcpy(&signedEnd,&end,sizeof(end));
        m_Effect->SendMsg(nullptr,80,signedEnd,0,0);
    }
}

void CWeather::Process()
{
    if(m_Effect && (!m_pPlayer || std::find(m_pPlayer->m_effectList.begin(),m_pPlayer->m_effectList.end(),m_Effect)==m_pPlayer->m_effectList.end()))
        m_Effect=nullptr;
}

void CWeather::Reset()
{
    // Source Reset only forgets the effect. It does not delete/stop it or clear
    // the retained player value; map teardown owns actor/effect destruction.
    m_Effect=nullptr;
}

void CWeather::OnActorDeleted(const CGameActor* actor)
{
    if(m_pPlayer==actor){Reset();m_pPlayer=nullptr;}
}
