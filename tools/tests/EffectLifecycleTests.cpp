#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <list>
#include <cmath>
#include <cstring>
#include <vector>
#include <array>
#include <string>
#include <memory>
#include <type_traits>
#include <set>

using u8 = std::uint8_t;
using u32 = std::uint32_t;
using msgparam_t = std::intptr_t;
int g_isSkipFrame = 1;
int primitiveDeaths = 0, effectDeaths = 0;
struct Probe { int& deaths; ~Probe() { ++deaths; } };
struct vector3d { float x = 0, y = 0, z = 0; };
vector3d AddVec3(vector3d a, vector3d b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
#include "EffectGiTypes.inc"
constexpr int kPrimTypeHeal = 0x3C;
struct matrix { float m[4][4]{}; };
#include "EffectSegmentTypes.inc"
void MatrixIdentity(matrix& m) { m = {}; for (int i = 0; i < 4; ++i) m.m[i][i] = 1; }
void MatrixAppendYRotation(matrix& m, float a) { m.m[2][0] = std::sin(a); m.m[2][2] = std::cos(a); }
void MatrixAppendXRotation(matrix&, float) {}
float GetRadian(int degree) { return degree * 3.14159265f / 180.0f; }
float GetSin(int degree) { return std::sin(GetRadian(degree)); }
float GetCos(int degree) { return std::cos(GetRadian(degree)); }
int rngDraws = 0, waves = 0;
int SilentRand() { ++rngDraws; return 0; }
float waveY = 99;
std::string waveName;
bool PlayWave(const char* name, float, float y, float, int volume, int range, float frequency) {
    ++waves; waveY = y; waveName = name;
    return volume == 250 && range == 40 && frequency == 1;
}
struct CTexture { int m_w = 64, m_h = 64, m_updateWidth = 32, m_updateHeight = 16; };
#include "StrTypes.inc"
struct TexMgr {
    CTexture texture;
    const char* lastPath = nullptr;
    CTexture* GetTexture(const char* path, bool) { lastPath = path; return &texture; }
} g_texMgr;
u32 resourceTick = 123;
u32 timeGetTime() { return resourceTick; }
struct CRes { int m_lockCnt=0; u32 m_timeStamp=0; int Lock(); int Unlock(); };
struct CSprRes : CRes {};
struct CActRes : CRes {};
struct ResourceManager {
    CSprRes spr; CActRes act;
    bool fail = false;
    std::vector<std::string> requests;
    template<class T> T* GetAs(const char* name) {
        requests.emplace_back(name);
        if (fail) return nullptr;
        if constexpr (std::is_same_v<T,CSprRes>) return &spr;
        else return &act;
    }
} g_resMgr;
int lastTextureFrame = -1;
CTexture* KAC_LAYER::GetTexture(int frame) { lastTextureFrame = frame; return &g_texMgr.texture; }
using D3DBLEND = int;
constexpr int D3DPT_TRIANGLESTRIP = 5, D3DCULL_NONE = 1, D3DBLEND_ONE = 2, D3DBLEND_INVSRCALPHA = 6;
struct tlvertex3d { float x=0,y=0,z=0,oow=0; u32 color=0,specular=0; float tu=0,tv=0; };
struct RPFace {
    int primType=0, numVerts=0, numIndices=0, mtPreset=0, cullMode=0;
    tlvertex3d m_verts[6], *verts=nullptr;
    unsigned short* indices=nullptr;
    CTexture* tex=nullptr;
    D3DBLEND srcAlphaMode=0,destAlphaMode=0;
    float alphaSortKey=0;
};
struct SilentRenderer {
    float m_hpc=7, m_vpc=7;
    int m_xoffset=0,m_yoffset=0,m_halfWidth=0,m_halfHeight=0,submissions=0,lastFlags=0;
    RPFace face;
    RPFace* BorrowNullRP() { face = {}; return &face; }
    void AddRP(RPFace*, int flags) { ++submissions; lastFlags=flags; }
} g_renderer;
struct SlashQuad {
    vector3d points[4];
    u32 color;
    float u0,u1,v0,v1;
    D3DBLEND blend;
    int flags;
};
std::vector<SlashQuad> slashQuads;
void SubmitWorldTeiRect(const vector3d& a,const vector3d& b,const vector3d& c,const vector3d& d,
    const matrix&,CTexture*,unsigned int color,float u0,float u1,float v0,float v1,D3DBLEND blend,float,int flags) {
    slashQuads.push_back({{a,b,c,d},color,u0,u1,v0,v1,blend,flags});
}
struct CView {
    matrix view;
    CView() { MatrixIdentity(view); }
    const matrix& GetViewMatrix() const { return view; }
} testView;
struct CGameObject {
    virtual ~CGameObject() = default;
    virtual u8 OnProcess() { return 1; }
    virtual void SendMsg(CGameObject*, int, msgparam_t, msgparam_t, msgparam_t) {}
};
struct CRenderObject : CGameObject {
    vector3d m_pos;
    float m_roty = 0;
    int m_BodyLight = 0;
    char m_BeZero = 0;
};
struct CRagEffect;
struct CEffectPrim;
struct CAbleToMakeEffect : CRenderObject {
    std::list<CRagEffect*> m_effectList;
    CRagEffect* m_beginSpellEffect = nullptr;
    CRagEffect* m_magicTargetEffect = nullptr;
    void RemoveEffectById(int);
    void DetachEffects();
    void SendMsg(CGameObject*, int, msgparam_t, msgparam_t, msgparam_t) override;
    CRagEffect* LaunchEffect(int, vector3d, float);
    int launches = 0;
};
struct CSkill : CGameObject {};
struct CWorld {
    std::list<CGameObject*> m_gameObjectList;
    std::list<CSkill*> m_skillList;
    CRenderObject* m_player = nullptr;
    void UpdateGameObjects();
} g_world;
struct CGameMode { CWorld* m_world = &g_world; CView* m_view = &testView; } gameMode;
struct ModeManager { CGameMode* GetCurrentGameMode() { return &gameMode; } } g_modeMgr;
#include "PrimitiveEnum.inc"
constexpr EFFECTPRIMID Other = PP_BUBBLE_DROP;
void UpdateSlash1Primitive(CEffectPrim*);
void UpdateSuperAngelPrimitive(CEffectPrim*);
struct CEffectPrim : CGameObject {
    Probe deaths{ primitiveDeaths };
    CRagEffect* m_master = nullptr;
    EFFECTPRIMID m_type = Other;
    EffectGi m_gi[4];
    u32 m_renderFlag = 0;
    int m_stateCnt = 0, m_duration = 10, updates = 0, observedFrame = -1, resetFrame = -1;
    int m_pattern = 0, m_spawnCount = 0, m_fadeOutCnt = 0;
    float m_size = 0, m_heightSize = 0, m_longitude = 0, m_latitude = 0, m_speed = 0, m_accel = 0;
    vector3d m_deltaPos, m_deltaPos2, m_pos, m_orgPos;
    float m_roll = 0, m_rollSpeed = 0, m_rollAccel = 0, m_alphaDelta = 0, m_minAlpha = 0;
    bool m_isDisappear = false;
    matrix m_matrix;
    CSprRes* m_superAngelSprRes = nullptr;
    CActRes* m_superAngelActRes = nullptr;
    std::string m_spriteActName,m_spriteSprName;
    int m_spriteAction=0,m_animSpeed=0,m_curMotion=0,m_numSegments=0;
    bool m_repeatAnim=false,m_spriteRepeat=false;
    float m_alpha=0,m_radius=0;
    std::array<EffectSegmentState,12> m_segments;

    std::vector<CTexture*> m_texture;
    bool active = true;
    ~CEffectPrim() override;
    void Init(CRagEffect*, EFFECTPRIMID, const vector3d&);
    void SendMsg(CGameObject*, int, msgparam_t, msgparam_t, msgparam_t) override;
    u8 OnProcess() override;
    bool UsesLegacyBandPayload() const;
    bool ProcessPrimitiveFrame() {
        ++updates;
        observedFrame = m_stateCnt;
        if (m_type == PP_SLASH1) UpdateSlash1Primitive(this);
        if (m_type == PP_SUPERANGEL) UpdateSuperAngelPrimitive(this);
        if (resetFrame >= 0) m_stateCnt = resetFrame;
        return active;
    }
};
int angelSpriteSubmissions=0,angelSubmittedMotion=-1;
u32 angelSubmittedColor=0;
D3DBLEND angelSubmittedBlend=0;
bool SubmitAnimatedSpriteParticle(const CEffectPrim& prim,const vector3d&,matrix*,unsigned int color,D3DBLEND blend,int) {
    ++angelSpriteSubmissions;angelSubmittedMotion=prim.m_curMotion;angelSubmittedColor=color;angelSubmittedBlend=blend;return true;
}
struct CRagEffect : CGameObject {
    Probe deaths{ effectDeaths };
    CRenderObject* m_master = nullptr;
    bool m_ownsMaster = false, m_masterIsActor = false, m_isWorldEffect = false;
    u32 m_masterActorGid = 0;
    int m_type = 0, m_stateCnt = 0, m_duration = 3, m_count = 0, m_level = 0;
    float m_longitude = 0, m_param[4]{}, m_tlvertX=0, m_tlvertY=0, m_tlvertZ=0;
    vector3d m_strScreenOffset, m_strAnchorPosition, m_effectPos, m_cachedPos, m_deltaPos, m_deltaPos2, m_targetPos;
    bool m_hasTargetPos = false;
    void* m_ezEffectRes = nullptr;
    KANICLIP* m_aniClips = nullptr;
    int m_iCurLayer = 0, m_cEndLayer = 0;
    std::array<KAC_XFORMDATA, 128> m_actXformData{};
    std::array<int, 128> m_aiCurAniKey{};
    std::array<u8, 128> m_isLayerDrawn{};
    bool m_strFrameActive = true;
    bool FinishEffectFrame();
    void InitEZ2STRFrame();
    bool ProcessEZ2STR(float x = 0.0f, float y = -80.0f, float z = 0.0f);
    bool ProcessTargetPositionStr();
    void RenderAniClip(int,const KAC_LAYER&,const KAC_XFORMDATA&,matrix*);
    void Render(matrix*);
    std::list<CEffectPrim*> m_primList;
    bool active = true, spawn = false;
    int frames = 0, worldFrames = 0, m_flag = 0;
    vector3d m_grimToothOrigin, m_grimToothRemaining, m_grimToothStep;
    void SpawnGrimTooth();
    void ApplySuperAngel(signed char,signed char);
    void ProcessSuperAngelEffect();
    void ApplyEntry2();
    bool ApplyJobLvUp50(int);
    bool ApplyJobLvUp50_2(short);
    void ProcessJobLvUp50_2();
    ~CRagEffect() override;
    CRenderObject* ResolveCurrentMaster() { return m_master; }
    CAbleToMakeEffect* ResolveCurrentEffectOwner() { return dynamic_cast<CAbleToMakeEffect*>(m_master); }
    bool IsWorldEffect() const { return m_isWorldEffect; }
    const vector3d& EffectPosition() const { return m_effectPos; }
    int GetEffectType() const { return m_type; }
    u8 OnProcess() override;
    bool ProcessEffectFrame() {
        ++frames;
        if (spawn && frames == 1) LaunchEffectPrim(Other, {});
        return active && FinishEffectFrame();
    }
    u8 ProcessWorldEffect() { ++worldFrames; return 1; }
    CEffectPrim* LaunchEffectPrim(EFFECTPRIMID, const vector3d&);
    void DiscardPrimitive(CEffectPrim*);
    void ClearPrims();
    void SendMsg(CGameObject*, int, msgparam_t, msgparam_t, msgparam_t) override;
};

CRagEffect* CAbleToMakeEffect::LaunchEffect(int type, vector3d, float)
{
    ++launches;
    auto* effect = new CRagEffect;
    effect->m_master = this;
    effect->m_type = type;
    m_effectList.push_back(effect);
    g_world.m_gameObjectList.push_back(effect);
    return effect;
}

#define rand SilentRand
#include "EffectLifecycleBodies.inc"
#undef rand

int main()
{
    int failures = 0, checks = 0;
    auto check = [&](bool condition, const char* name) {
        ++checks;
        if (!condition) { ++failures; std::fprintf(stderr, "FAIL: %s\n", name); }
    };
    CAbleToMakeEffect actor;
    g_world.m_player = &actor;
    {
        CRagEffect effect;
        effect.m_master = &actor;
        effect.m_duration = 2;
        check(effect.OnProcess() == 1 && effect.m_stateCnt == 1 && effect.frames == 1,
            "effect advances once without needing wall-clock time");
        check(effect.OnProcess() == 0 && effect.m_stateCnt == 2 && effect.frames == 2,
            "effect expiry is strictly less than duration");
    }
    {
        CRagEffect effect;
        effect.m_master = &actor;
        effect.m_duration = 0;
        check(effect.OnProcess() == 0 && effect.frames == 1 && effect.m_stateCnt == 1,
            "zero-duration effect dispatches before expiry check");
    }
    {
        CRagEffect effect;
        effect.m_master = &actor;
        effect.active = false;
        check(effect.OnProcess() == 0 && effect.m_stateCnt == 0, "handler termination does not increment frame");
        effect.m_master = nullptr;
        const int frames = effect.frames;
        check(effect.OnProcess() == 0 && effect.frames == frames, "missing master terminates before dispatch");
    }
    {
        CRagEffect effect;
        effect.m_master = &actor;
        g_isSkipFrame = 0;
        actor.m_BeZero = 1; actor.m_BodyLight = 7;
        check(effect.OnProcess() == 1 && effect.frames == 0 && actor.m_BodyLight == 7,
            "skip gate leaves frame and actor light unchanged");
        effect.m_type = 25;
        check(effect.OnProcess() == 1 && effect.frames == 1 && actor.m_BodyLight == 7,
            "always-run effect dispatches at zero skip without clearing light");
        g_isSkipFrame = 5;
        check(effect.OnProcess() == 1 && effect.frames == 2 && effect.m_stateCnt == 2
            && actor.m_BodyLight == 0 && actor.m_BeZero == 0, "skip count five still dispatches only one frame");
        g_isSkipFrame = 1;
    }
    {
        CRagEffect effect;
        effect.m_master = &actor; effect.m_isWorldEffect = true;
        check(effect.OnProcess() == 1 && effect.worldFrames == 1 && effect.frames == 0,
            "map effects delegate to their existing scheduler");
    }
    {
        CRagEffect effect;
        effect.m_master = &actor;
        CEffectPrim prim;
        prim.m_master = &effect; prim.m_duration = 1;
        check(prim.OnProcess() == 1 && prim.m_stateCnt == 1, "primitive includes final duration frame");
        check(prim.OnProcess() == 0 && prim.m_stateCnt == 2, "primitive cannot extend duration for alpha/bands");
        prim.m_stateCnt = 0; prim.m_duration = 10; prim.updates = 0; g_isSkipFrame = 0;
        check(prim.OnProcess() == 1 && prim.updates == 0, "primitive skip gate pauses updates");
        prim.m_type = PP_2DTEXTURE; prim.m_gi[0].life = 99;
        check(prim.OnProcess() == 1 && prim.updates == 1, "life-99 texture bypasses primitive gate");
        prim.m_type = PP_3DCROSSTEXTURE;
        check(prim.OnProcess() == 1 && prim.updates == 2, "reference primitive 0x10 bypasses gate with life 99");
        prim.m_type = Other;
        check(prim.OnProcess() == 1 && prim.updates == 2, "other life-99 primitive remains gated");
        prim.SendMsg(nullptr, 24, 0, 0, 0);
        check(prim.OnProcess() == 0 && prim.updates == 2, "detached primitive terminates before updating");
        g_isSkipFrame = 1;
    }
    {
        CRagEffect* effect = new CRagEffect;
        effect->m_master = &actor; effect->spawn = true; effect->m_duration = 3;
        actor.m_effectList.push_back(effect);
        g_world.m_gameObjectList.push_back(effect);
        const int effectBefore = effectDeaths, primBefore = primitiveDeaths;
        g_world.UpdateGameObjects();
        check(g_world.m_gameObjectList.size() == 2 && effect->m_primList.size() == 1,
            "spawn registers world ownership and parent attachment");
        CEffectPrim* prim = effect->m_primList.front();
        check(prim->updates == 1, "world processes newly spawned primitive exactly once");
        g_world.UpdateGameObjects();
        check(prim->updates == 2, "parent does not also tick world-owned primitive");
        g_world.UpdateGameObjects();
        check(g_world.m_gameObjectList.empty() && actor.m_effectList.empty()
            && effectDeaths == effectBefore + 1 && primitiveDeaths == primBefore + 1,
            "parent expiry detaches primitives and world deletes each once");
    }
    {
        CRagEffect* effect = new CRagEffect;
        effect->m_master = &actor;
        CEffectPrim* prim = effect->LaunchEffectPrim(Other, {});
        g_world.m_gameObjectList.remove(prim);
        delete prim;
        check(effect->m_primList.empty(), "primitive destruction unregisters parent attachment");
        delete effect;
    }
    {
        CRagEffect effect;
        effect.m_master = &actor;
        CEffectPrim* prim = effect.LaunchEffectPrim(Other, {});
        const int before = primitiveDeaths;
        effect.SendMsg(nullptr, 25, 1, 0, 0);
        check(effect.m_primList.empty() && prim->m_master == nullptr && primitiveDeaths == before,
            "clear message detaches primitive without deleting world ownership");
        g_world.UpdateGameObjects();
        check(g_world.m_gameObjectList.empty() && primitiveDeaths == before + 1, "world reclaims detached primitive");
    }
    {
        CRagEffect first, second;
        first.m_master = &actor; second.m_master = &actor;
        first.m_type = second.m_type = 0;
        actor.m_effectList = { &first, &second };
        actor.m_beginSpellEffect = &first;
        actor.RemoveEffectById(0);
        check(actor.m_effectList.size() == 1 && first.m_master == nullptr && second.m_master == &actor
            && actor.m_beginSpellEffect == &first, "remove id zero detaches first match only, preserving message-109 pointer semantics");
        actor.DetachEffects();
        check(actor.m_effectList.empty() && second.m_master == nullptr, "actor exit detaches remaining effects");
    }
    {
        CRagEffect effect;
        vector3d delta{ 1, 2, 3 }, delta2{ 4, 5, 6 };
        effect.SendMsg(nullptr, 14, reinterpret_cast<msgparam_t>(&delta), reinterpret_cast<msgparam_t>(&delta2), 0);
        check(effect.m_deltaPos.x == 1 && effect.m_deltaPos2.z == 6, "message 14 copies both reference vectors");
    }
    {
        const int launches = actor.launches;
        actor.SendMsg(nullptr, 85, 16, 12, 0);
        check(actor.launches == launches + 2 && actor.m_effectList.size() == 1
            && g_world.m_gameObjectList.size() == 2 && actor.m_beginSpellEffect->m_duration == 12,
            "first positive begin message launches/removes/relaunches in reference order");
        CRagEffect* old = actor.m_beginSpellEffect;
        actor.SendMsg(nullptr, 85, 16, 0, 0);
        check(actor.launches == launches + 3 && old->m_master == nullptr
            && actor.m_beginSpellEffect != old && actor.m_beginSpellEffect->m_duration == 0,
            "repeat begin message relaunches instead of refreshing; zero duration is preserved");
        actor.SendMsg(nullptr, 86, 23, 9, 0);
        CRagEffect* magic = actor.m_magicTargetEffect;
        const int afterMagic = actor.launches;
        actor.SendMsg(nullptr, 86, 24, 0, 0);
        check(actor.launches == afterMagic && actor.m_magicTargetEffect == magic && magic->m_duration == 0,
            "target message reuses existing magic target and sets zero duration");
        int type = 23;
        actor.m_effectList.push_front(nullptr);
        actor.SendMsg(nullptr, 120, reinterpret_cast<msgparam_t>(&type), 0, 0);
        check(type == -1 && actor.m_effectList.front() != nullptr, "effect query prunes nulls and reports an attached id");
        type = 999;
        actor.SendMsg(nullptr, 120, reinterpret_cast<msgparam_t>(&type), 0, 0);
        check(type == 999, "effect query leaves unknown id unchanged");
        actor.SendMsg(nullptr, 84, 0, 0, 0);
        check(actor.m_beginSpellEffect == nullptr && actor.m_effectList.size() == 1, "begin cancellation detaches its listed effect");
        actor.SendMsg(nullptr, 87, 0, 0, 0);
        check(actor.m_magicTargetEffect == nullptr && actor.m_effectList.empty(), "target cancellation detaches its listed effect");
        g_world.UpdateGameObjects();
        check(g_world.m_gameObjectList.empty(), "world reclaims cancelled and superseded cast effects");
    }
    {
        CRagEffect effect;
        effect.m_master = &actor;
        effect.m_grimToothOrigin = { 10, 5, 20 };
        effect.m_deltaPos = { 10, 99, 10 };
        const int before = rngDraws;
        effect.SpawnGrimTooth();
        auto* prim = effect.m_primList.front();
        check(waves == 1 && waveY == 0 && rngDraws == before + 5, "Grim Tooth retains startup wave and both discarded random draws");
        check(effect.m_flag == 0 && std::abs(prim->m_deltaPos2.z + 2) < 0.0001f
            && prim->m_deltaPos2.y == 20 && std::abs(effect.m_grimToothRemaining.z + 4) < 0.0001f,
            "Grim Tooth subtracts two-unit step before spawn and again after spawn without target-height interpolation");
        check(prim->m_duration == 40 && prim->m_spawnCount == 6 && prim->m_fadeOutCnt == 30
            && prim->m_latitude == 75 && std::strcmp(g_texMgr.lastPath, "effect\\stone.bmp") == 0,
            "Grim Tooth uses reference lifetime, count, orientation and single texture name");
        effect.m_stateCnt = 1;
        effect.SpawnGrimTooth();
        check(effect.m_primList.size() == 1 && rngDraws == before + 5 && waves == 1,
            "Grim Tooth advances between three-frame emissions without extra random or audio calls");
        effect.m_stateCnt = 3;
        effect.m_deltaPos = effect.m_grimToothOrigin;
        effect.SpawnGrimTooth();
        check(effect.m_flag == 1 && effect.m_primList.size() == 2,
            "Grim Tooth distance gate sets flag and still emits on that frame");
        effect.m_stateCnt = 6;
        effect.SpawnGrimTooth();
        check(effect.m_primList.size() == 2, "Grim Tooth flag prevents later emission");
    }
    g_world.UpdateGameObjects();
    {
        CRagEffect effect;
        effect.m_master = &actor;
        effect.m_effectPos = { 7, 8, 9 };
        effect.m_count = 17;
        const struct { int option; vector3d expected; } controls[] = {
            {130, {200,200,0}}, {250, {200,200,0}}, {128, {250,150,150}},
            {169, {180,180,250}}, {243, {250,250,250}},
        };
        for (const auto& control : controls) {
            effect.SendMsg(nullptr, 44, 65535, control.option, 77);
            const auto& pos = effect.EffectPosition();
            check(pos.x == control.expected.x && pos.y == control.expected.y && pos.z == control.expected.z
                && effect.m_count == 17, "message 44 positive sentinel stores exact control triple without changing count");
        }
        check(effect.m_duration == 77, "message 44 option 243 preserves third parameter as duration");
        effect.SendMsg(nullptr, 44, 65534, 128, 0);
        check(effect.EffectPosition().x == -1 && effect.EffectPosition().z == -1, "message 44 negative sentinel 128");
        effect.SendMsg(nullptr, 44, 65534, 130, 0);
        check(effect.EffectPosition().x == -2 && effect.EffectPosition().y == -2, "message 44 negative sentinel 130");
        effect.SendMsg(nullptr, 44, 65534, 999, 0);
        effect.SendMsg(nullptr, 44, 65535, 999, 0);
        check(effect.EffectPosition().x == -2 && effect.m_count == 17 && effect.m_duration == 77,
            "unrecognized sentinel options leave control values and duration unchanged");
        effect.SendMsg(nullptr, 44, 51, 0, 0);
        check(effect.m_count == 50, "ordinary message 44 caps only above 50");
        effect.SendMsg(nullptr, 44, -3, 0, 0);
        check(effect.m_count == -3, "ordinary message 44 preserves negative count");
        CEffectPrim prim;
        prim.m_deltaPos = { 1, 2, 3 };
        prim.Init(&effect, Other, { 4, 5, 6 });
        check(prim.m_deltaPos.x == 5 && prim.m_deltaPos.y == 7 && prim.m_deltaPos.z == 9
            && prim.m_pos.x == -2 && prim.m_orgPos.z == -2, "primitive Init adds delta and copies persistent parent position");
        g_isSkipFrame = 0;
        const int copiedTypes[] = {0x17,0x1C,0x1D,0x1E,0x20,0x23,0x3C,0x44,0x63,0x68,0x69,0x6A,0x6C,0x6D,0x6F};
        for (int type : copiedTypes) {
            prim.m_pos = {};
            prim.m_type = static_cast<EFFECTPRIMID>(type);
            check(prim.OnProcess() == 1 && prim.m_pos.x == -2 && prim.updates == 0,
                "reference follow-position primitive copies before zero-skip gate");
        }
        prim.m_type = Other;
        prim.m_pos = { 11, 12, 13 };
        check(prim.OnProcess() == 1 && prim.m_pos.x == 11, "other primitive retains own position when gated");
        actor.m_pos = { 91, 92, 93 };
        effect.OnProcess();
        check(effect.EffectPosition().x == -2, "actor position cache does not overwrite persistent effect control data");
        g_isSkipFrame = 1;
    }
    {
        CRagEffect effect;
        auto clipStorage = std::make_unique<KANICLIP>();
        KANICLIP& clip = *clipStorage;
        effect.m_master = &actor;
        effect.m_aniClips = &clip;
        effect.m_cEndLayer = 1;
        effect.m_isLayerDrawn[0] = 1;
        check(!effect.ProcessEZ2STR() && effect.m_isLayerDrawn[0] == 1,
            "missing STR resource terminates without clearing final frame metadata");
        effect.m_ezEffectRes = &clip;
        clip.cLayer = 2;
        KAC_KEYFRAME keys[2];
        keys[0].iFrame = 0; keys[0].XformData.x = 17;
        keys[1].iFrame = 2; keys[1].XformData.x = 23;
        clip.aLayer[1].cAniKey = 2; clip.aLayer[1].aAniKey = keys;
        effect.m_actXformData[1].x = 99;
        effect.m_aiCurAniKey[3] = 8;
        effect.InitEZ2STRFrame();
        check(effect.m_cEndLayer == 1 && effect.m_stateCnt == 0 && effect.m_iCurLayer == 0
            && effect.m_aiCurAniKey[1] == 0 && effect.m_aiCurAniKey[3] == 8
            && effect.m_actXformData[1].x == 99, "STR restart resets active cursors and counters while preserving accumulated transforms");
        check(effect.ProcessEZ2STR() && effect.m_actXformData[1].x == 17 && effect.m_iCurLayer == 0,
            "STR initial absolute key applies before any layer ends");
        effect.m_stateCnt = 2;
        check(effect.ProcessEZ2STR() && effect.m_iCurLayer == 1 && effect.m_aiCurAniKey[1] == -1
            && effect.m_actXformData[1].x == 23 && effect.m_isLayerDrawn[1] == 1,
            "STR terminal key still produces its final frame and marks layer complete");
        effect.m_stateCnt = 3;
        check(!effect.ProcessEZ2STR() && effect.m_isLayerDrawn[1] == 1 && effect.m_actXformData[1].x == 23,
            "STR ends on following call without mutating final transforms or draw metadata");
        clip.aLayer[0].cAniKey = 2;
        effect.InitEZ2STRFrame();
        check(effect.m_cEndLayer == 2, "STR layer zero contributes when it has keys");
    }
    {
        CRagEffect effect;
        effect.m_master = &actor;
        effect.m_type = 23;
        effect.m_strFrameActive = false;
        check(effect.OnProcess() == 0 && effect.m_stateCnt == 0,
            "checked STR result terminates before outer frame increment");
        const int ignored[] = {130,136,244,250,390,391,392,406,407,440};
        for (int id : ignored) {
            effect.m_type = id; effect.m_stateCnt = 0;
            check(effect.OnProcess() == 1 && effect.m_stateCnt == 1,
                "reference ignored STR result still permits outer duration processing");
        }
        auto clipStorage = std::make_unique<KANICLIP>();
        KANICLIP& clip = *clipStorage;
        clip.cLayer = 2;
        clip.aLayer[1].cAniKey = 2;
        effect.m_aniClips = &clip;
        effect.m_type = 145; effect.m_stateCnt = 40; effect.m_iCurLayer = 1;
        check(effect.OnProcess() == 1 && effect.m_stateCnt == 1 && effect.m_iCurLayer == 0,
            "effect 145 restarts completed STR before the outer increment");
    }
    {
        CRagEffect effect;
        effect.m_master = &actor;
        const int before = primitiveDeaths;
        CEffectPrim* rejected = effect.LaunchEffectPrim(Other, {});
        effect.DiscardPrimitive(rejected);
        CEffectPrim* replacement = effect.LaunchEffectPrim(Other, {});
        check(primitiveDeaths == before && rejected->m_master == nullptr
            && effect.m_primList.size() == 1 && effect.m_primList.front() == replacement
            && g_world.m_gameObjectList.size() == 2,
            "failed resource setup detaches registered primitive without deleting world-owned memory");
        g_world.UpdateGameObjects();
        check(primitiveDeaths == before + 1 && g_world.m_gameObjectList.size() == 1
            && effect.m_primList.front() == replacement && replacement->updates == 1,
            "world reclaims rejected primitive exactly once and processes replacement");
        effect.ClearPrims();
        g_world.UpdateGameObjects();
        check(primitiveDeaths == before + 2 && g_world.m_gameObjectList.empty(),
            "replacement remains under normal world ownership");
    }
    {
        CRagEffect effect;
        effect.m_master = &actor;
        actor.m_pos = { 10, 20, 30 };
        effect.m_deltaPos = { 40, 50, 60 };
        auto clipStorage = std::make_unique<KANICLIP>();
        KANICLIP& clip = *clipStorage;
        clip.cLayer = 2;
        KAC_KEYFRAME keys[2];
        keys[1].iFrame = 2;
        clip.aLayer[1].cAniKey = 2; clip.aLayer[1].aAniKey = keys;
        effect.m_aniClips = &clip; effect.m_ezEffectRes = &clip;
        effect.InitEZ2STRFrame();
        check(effect.ProcessTargetPositionStr() && actor.m_pos.x == 10 && actor.m_pos.y == 20
            && effect.m_deltaPos2.z == 30 && effect.m_strAnchorPosition.x == 40
            && effect.m_strAnchorPosition.y == 50 && effect.m_strAnchorPosition.z == 60,
            "case 185 captures target anchor and restores actor after an active STR frame");
        effect.m_stateCnt = 2;
        effect.ProcessTargetPositionStr();
        actor.m_pos = { 11, 21, 31 };
        effect.m_stateCnt = 3;
        check(!effect.ProcessTargetPositionStr() && actor.m_pos.x == 11 && actor.m_pos.z == 31
            && effect.m_deltaPos2.y == 21 && effect.m_strAnchorPosition.x == 40,
            "case 185 restores current actor position on STR completion without losing last projected anchor");
        effect.m_ezEffectRes = nullptr;
        check(!effect.ProcessTargetPositionStr() && actor.m_pos.x == 11 && actor.m_pos.y == 21,
            "case 185 restores actor when resource is missing");
    }
    {
        CRagEffect effect;
        effect.m_master = &actor;
        effect.m_duration = 700;
        const int beforeWaves = waves, beforeRng = rngDraws;
        effect.ApplyEntry2();
        CEffectPrim* prim = effect.m_primList.front();
        check(static_cast<int>(prim->m_type) == 0x3C && prim->m_renderFlag == 5
            && prim->m_duration == 700 && prim->m_size == 4
            && std::strcmp(g_texMgr.lastPath, "effect\\ring_blue.tga") == 0,
            "Entry2 launches reference heal primitive with single texture and source duration");
        check(waves == beforeWaves + 1 && waveY == -150 && waveName == "effect\\EF_Portal.wav"
            && rngDraws == beforeRng + 4, "Entry2 preserves vertical sound argument and one random angle per band");
        const float heights[] = {30,30,4,4};
        const float distances[] = {3.7f,3.4000001f,3.5999999f,3.7f};
        const short rises[] = {90,90,10,5};
        for (int band = 0; band < 4; ++band) {
            const EffectGi& gi = prim->m_gi[band];
            check(gi.life == 1 && gi.process == 0 && gi.fullDisplayAngle == 360
                && gi.maxHeight == heights[band] && gi.distance == distances[band]
                && gi.riseAngle == rises[band] && gi.alphaT == 1400 && gi.alphaB == 0
                && std::all_of(gi.height.begin(),gi.height.end(),[](float h){return h==0;})
                && std::all_of(gi.flag.begin(),gi.flag.end(),[](u8 f){return f==2;}),
                "Entry2 band carries exact height, distance, rise and all 21 height/flag samples");
        }
        effect.m_stateCnt = 1;
        actor.m_pos = { 71,72,73 };
        effect.ApplyEntry2();
        check(effect.EffectPosition().x == 71 && effect.m_primList.size() == 1
            && waves == beforeWaves + 1 && rngDraws == beforeRng + 4,
            "Entry2 follows actor after launch without new primitive, RNG or sound");
        effect.ClearPrims();
        g_world.UpdateGameObjects();
    }
    {
        CRagEffect effect;
        effect.m_master = &actor;
        CEffectPrim prim;
        prim.m_master = &effect;
        prim.m_duration = 5;
        check(prim.OnProcess() == 1 && prim.observedFrame == 0 && prim.m_stateCnt == 1,
            "primitive handler observes frame zero before shared outer increment");
        prim.active = false;
        check(prim.OnProcess() == 0 && prim.observedFrame == 1 && prim.m_stateCnt == 1,
            "primitive handler failure terminates without advancing its counter");
        prim.active = true; prim.resetFrame = 3;
        check(prim.OnProcess() == 1 && prim.m_stateCnt == 4,
            "outer primitive increment respects explicit handler counter replacement");
        prim.resetFrame = -1; prim.m_duration = 0; prim.m_stateCnt = 0;
        check(prim.OnProcess() == 0 && prim.observedFrame == 0 && prim.m_stateCnt == 1,
            "zero-duration primitive dispatches at frame zero then expires");
    }
    {
        KAC_XFORMDATA target, delta;
        target.x = 1; delta.x = 2;
        target.us2 = 3; delta.us2 = 4;
        target.ax[3] = 5; delta.ax[3] = 6;
        target.ay[0] = 7; delta.ay[0] = 8;
        target.rz = 9; delta.rz = 10;
        target.crR = 11; delta.crR = 12;
        target.crA = 13; delta.crA = 14;
        target.srcalpha = 5; target.destalpha = 2; target.mtpreset = 9;
        delta.srcalpha = 1; delta.destalpha = 7; delta.mtpreset = 3;
        AddXformDelta(&target,delta);
        check(target.x == 3 && target.us2 == 7 && target.ax[3] == 11 && target.ay[0] == 15
            && target.rz == 19 && target.crR == 23 && target.crA == 27,
            "STR delta keys accumulate positional, UV, corner, rotation and color fields");
        check(target.srcalpha == 5 && target.destalpha == 2 && target.mtpreset == 9,
            "STR delta keys retain existing blend modes and material preset");
    }
    {
        auto runMode = [&](u32 mode, float start, float delta, int age, int textures) {
            CRagEffect effect;
            effect.m_master = &actor;
            auto clipStorage = std::make_unique<KANICLIP>();
        KANICLIP& clip = *clipStorage; clip.cLayer = 2;
            KAC_KEYFRAME keys[2]; keys[1].iFrame = 1000;
            keys[0].dwType = 1;
            keys[0].XformData.anitype = mode;
            keys[0].XformData.aniframe = delta;
            keys[0].XformData.anidelta = delta;
            clip.aLayer[1].cAniKey = 2; clip.aLayer[1].aAniKey = keys; clip.aLayer[1].cTex = textures;
            effect.m_aniClips = &clip; effect.m_ezEffectRes = &clip;
            effect.InitEZ2STRFrame(); effect.m_stateCnt = age;
            effect.m_actXformData[1].aniframe = start;
            effect.ProcessEZ2STR();
            return effect.m_actXformData[1].aniframe;
        };
        check(runMode(0,3,20,7,5) == 3, "STR stop mode retains frame");
        check(runMode(1,2,1.5f,7,5) == 3.5f, "STR interpolation mode adds key frame delta");
        check(runMode(2,4.5f,2,7,5) == 4, "STR once mode clamps at last texture");
        check(runMode(2,-3,1,7,5) == -2, "STR once mode preserves negative frame without an invented lower clamp");
        check(runMode(3,3,13,7,5) == 1, "STR forward loop truncates quotient across multiple cycles");
        check(runMode(3,1,-9,7,5) == -8, "STR forward loop leaves negative values untouched");
        check(runMode(4,1,2,7,5) == 4, "STR reverse loop applies its single texture-count addition");
        check(runMode(4,1,13,7,5) == -7, "STR reverse loop preserves original out-of-range result for a large backward step");
        check(runMode(5,0,1,7,5) == 1, "STR bidirectional mode uses integer quotient parity");
        check(runMode(5,0,-1,1,5) == -1, "STR bidirectional mode preserves signed remainder");
        check(runMode(99,3,20,7,5) == 3, "STR unknown animation mode leaves frame unchanged");
    }
    {
        CRagEffect effect;
        effect.m_master = &actor;
        auto clipStorage = std::make_unique<KANICLIP>();
        KANICLIP& clip = *clipStorage; clip.cLayer = 2;
        KAC_KEYFRAME keys[2]; keys[0].XformData.aniframe = 1;
        keys[1].iFrame = 10; keys[1].dwType = 1;
        keys[1].XformData.anitype = 5; keys[1].XformData.anidelta = 2;
        clip.aLayer[1].cAniKey = 2; clip.aLayer[1].aAniKey = keys; clip.aLayer[1].cTex = 5;
        effect.m_aniClips = &clip; effect.m_ezEffectRes = &clip;
        effect.InitEZ2STRFrame(); effect.ProcessEZ2STR(); effect.m_stateCnt = 10;
        check(effect.ProcessEZ2STR() && effect.m_actXformData[1].aniframe == 3
            && effect.m_aiCurAniKey[1] == -1 && effect.m_iCurLayer == 1,
            "STR key transition uses the previous key age for bidirectional terminal delta");
    }
    {
        CRagEffect effect;
        effect.m_master = &actor;
        auto clipStorage = std::make_unique<KANICLIP>();
        KANICLIP& clip = *clipStorage; clip.cLayer = 4;
        KAC_KEYFRAME keys[3][2];
        for (int i = 0; i < 3; ++i) {
            keys[i][0].dwType = 1; keys[i][0].XformData.anitype = i == 0 ? 3 : 4;
            keys[i][0].XformData.anidelta = i == 0 ? 11.0f : 2.0f;
            keys[i][1].iFrame = 1000;
            clip.aLayer[i+1].cAniKey = 2; clip.aLayer[i+1].aAniKey = keys[i]; clip.aLayer[i+1].cTex = 5;
            effect.m_actXformData[i+1].aniframe = 1;
        }
        effect.m_aniClips = &clip; effect.m_ezEffectRes = &clip;
        effect.InitEZ2STRFrame();
        check(effect.ProcessEZ2STR() && effect.m_actXformData[1].aniframe == 2
            && effect.m_actXformData[2].aniframe == -6 && effect.m_actXformData[3].aniframe == 9,
            "STR loop quotient is shared across layers in binary order, including its reverse-loop quirk");
        clip.cLayer = 2;
        keys[0][0].XformData.anitype = 4; keys[0][0].XformData.anidelta = 2;
        effect.InitEZ2STRFrame(); effect.m_actXformData[1].aniframe = 1;
        check(effect.ProcessEZ2STR(7,-9,3) && effect.m_actXformData[1].aniframe == -11
            && effect.m_strScreenOffset.x == 7 && effect.m_strScreenOffset.y == -9 && effect.m_strScreenOffset.z == 3,
            "STR reverse-loop scratch starts at passed z while screen offsets preserve all original arguments");
    }
    {
        auto close = [](float a,float b) { return std::abs(a-b) < 0.0001f; };
        CRagEffect effect;
        effect.m_effectPos = {0,0,1};
        effect.m_tlvertX = 100; effect.m_tlvertY = 200;
        matrix view; MatrixIdentity(view);
        KAC_LAYER layer; layer.cTex = 1;
        KAC_XFORMDATA xf;
        xf.x = 320; xf.y = 240; xf.u = 0.5f; xf.us = 0.25f; xf.v = 0.2f; xf.vs = 0.4f;
        xf.crA = 257.9f; xf.crR = -1.9f; xf.crG = 258.9f; xf.crB = 511.1f;
        xf.srcalpha = 7; xf.destalpha = 8; xf.mtpreset = 9;
        const int xs[2][4] = {{40,10,30,20},{20,30,10,40}};
        const int ys[2][4] = {{400,100,300,200},{400,100,300,200}};
        for (int flipX = 0; flipX < 2; ++flipX) {
            for (int flipY = 0; flipY < 2; ++flipY) {
                for (int k=0;k<4;++k) {
                    xf.ax[k] = static_cast<float>(flipX ? 40-10*k : 10+10*k);
                    xf.ay[k] = static_cast<float>(flipY ? 400-100*k : 100+100*k);
                }
                effect.RenderAniClip(1,layer,xf,&view);
                bool vertices = true;
                for (int k=0;k<4;++k) {
                    const auto& v = g_renderer.face.m_verts[k];
                    const bool right = flipX ? k < 2 : k >= 2;
                    const bool lower = flipY ? (k==1 || k==3) : (k==0 || k==2);
                    vertices = vertices && close(v.x,100.0f+xs[flipX][k]) && close(v.y,200.0f+ys[flipY][k])
                        && close(v.tu,right ? 0.375f : 0.25f) && close(v.tv,lower ? 0.15f : 0.05f)
                        && v.z == 0.00003102122f && v.oow == 1 && v.specular == 0xFFFFFFFFu && v.color == 0x01FF02FFu;
                }
                check(vertices, "STR mirrored/unmirrored quad preserves corner order, texture cropping, wrapped colors and depth");
            }
        }
        check(g_renderer.face.srcAlphaMode == 7 && g_renderer.face.destAlphaMode == 8
            && g_renderer.face.mtPreset == 9 && g_renderer.lastFlags == 5 && g_renderer.face.numVerts == 4,
            "STR submits original blend values and preset without destination-alpha rewriting");
        xf.aniframe = 99;
        effect.RenderAniClip(1,layer,xf,&view);
        check(lastTextureFrame == 99, "STR texture lookup retains upper frame index instead of clamping to layer texture count");
        xf.aniframe = -2;
        effect.RenderAniClip(1,layer,xf,&view);
        check(lastTextureFrame == 0, "STR texture lookup clamps only nonpositive frame to zero");
        const int before = g_renderer.submissions;
        xf.srcalpha = xf.destalpha = 0;
        effect.RenderAniClip(1,layer,xf,&view);
        check(g_renderer.submissions == before+1 && g_renderer.face.srcAlphaMode == 0 && g_renderer.face.destAlphaMode == 0,
            "STR zero-blend key is submitted immediately without invented key-age suppression");
        for (int k=0;k<4;++k) { xf.ax[k] = static_cast<float>(10+10*k); xf.ay[k] = static_cast<float>(100+100*k); }
        xf.rz = 256;
        effect.RenderAniClip(1,layer,xf,&view);
        check(std::abs(g_renderer.face.m_verts[1].x) < 0.001f && std::abs(g_renderer.face.m_verts[1].y-210) < 0.001f,
            "STR quad rotation uses reference 1024-unit angular conversion");
    }
    {
        CRagEffect effect;
        effect.m_master = &actor;
        actor.m_pos = {2,4,2}; effect.m_effectPos = {0,0,1};
        g_renderer.m_halfWidth = 500; g_renderer.m_halfHeight = 400;
        g_renderer.m_xoffset = 11; g_renderer.m_yoffset = 22;
        MatrixIdentity(testView.view);
        auto clipStorage = std::make_unique<KANICLIP>();
        KANICLIP& clip = *clipStorage; clip.cLayer = 2;
        KAC_KEYFRAME keys[2]; keys[1].iFrame = 1000;
        keys[0].XformData.x = 320; keys[0].XformData.y = 240;
        for(int k=0;k<4;++k) {keys[0].XformData.ax[k]=static_cast<float>(10+10*k);keys[0].XformData.ay[k]=static_cast<float>(100+100*k);}
        clip.aLayer[1].cAniKey = 2; clip.aLayer[1].aAniKey = keys; clip.aLayer[1].cTex = 1;
        effect.m_aniClips = &clip; effect.m_ezEffectRes = &clip;
        effect.InitEZ2STRFrame();
        check(effect.ProcessEZ2STR() && effect.m_tlvertX == 507 && effect.m_tlvertY == 334 && effect.m_tlvertZ < 0,
            "STR processing projection uses half-screen offsets and unclamped device depth");
        matrix renderView; MatrixIdentity(renderView); renderView.m[3][0] = 100; renderView.m[3][2] = 1;
        effect.RenderAniClip(1,clip.aLayer[1],effect.m_actXformData[1],&renderView);
        check(g_renderer.face.m_verts[0].x == 527 && g_renderer.face.m_verts[0].y == 534
            && g_renderer.face.m_verts[0].oow == 0.5f && effect.m_tlvertX == 507,
            "STR render uses saved screen anchor while pixel ratio and reciprocal depth use persistent effect position and render view");
        const auto negative = ProjectStrPoint({2,3,-1},testView.view,3,4);
        check(negative.x == -11 && negative.y == -17 && negative.z == -1,
            "STR projection preserves negative reciprocal depth without billboard clipping");
        g_renderer.m_halfWidth = g_renderer.m_halfHeight = g_renderer.m_xoffset = g_renderer.m_yoffset = 0;
    }
    {
        CRagEffect effect;
        effect.m_effectPos = {0,0,1};
        auto clipStorage = std::make_unique<KANICLIP>();
        KANICLIP& clip = *clipStorage; clip.cLayer = 3;
        effect.m_aniClips = &clip;
        effect.m_isLayerDrawn[0] = effect.m_isLayerDrawn[1] = 1;
        matrix view; MatrixIdentity(view);
        int before = g_renderer.submissions;
        effect.Render(&view);
        check(g_renderer.submissions == before+1,
            "effect Render draws marked STR layers independently of handler tag and excludes layer zero");
        effect.m_isLayerDrawn[2] = 1;
        before = g_renderer.submissions;
        effect.Render(&view);
        check(g_renderer.submissions == before+2, "effect Render submits every marked nonzero STR layer");
    }
    {
        CRagEffect effect;
        effect.m_master = &actor; effect.m_duration = 700;
        const int randomBefore = rngDraws;
        check(!effect.ApplyJobLvUp50(0) && effect.ApplyJobLvUp50(45) && effect.m_primList.empty(),
            "JobLvUp50 only time zero processes STR and missing STR does not make time 45 fail");
        effect.m_stateCnt = 5;
        effect.ApplyJobLvUp50(0); effect.ApplyJobLvUp50(45);
        check(effect.m_primList.size() == 2 && std::strcmp(g_texMgr.lastPath,"effect\\magic_green.tga") == 0
            && rngDraws == randomBefore, "JobLvUp50 emits two exact green slash primitives without random draws");
        int time = 0;
        for (auto* prim : effect.m_primList) {
            bool bands = prim->m_type == PP_SLASH1 && prim->m_duration == 700 && prim->m_size == 9 && prim->m_renderFlag == 5;
            for(int k=0;k<4;++k) {
                const auto& gi=prim->m_gi[k];
                bands = bands && gi.life==1 && gi.process==0 && gi.maxHeight==2 && gi.distance==2
                    && gi.height[0]==1 && gi.rotStart==time+90*k && gi.alphaB==0 && gi.flag[0]==0;
            }
            check(bands,"JobLvUp50 carries exact four-band profiles and source orientation");
            time = 45;
        }
        g_world.UpdateGameObjects();
        auto* first = effect.m_primList.front();
        check(first->m_stateCnt == 1 && first->m_gi[0].process == 1 && first->m_gi[0].height[0] == 2
            && first->m_gi[0].alphaB == 5 && first->m_gi[0].distance == 4,
            "world dispatches slash update before the shared primitive frame increment");
        effect.m_stateCnt = 6; actor.m_pos={12,13,14}; effect.ApplyJobLvUp50(45);
        check(effect.m_primList.size()==2 && effect.EffectPosition().x==12,
            "JobLvUp50 follows actor after its emission frame without another primitive");
        effect.ApplyJobLvUp50_2(0); effect.ApplyJobLvUp50_2(45);
        auto* blue = effect.m_primList.back();
        check(effect.m_primList.size()==4 && blue->m_size==4 && blue->m_gi[0].maxHeight==1.5f
            && blue->m_gi[3].rotStart==315 && std::strcmp(g_texMgr.lastPath,"effect\\magic_blue.tga")==0,
            "JobLvUp50_2 emits blue size-four slashes on every call with no STR processing");
        effect.ClearPrims(); g_world.UpdateGameObjects();
    }
    {
        CRagEffect effect; effect.m_master=&actor; effect.m_effectPos={10,20,30};
        CEffectPrim prim; prim.m_master=&effect;
        auto& normal=prim.m_gi[0]; normal.life=1;normal.process=6;normal.height[0]=9;normal.alphaB=10;normal.maxHeight=3.9f;normal.distance=2;
        auto& run=prim.m_gi[1]; run=normal;run.process=4;run.maxHeight=1.9f;run.flag[0]=1;
        auto& dead=prim.m_gi[2];dead=normal;dead.process=20;dead.height[0]=10;dead.alphaB=1;dead.maxHeight=5;
        prim.m_gi[3].process=17;
        UpdateSlash1Primitive(&prim);
        check(prim.m_pos.x==10 && normal.process==7 && normal.height[0]==10 && normal.alphaB==15
            && normal.distance==4 && normal.maxHeight>4, "slash normal branch grows and permits reference height overshoot");
        check(run.process==5 && run.alphaB==15 && run.distance==3 && run.maxHeight>2,
            "slash run-stop variant uses shorter alpha rise and smaller radial/height steps");
        check(dead.life==0 && dead.alphaB==0 && dead.distance==4 && prim.m_gi[3].process==17,
            "slash expiry still finishes its current update and skips preexisting inactive bands");
        UpdateSlash1Primitive(&prim);
        check(normal.alphaB==13 && run.alphaB==13 && dead.process==21,
            "slash starts fading only after inner radius ten and leaves dead bands untouched");
        prim.m_texture.push_back(&g_texMgr.texture); prim.m_renderFlag=5;prim.m_size=9;
        for(auto& gi:prim.m_gi)gi.life=0;
        normal.life=1;normal.alphaB=30;normal.height[0]=3;normal.distance=2;normal.maxHeight=4;normal.rotStart=0;
        matrix view;MatrixIdentity(view);slashQuads.clear();
        RenderSlash1Primitive(prim,view);
        check(slashQuads.size()==3 && slashQuads[0].color==0x0AFFFFFFu && slashQuads[1].color==0x1EFFFFFFu
            && slashQuads[2].points[0].z>30 && slashQuads[0].points[0].z<30,
            "slash geometry emits three adjacent rays with dimmer side ribbons and wrapped angles");
        const auto& center=slashQuads[1];
        check(center.points[0].x==13 && center.points[0].y==20 && center.points[1].x==15
            && center.points[2].y==16 && std::abs(center.points[3].y-19.2f)<0.0001f
            && center.u0==0 && center.u1==1 && center.v0==1 && center.v1==0 && center.blend==2 && center.flags==5,
            "slash world geometry preserves inner/outer heights, UVs and additive submission without multiplying by stored size");
        normal.alphaB=0;slashQuads.clear();RenderSlash1Primitive(prim,view);
        check(slashQuads.empty(),"slash renderer omits zero-alpha bands");
    }
    {
        CRagEffect effect;effect.m_master=&actor;
        effect.m_stateCnt=14;effect.ProcessJobLvUp50_2();
        check(effect.m_primList.empty(),"effect 590 waits for frame 15 before calling its unconditional slash helpers");
        effect.m_stateCnt=15;effect.ProcessJobLvUp50_2();
        check(effect.m_primList.size()==2,"effect 590 calls its zero/45-degree helpers on frame 15");
        effect.m_stateCnt=16;effect.ProcessJobLvUp50_2();
        check(effect.m_primList.size()==2,"effect 590 does not repeat emission after frame 15");
        effect.ClearPrims();g_world.UpdateGameObjects();
    }
    {
        CRes resource;
        check(resource.Lock()==1 && resource.Lock()==2 && resource.m_timeStamp==0,
            "resource Lock increments reference count without changing timestamp");
        resourceTick=456;
        check(resource.Unlock()==1 && resource.m_timeStamp==456 && resource.Unlock()==0 && resource.Unlock()==-1,
            "resource Unlock records current tick and preserves signed reference count semantics");
    }
    {
        CRagEffect effect;effect.m_master=&actor;effect.m_duration=300;
        actor.m_pos={10,50,30};
        const int variants[]={0,1,2,3,4,10,11};
        const int randomBefore=rngDraws;
        g_resMgr.requests.clear();
        for (int variant:variants) effect.ApplySuperAngel(static_cast<signed char>(variant),0);
        check(effect.m_primList.size()==7 && rngDraws==randomBefore && g_resMgr.requests.size()==14
            && g_resMgr.spr.m_lockCnt==7 && g_resMgr.act.m_lockCnt==7,
            "SuperAngel emits seven exact variants and loads/locks one sprite/action pair each without RNG");
        std::set<std::string> sprites,actions;
        int index=0;
        for(auto* prim:effect.m_primList) {
            const int variant=variants[index++];
            sprites.insert(prim->m_spriteSprName);actions.insert(prim->m_spriteActName);
            check(prim->m_type==PP_SUPERANGEL && prim->m_duration==300 && prim->m_alpha==0 && prim->m_size==1
                && prim->m_renderFlag==(variant==10?0x201u:5u) && prim->m_gi[0].flag[0]==variant
                && prim->m_gi[0].sparkY==(variant>=10?20.0f:10.0f) && prim->m_numSegments==1
                && prim->m_spriteAction==0 && prim->m_animSpeed==16 && prim->m_curMotion==0 && prim->m_repeatAnim,
                "SuperAngel variant has reference type, position, action, alpha and render flags");
        }
        check(sprites.size()==7 && actions.size()==7 && sprites.begin()->find("fake_angel")==std::string::npos,
            "SuperAngel variants retain distinct recovered Korean sprite/action names");
        effect.ClearPrims();g_world.UpdateGameObjects();
        check(g_resMgr.spr.m_lockCnt==0 && g_resMgr.act.m_lockCnt==0 && g_resMgr.spr.m_timeStamp==resourceTick,
            "world reclamation releases every SuperAngel resource lock with current timestamp");
    }
    {
        CRagEffect effect;effect.m_master=&actor;effect.m_type=338;
        const int before=waves;
        for(int frame=0;frame<=18;++frame) {effect.m_stateCnt=frame;effect.ProcessSuperAngelEffect();}
        check(effect.m_primList.size()==10 && waves==before+1 && waveName=="levelup.wav" && waveY==0,
            "effect 338 launches four initial variants plus six wing trails without duplicate frame-zero wing, with one listener wave");
        int wings=0;for(auto* prim:effect.m_primList)if(prim->m_gi[0].flag[0]==1)++wings;
        check(wings==7,"effect 338 wing births occur at frames 0,3,6,9,12,15,18");
        effect.m_stateCnt=19;effect.ProcessSuperAngelEffect();
        check(effect.m_primList.size()==10,"SuperAngel stops trail emission after frame 18");
        effect.m_stateCnt=65;effect.ProcessSuperAngelEffect();auto* slash=effect.m_primList.back();
        check(effect.m_primList.size()==11 && slash->m_type==PP_SLASH1 && slash->m_size==4
            && slash->m_gi[3].rotStart==270 && slash->m_gi[0].maxHeight==2 && slash->m_gi[0].height[0]==1
            && std::strcmp(g_texMgr.lastPath,"effect\\ring_blue.tga")==0,
            "SuperAngel emits its original four-band blue slash at frame 65");
        effect.m_stateCnt=66;effect.ProcessSuperAngelEffect();
        check(effect.m_primList.size()==12 && effect.m_primList.back()->m_gi[3].rotStart==315,
            "SuperAngel second slash is rotated 45 degrees on frame 66");
        effect.ClearPrims();g_world.UpdateGameObjects();
    }
    {
        CRagEffect effect;effect.m_master=&actor;effect.m_type=582;
        const int before=waves;
        effect.ProcessSuperAngelEffect();
        check(effect.m_primList.size()==2 && effect.m_primList.front()->m_gi[0].flag[0]==11
            && effect.m_primList.back()->m_gi[0].flag[0]==10 && waves==before,
            "effect 582 preserves 11-before-10 order and does not invent a level-up wave");
        effect.m_stateCnt=3;effect.ProcessSuperAngelEffect();
        check(effect.m_primList.size()==3 && effect.m_primList.back()->m_gi[0].flag[1]==3,
            "effect 582 repeats only variant 11 and stores birth frame");
        effect.ClearPrims();g_world.UpdateGameObjects();
        g_resMgr.fail=true;effect.m_stateCnt=0;effect.ApplySuperAngel(0,0);
        check(effect.m_primList.size()==1 && !effect.m_primList.front()->m_superAngelSprRes
            && !effect.m_primList.front()->m_superAngelActRes,
            "missing SuperAngel assets leave one unrenderable original primitive without alternate names or replacement textures");
        matrix view;MatrixIdentity(view);const int submitted=angelSpriteSubmissions;
        RenderSuperAngelPrimitive(*effect.m_primList.front(),view);
        check(angelSpriteSubmissions==submitted,"SuperAngel renderer does not substitute missing resources");
        effect.ClearPrims();g_world.UpdateGameObjects();g_resMgr.fail=false;
    }
    {
        CRagEffect effect;effect.m_master=&actor;effect.m_effectPos={999,999,999};
        const int variants[]={0,1,2,3,4,10,11};
        const float initial[]={5,5,5,5,5,10,0};
        const int cutoffs[]={100,75,30,60,70,100,75};
        const float fades[]={4,6,3,3,3,6,6};
        for(int index=0;index<7;++index) {
            CEffectPrim prim;prim.m_master=&effect;prim.m_type=PP_SUPERANGEL;prim.m_numSegments=1;prim.m_size=1;
            auto& gi=prim.m_gi[0];gi.life=1;gi.flag[0]=static_cast<u8>(variants[index]);gi.sparkX=10;gi.sparkY=20;gi.sparkZ=30;
            UpdateSuperAngelPrimitive(&prim);
            check(prim.m_alpha==initial[index] && gi.process==1 && std::abs(prim.m_pos.y-19.9f)<0.00001f
                && prim.m_pos.x==10 && prim.m_segments[0].alpha==prim.m_alpha && gi.arcBaselineY==gi.sparkY,
                "SuperAngel GI drives initial alpha, fixed-origin drift and head segment state");
            gi.process=cutoffs[index];prim.m_alpha=100;
            UpdateSuperAngelPrimitive(&prim);
            check(prim.m_alpha==100-fades[index],"SuperAngel variants begin fading at their reference process boundary");
        }
        CEffectPrim prim;prim.m_master=&effect;prim.m_numSegments=1;auto& gi=prim.m_gi[0];gi.life=1;
        gi.flag[0]=0;prim.m_alpha=244;UpdateSuperAngelPrimitive(&prim);
        check(prim.m_alpha==249,"SuperAngel normal alpha rise preserves reference overshoot");
        gi.process=0;prim.m_alpha=-100;UpdateSuperAngelPrimitive(&prim);
        check(prim.m_alpha==-95,"SuperAngel preserves negative input alpha outside its explicit fade-clamp branches");
        gi.process=0;gi.flag[0]=10;prim.m_alpha=254;UpdateSuperAngelPrimitive(&prim);
        check(prim.m_alpha==255,"SuperAngel variant ten explicitly clamps its alpha rise");
        gi.process=0;gi.flag[0]=11;gi.flag[1]=9;prim.m_alpha=0;
        for(int i=0;i<40;++i)UpdateSuperAngelPrimitive(&prim);
        check(prim.m_alpha==31,"SuperAngel birth-frame trail alpha uses 40 minus birth before variant-specific rules");
        gi.process=75;prim.m_alpha=10;UpdateSuperAngelPrimitive(&prim);
        check(prim.m_alpha==7,"SuperAngel delayed trails fade by three after process 75");
        gi.flag[0]=0;gi.flag[1]=0;gi.process=32;prim.m_curMotion=0;UpdateSuperAngelPrimitive(&prim);
        check(prim.m_curMotion==1,"ordinary SuperAngel motion advances on multiples of three after process 30");
        gi.flag[0]=10;gi.process=31;prim.m_curMotion=0;UpdateSuperAngelPrimitive(&prim);
        check(prim.m_curMotion==1,"SuperAngel ten/eleven motion advances on multiples of four after process 30");
        gi.flag[0]=1;gi.process=31;prim.m_curMotion=50;UpdateSuperAngelPrimitive(&prim);
        check(prim.m_curMotion==29,"ordinary SuperAngel motion clamps at 29");
        gi.flag[0]=11;gi.process=31;prim.m_curMotion=50;UpdateSuperAngelPrimitive(&prim);
        check(prim.m_curMotion==15,"SuperAngel ten/eleven motion clamps at 15");
        gi.life=0;prim.m_deltaPos={4,5,6};const int process=gi.process;UpdateSuperAngelPrimitive(&prim);
        check(gi.process==process && prim.m_pos.x==4 && prim.m_segments[0].pos.y==5,
            "inactive SuperAngel GI still stores delta position and shifts its segment");
        matrix view;MatrixIdentity(view);prim.m_superAngelSprRes=&g_resMgr.spr;prim.m_superAngelActRes=&g_resMgr.act;
        g_resMgr.spr.Lock();g_resMgr.act.Lock();prim.m_alpha=30;prim.m_renderFlag=5;prim.m_curMotion=7;
        RenderSuperAngelPrimitive(prim,view);
        check(angelSubmittedMotion==7 && angelSubmittedColor==0x1EFFFFFFu && angelSubmittedBlend==2,
            "SuperAngel sprite submission uses explicit motion, GI alpha and additive flags");
        prim.m_renderFlag=0x201;RenderSuperAngelPrimitive(prim,view);
        check(angelSubmittedColor==0xFFFFFFFFu && angelSubmittedBlend==6,
            "SuperAngel clip-color flag does not multiply clip alpha by primitive alpha");
    }
    {
        CRagEffect effect;
        CEffectPrim prim;prim.m_master=&effect;
        for (auto type:{PP_MAPPILLAR,PP_PORTAL,PP_WIND}) {
            prim.m_type=type;
            check(!prim.UsesLegacyBandPayload(),"canonical skill primitive ids use reference GI payload");
            effect.m_isWorldEffect=true;
            check(prim.UsesLegacyBandPayload(),"retained map emitter dispatch preserves legacy band payload after enum correction");
            effect.m_isWorldEffect=false;
        }
        prim.m_type=PP_HEAL;effect.m_isWorldEffect=true;
        check(!prim.UsesLegacyBandPayload(),"legacy map payload exception is limited to its three original band kinds");
    }
    if (!failures) std::printf("%d effect lifecycle checks passed; no media loaded or rendered.\n", checks);
    return failures ? 1 : 0;
}
