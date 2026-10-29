#include <cstdint>
#include <cstdio>
#include <list>
#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

using u8 = std::uint8_t;
using u32 = std::uint32_t;
using msgparam_t = std::intptr_t;
struct vector3d { float x = 0, y = 0, z = 0; };
struct matrix {};
u32 nowTick = 321;
u32 timeGetTime() { return nowTick; }
struct CGameObject {
    virtual ~CGameObject() = default;
    virtual void SendMsg(CGameObject*, int, msgparam_t, msgparam_t, msgparam_t) {}
};
struct CMsgEffect;
struct World { std::list<CGameObject*> m_gameObjectList; } g_world;
struct CGameActor : CGameObject {
    u32 m_gid = 0;
    vector3d m_pos;
    int m_oldstateId = 0, m_stateId = 0;
    CMsgEffect *m_iceEffect = nullptr, *m_curseEffect = nullptr, *m_sleepEffect = nullptr;
    CMsgEffect *m_stunEffect = nullptr, *m_silenceEffect = nullptr, *m_angelusEffect = nullptr;
    std::list<CMsgEffect*> m_msgEffectList;
    int stateChanges = 0;
    void SetState(int state) { m_stateId = state; ++stateChanges; }
    CMsgEffect* SpawnMsgSprite(int processType, const char* spritePath, const char* actPath, int action, float zoom);
    bool ProcessStatusMsgEffectMessage(int msg);
};
struct CMsgEffect : CGameObject {
    int m_msgEffectType = 0, m_digit = 0, m_numberValue = 0, m_sprShift = 0;
    int m_spriteActionIndex = 0, m_spriteMotionIndex = 0;
    int m_lastProcessedSpriteMotionIndex = -1, m_alpha = 0;
    u32 m_masterGid = 0, m_colorArgb = 0, m_stateStartTick = 0;
    vector3d m_pos, m_orgPos;
    float m_zoom = 0, m_orgZoom = 0, m_followTravelDistance = 0;
    CGameActor* m_masterActor = nullptr;
    u8 m_isVisible = 0, m_isDisappear = 0, m_followInitialized = 0, m_followRenderInitialized = 0;
    u32 m_followRenderTick = 0;
    std::string m_customSpritePath, m_customActPath;
    bool ProcessOwnerMessage(CGameObject* sender, int msg);
    void SendMsg(CGameObject* sender, int msg, msgparam_t par1, msgparam_t par2, msgparam_t par3) override;
};
constexpr int kAttachedActorSpriteMsgEffectType = 3000;
void InitializeAttachedActorTrailPosition(CMsgEffect&) {}
void ProcessAttachedActorSpriteState(CMsgEffect&) {}
bool ShouldTraceAttachedCartEffect(const CMsgEffect&) { return false; }
bool IsAnimatedSpriteBackedMsgEffectType(int) { return false; }
bool TryPlayMsgSpriteMotionEvent(const CMsgEffect&, int) { return false; }
void DbgLog(const char*, ...) {}

#include "MsgEffectStateLifecycleBodies.inc"

int failures = 0;
void Check(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); ++failures; }
}
void CheckSprite(CMsgEffect* effect, int type, const char* path, const char* act, int action) {
    Check(effect != nullptr, "effect was spawned");
    if (!effect) return;
    Check(effect->m_msgEffectType == type, "process type");
    if (effect->m_customSpritePath != std::string("data\\sprite\\") + path) {
        std::fprintf(stderr, "sprite path actual=[%s] expected=[%s]\n", effect->m_customSpritePath.c_str(), (std::string("data\\sprite\\") + path).c_str());
        Check(false, "sprite resource path");
    }
    if (effect->m_customActPath != std::string("data\\sprite\\") + act) {
        std::fprintf(stderr, "action path actual=[%s] expected=[%s]\n", effect->m_customActPath.c_str(), (std::string("data\\sprite\\") + act).c_str());
        Check(false, "action resource path");
    }
    Check(effect->m_spriteActionIndex == action, "action index");
    Check(effect->m_zoom == 1.0f && effect->m_orgZoom == 1.0f, "zoom");
    Check(effect->m_isVisible == 1 && effect->m_alpha == 255, "visible opaque sprite");
    Check(effect->m_stateStartTick == nowTick, "state tick initialized");
}
int main() {
    CGameActor actor;
    actor.m_gid = 0x1234;
    actor.m_pos = {12.0f, 34.0f, 56.0f};
    actor.m_oldstateId = 4;

    actor.ProcessStatusMsgEffectMessage(58);
    auto* ice = actor.m_iceEffect;
    CheckSprite(ice, 5, "\xC0\xCC\xC6\xD1\xC6\xAE\\\xBE\xF3\xC0\xBD\xB6\xAF.spr",
        "\xC0\xCC\xC6\xD1\xC6\xAE\\\xBE\xF3\xC0\xBD\xB6\xAF.act", 0);
    Check(ice && ice->m_masterGid == actor.m_gid && ice->m_masterActor == &actor, "ice ownership attached");
    Check(ice && ice->m_pos.x == 12 && ice->m_pos.y == 34 && ice->m_pos.z == 56, "spawn position copied");
    Check(g_world.m_gameObjectList.size() == 1 && actor.m_msgEffectList.size() == 1, "attached to world and actor lists");
    actor.ProcessStatusMsgEffectMessage(58);
    Check(actor.m_iceEffect == ice && actor.m_msgEffectList.size() == 1, "ice duplicate suppressed");
    actor.ProcessStatusMsgEffectMessage(59);
    Check(actor.m_iceEffect == nullptr && ice->m_isDisappear == 1, "ice release marks effect and clears slot");

    actor.ProcessStatusMsgEffectMessage(62);
    actor.ProcessStatusMsgEffectMessage(63);
    Check(actor.m_msgEffectList.size() == 3, "transient ice and smoke effects are emitted per message");
    auto it = actor.m_msgEffectList.begin(); ++it;
    CheckSprite(*it++, 6, "\xC0\xCC\xC6\xD1\xC6\xAE\\\xBE\xF3\xC0\xBD\xB6\xAF.spr",
        "\xC0\xCC\xC6\xD1\xC6\xAE\\\xBE\xF3\xC0\xBD\xB6\xAF.act", 1);
    CheckSprite(*it, 6, "\xC0\xCC\xC6\xD1\xC6\xAE\\smoke.spr",
        "\xC0\xCC\xC6\xD1\xC6\xAE\\smoke.act", 0);

    struct Status { int add, remove, type; const char* asset; CMsgEffect** slot; };
    const Status statuses[] = {
        {67,68,7,"Status-curse",&actor.m_curseEffect}, {69,70,8,"Status-sleep",&actor.m_sleepEffect},
        {71,72,9,"Status-stun",&actor.m_stunEffect}, {73,74,10,"emotion",&actor.m_silenceEffect},
        {75,76,11,"msg",&actor.m_angelusEffect}
    };
    for (const auto& status : statuses) {
        const int before = static_cast<int>(actor.m_msgEffectList.size());
        actor.ProcessStatusMsgEffectMessage(status.add);
        CMsgEffect* effect = *status.slot;
        char sprite[96], act[96];
        std::snprintf(sprite, sizeof(sprite), "\xC0\xCC\xC6\xD1\xC6\xAE\\%s.spr", status.asset);
        std::snprintf(act, sizeof(act), "\xC0\xCC\xC6\xD1\xC6\xAE\\%s.act", status.asset);
        const int action = status.type == 10 ? 10 : status.type == 11 ? 1 : 0;
        CheckSprite(effect, status.type, sprite, act, action);
        Check(effect && effect->m_masterActor == &actor && effect->m_masterGid == actor.m_gid, "status effect owner attached");
        actor.ProcessStatusMsgEffectMessage(status.add);
        Check(*status.slot == effect && static_cast<int>(actor.m_msgEffectList.size()) == before + 1, "status duplicate suppressed");
        if (status.add == 69) Check(actor.m_stateId == 10 && actor.stateChanges == 1, "sleep sets actor state once");
        actor.ProcessStatusMsgEffectMessage(status.remove);
        Check(*status.slot == nullptr && effect->m_isDisappear == 1, "status release marks effect and clears slot");
        if (status.add == 69) Check(actor.m_stateId == actor.m_oldstateId && actor.stateChanges == 2, "sleep release restores prior actor state");
    }
    const int oldStateChanges = actor.stateChanges;
    actor.ProcessStatusMsgEffectMessage(70);
    Check(actor.stateChanges == oldStateChanges, "sleep release without active effect does not change state");
    Check(actor.ProcessStatusMsgEffectMessage(999) == false, "unhandled message falls through");
    for (auto* effect : actor.m_msgEffectList) delete effect;
    if (failures) return 1;
    std::puts("Message effect status lifecycle checks passed");
    return 0;
}
