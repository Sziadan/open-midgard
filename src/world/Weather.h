#pragma once

class CAbleToMakeEffect;
class CGameActor;
class CRagEffect;

// Snow controller from CWeather649AB0/649B30/649B50. Effects belong to the
// world's game-object list; this process-global controller retains only a handle.
class CWeather {
public:
    void LaunchSnow();
    void StopSnow();
    void Process();
    void Reset();
    void OnActorDeleted(const CGameActor* actor);
    static bool IsSnowMap(const char* map);

    CAbleToMakeEffect* m_pPlayer=nullptr;
    CRagEffect* m_Effect=nullptr;
};

extern CWeather g_Weather;
