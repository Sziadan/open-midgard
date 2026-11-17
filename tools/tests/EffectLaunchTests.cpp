#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <limits>
#include <vector>

struct vector3d { float x = 0, y = 0, z = 0; };
struct CAbleToMakeEffect;

struct CRagEffect {
    static int created;
    int type = -1;
    CRagEffect() { ++created; }
    void Init(CAbleToMakeEffect*, int effectId, const vector3d&) { type = effectId; }
};
int CRagEffect::created = 0;

struct World { std::vector<CRagEffect*> m_gameObjectList; };
struct CGameMode { World* m_world = nullptr; };
struct ModeManager {
    CGameMode* mode = nullptr;
    CGameMode* GetCurrentGameMode() { return mode; }
} g_modeMgr;
struct Session { bool m_isEffectOn = true; } g_session;

struct CAbleToMakeEffect {
    std::vector<CRagEffect*> m_effectList;
    CRagEffect* LaunchEffect(int effectId, vector3d deltaPos, float fRot);
};

#include "LaunchEffectBody.inc"

int main()
{
    World world;
    CGameMode mode{&world};
    g_modeMgr.mode = &mode;
    CAbleToMakeEffect actor;

    for (int id : {-1, -2, std::numeric_limits<int>::min()}) {
        assert(actor.LaunchEffect(id, {}, 0.0f) == nullptr);
    }
    assert(CRagEffect::created == 0);
    assert(world.m_gameObjectList.empty());
    assert(actor.m_effectList.empty());

    for (int id : {0, 29}) {
        CRagEffect* effect = actor.LaunchEffect(id, {}, 0.0f);
        assert(effect && effect->type == id);
        assert(world.m_gameObjectList.back() == effect);
        assert(actor.m_effectList.back() == effect);
    }
    assert(CRagEffect::created == 2);
    g_session.m_isEffectOn = false;
    assert(actor.LaunchEffect(29, {}, 0.0f) == nullptr);
    assert(CRagEffect::created == 2);
    for (CRagEffect* effect : world.m_gameObjectList) delete effect;
}
