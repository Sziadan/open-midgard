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
#include "ParticleMatrixBodies.inc"
float GetRadian(int degree) { return degree * 3.14159265f / 180.0f; }
float GetSin(int degree) { return std::sin(GetRadian(degree)); }
float GetCos(int degree) { return std::cos(GetRadian(degree)); }
int rngDraws = 0, waves = 0;
std::vector<int> rngValues;
size_t rngCursor=0;
int SilentRand() { ++rngDraws; return rngCursor < rngValues.size() ? rngValues[rngCursor++] : 0; }
float waveY = 99;
std::string waveName;
struct SilentWaveCall { std::string name; float x,y,z; int frameRng,volume,range;float frequency; };
std::vector<SilentWaveCall> waveCalls;
bool PlayWave(const char* name, float x, float y, float z, int volume, int range, float frequency) {
    waveCalls.push_back({name,x,y,z,rngDraws,volume,range,frequency});
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
struct CActRes : CRes { int count=8; int GetMotionCount(int) const { return count; } };
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
    float m_hpc=7, m_vpc=7, m_screenYFactor=1;
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
    float GetCameraLatitude() const { return -45; }
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
    struct Color { u8 a=0,r=0,g=0,b=0; } m_oldColor,m_curColor;
    struct SpriteColor { struct { Color s; } u; };
    struct { SpriteColor m_sprColor; } m_u28;
    int m_isSprArgbFixed=0;
    int tintCalls=0,alpha=123,red=1,green=2,blue=3;
    void SetArgb(int a,int r,int g,int b) {
        ++tintCalls;if(a!=-1)alpha=a;if(r!=-1)red=r;if(g!=-1)green=g;if(b!=-1)blue=b;
    }
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
struct SilentAttr {
    float height=1000,lastX=0,lastZ=0;int calls=0;
    float GetHeight(float x,float z){++calls;lastX=x;lastZ=z;return height;}
};
struct CWorld {
    SilentAttr* m_attr=nullptr;
    std::list<CGameObject*> m_gameObjectList;
    std::list<CSkill*> m_skillList;
    CRenderObject* m_player = nullptr;
    void UpdateGameObjects();
} g_world;
struct CGameMode {
    CWorld* m_world = &g_world; CView* m_view = &testView;
    int messages=0,lastMsg=0,lastArg=0;
    void SendMsg(int msg,msgparam_t arg,msgparam_t,msgparam_t) {++messages;lastMsg=msg;lastArg=static_cast<int>(arg);}
} gameMode;
struct ModeManager { CGameMode* GetCurrentGameMode() { return &gameMode; } } g_modeMgr;
#include "PrimitiveEnum.inc"
constexpr EFFECTPRIMID Other = PP_BUBBLE_DROP;
void UpdateSlash1Primitive(CEffectPrim*);
void UpdateSuperAngelPrimitive(CEffectPrim*);
bool UpdateLinearReferenceParticle(CEffectPrim*);
bool UpdateReferenceCircleState(CEffectPrim*);
bool UpdateReferenceCylinderState(CEffectPrim*);
bool UpdateReferenceCrossTexture(CEffectPrim*);
bool UpdateWarpZoneOrbit(CEffectPrim*);
struct CEffectPrim : CGameObject {
    Probe deaths{ primitiveDeaths };
    CRagEffect* m_master = nullptr;
    EFFECTPRIMID m_type = Other;
    EffectGi m_gi[4];
    u32 m_renderFlag = 1;
    int m_stateCnt = 0, m_duration = 10, updates = 0, observedFrame = -1, resetFrame = -1;
    int m_pattern = 0, m_spawnCount = 0, m_fadeOutCnt = 0;
    float m_outerSize = 0, m_arcAngle = 36, m_maxHeightSize = 100, m_widthSize=0;
    float m_outerSpeed=0,m_outerAccel=0,m_heightSpeed=0,m_heightAccel=0;
    int m_totalTexture=1;
    float m_size = 1, m_heightSize = 0, m_longitude = 0, m_latitude = 0, m_speed = 0, m_accel = 0;
    vector3d m_deltaPos, m_deltaPos2, m_deltaPosAccel, m_pos, m_orgPos;
    float m_roll = 0, m_rollSpeed = 0, m_rollAccel = 0, m_alphaDelta = 0, m_minAlpha = 0;
    bool m_isDisappear = false;
    matrix m_matrix = [] { matrix value; MatrixIdentity(value); return value; }();
    CSprRes* m_particleSprRes = nullptr;
    CActRes* m_particleActRes = nullptr;
    std::string m_spriteActName,m_spriteSprName;
    int m_spriteAction=0,m_animSpeed=0,m_curMotion=0,m_numSegments=0;
    bool m_repeatAnim=false,m_spriteRepeat=false;
    float m_alpha=0,m_radius=0,m_alphaSpeed=0,m_maxAlpha=254,m_sizeSpeed=0,m_sizeAccel=0;
    std::array<float,4> m_param{};
    float m_gravSpeed=0,m_gravAccel=0,m_latiSpeed=0;
    vector3d m_speed3d;
    float m_longSpeed=0,m_longAccel=0,m_radiusSpeed=0,m_radiusAccel=0,m_innerSpeed=0,m_innerAccel=0,m_innerSize=0;
    u32 m_tintColor=0xFFFFFF,m_argb=0xFFFFFFFF;
    std::array<EffectSegmentState,50> m_segments;

    std::vector<CTexture*> m_texture;
    std::vector<float> m_chPoint,m_chVal1;
    bool m_chSpeed=false;
    bool active = true;
    ~CEffectPrim() override;
    void Init(CRagEffect*, EFFECTPRIMID, const vector3d&);
    void SendMsg(CGameObject*, int, msgparam_t, msgparam_t, msgparam_t) override;
    u8 OnProcess() override;
    bool UsesLegacyBandPayload() const;
    bool UsesReferenceParticle() const;
    bool UsesReferenceCircle() const;
    bool UsesReferenceCylinder() const;
    bool UsesReferenceCrossTexture() const;
    void SetParticleResources(const char*, const char*,bool = false);
    bool ProcessPrimitiveFrame() {
        ++updates;
        observedFrame = m_stateCnt;
        if (m_type == PP_SLASH1) UpdateSlash1Primitive(this);
        if (m_type == PP_SUPERANGEL) UpdateSuperAngelPrimitive(this);
        if (UsesReferenceCircle()) UpdateReferenceCircleState(this);
        if (UsesReferenceCrossTexture() && !UpdateReferenceCrossTexture(this)) return false;
        if (UsesReferenceCylinder() && !UpdateReferenceCylinderState(this)) return false;
        if (m_type == PP_3DPARTICLEORBIT && UsesReferenceParticle() && !UpdateWarpZoneOrbit(this)) return false;
        if (m_type == PP_3DPARTICLE && UsesReferenceParticle() && !UpdateLinearReferenceParticle(this)) return false;
        if (resetFrame >= 0) m_stateCnt = resetFrame;
        return active;
    }
};
int angelSpriteSubmissions=0,angelSubmittedMotion=-1;
u32 angelSubmittedColor=0;
D3DBLEND angelSubmittedBlend=0;
u32 RGB(u8 r,u8 g,u8 b) { return r | (u32(g)<<8) | (u32(b)<<16); }
u32 PackColor(unsigned int alpha, u32 color) {
    return (alpha << 24) | ((color & 255) << 16) | (color & 0xFF00) | ((color >> 16) & 255);
}
void RenderParticleSprites(const CEffectPrim& prim,CSprRes&,const CActRes&,const matrix&,float) {
    ++angelSpriteSubmissions;angelSubmittedMotion=prim.m_curMotion;
    angelSubmittedColor=prim.m_segments[0].argb;
    angelSubmittedBlend=prim.m_renderFlag==5?2:6;
}
struct CRagEffect : CGameObject {
    Probe deaths{ effectDeaths };
    CRenderObject* m_master = nullptr;
    bool m_ownsMaster = false, m_masterIsActor = false, m_isWorldEffect = false;
    u32 m_masterActorGid = 0;
    int m_type = 0, m_stateCnt = 0, m_duration = 3, m_count = 0, m_level = 0;
    float m_latitude=0;
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
    void SpawnRuwach();
    void ApplyReadyPortal();
    void ApplyPortal();
    void ApplyWarpZone2(const char*);
    void ApplyWarpZone();
    void ApplyIceArrow();
    void ApplyPotionPillar(float,int);
    bool ProcessPotionConcentration();
    bool ApplyPotionBerserk();
    void ApplySight(float,bool);
    void SpawnSight();
    void SpawnSight2();
    void SpawnBlessing();
    void SpawnSmoke();
    void SpawnTorch();
    void SpawnEnchantPoison();
    void SpawnEnchantPoison2();
    void ProcessEnchantPoison2();
    void ApplyEntry2();
    bool ApplyJobLvUp50(int);
    bool ApplyJobLvUp50_2(short);
    void ProcessJobLvUp50_2();
    ~CRagEffect() override;
    CRenderObject* ResolveCurrentMaster() { return m_master; }
    CAbleToMakeEffect* ResolveCurrentEffectOwner() { return dynamic_cast<CAbleToMakeEffect*>(m_master); }
    bool IsWorldEffect() const { return m_isWorldEffect; }
    int GetDuration() const { return m_duration; }
    int GetStateCount() const { return m_stateCnt; }
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
    void AbsorbPrimDelta(const vector3d& delta){m_deltaPos=delta;}
    void SetGroundHitFlag(){m_flag=1;}
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
        check(effect.m_primList.size()==1 && !effect.m_primList.front()->m_particleSprRes
            && !effect.m_primList.front()->m_particleActRes,
            "missing SuperAngel assets leave one unrenderable original primitive without alternate names or replacement textures");
        matrix view;MatrixIdentity(view);const int submitted=angelSpriteSubmissions;
        RenderReferenceParticlePrimitive(*effect.m_primList.front(),view);
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
        matrix view;MatrixIdentity(view);prim.m_particleSprRes=&g_resMgr.spr;prim.m_particleActRes=&g_resMgr.act;
        g_resMgr.spr.Lock();g_resMgr.act.Lock();prim.m_alpha=30;prim.m_renderFlag=5;prim.m_curMotion=7;ShiftCircleSegments(&prim);
        RenderReferenceParticlePrimitive(prim,view);
        check(angelSubmittedMotion==7 && angelSubmittedColor==0x1EFFFFFFu && angelSubmittedBlend==2,
            "SuperAngel sprite submission uses explicit motion, GI alpha and additive flags");
        prim.m_renderFlag=0x201;RenderReferenceParticlePrimitive(prim,view);
        check(angelSubmittedColor==0x1EFFFFFFu && angelSubmittedBlend==6,
            "SuperAngel render flag 0x201 preserves segment alpha and uses ordinary blending");
    }
    {
        CEffectPrim prim;prim.m_numSegments=2;prim.m_alpha=259.75f;prim.m_size=4;
        prim.m_tintColor=0x563412;
        ShiftCircleSegments(&prim);
        check(prim.m_segments[0].argb==0x03123456u,"segment head packs truncated wrapping alpha and primitive RGB");
        check(prim.m_segments[1].argb==0x81123456u,"history segment packs its own attenuated alpha");
    }
    {
        CRenderObject actor;actor.m_pos={10,20,30};
        CRagEffect effect;effect.m_master=&actor;effect.m_type=33;effect.m_duration=100;
        const int initialRng=rngDraws,initialWaves=waves;
        g_resMgr.requests.clear();effect.m_stateCnt=1;effect.SpawnRuwach();
        check(effect.m_primList.empty() && effect.m_effectPos.x==10,"Ruwach copies master position even on non-emission frames");
        effect.m_stateCnt=3;effect.SpawnRuwach();
        check(effect.m_primList.size()==2 && rngDraws==initialRng && waves==initialWaves,"Ruwach emits two particles every third frame without RNG or wave calls");
        auto it=effect.m_primList.begin();auto* particle=*it++;auto* shadow=*it;
        check(g_resMgr.requests.size()==4 && g_resMgr.requests[0]=="\xC0\xCC\xC6\xD1\xC6\xAE\\particle2.spr"
            && g_resMgr.requests[1]=="\xC0\xCC\xC6\xD1\xC6\xAE\\particle2.act"
            && g_resMgr.requests[2]=="Shadow.spr" && g_resMgr.requests[3]=="Shadow.act", "Ruwach uses original sprite/action names and sprite-then-action load order");
        check(std::fabs(particle->m_deltaPos2.x-GetSin(-11)*15)<0.0001f
            && std::fabs(particle->m_deltaPos2.z-GetCos(-11)*-15)<0.0001f,
            "Ruwach angle truncates toward zero to integer degrees before trig lookup");
        check(particle->m_deltaPos2.y==-10 && shadow->m_deltaPos2.y==0
            && particle->m_deltaPos2.x==shadow->m_deltaPos2.x && particle->m_deltaPos2.z==shadow->m_deltaPos2.z,
            "Ruwach particle/shadow share horizontal offset with distinct source heights");
        check(particle->m_duration==25 && shadow->m_duration==25 && particle->m_fadeOutCnt==19 && shadow->m_fadeOutCnt==19,
            "Ruwach retains twenty-five-frame duration and frame-nineteen fade");
        check(particle->m_size==3 && particle->m_sizeSpeed==-0.1f && particle->m_alpha==250 && particle->m_alphaSpeed==-3,
            "Ruwach particle has original size and alpha profile");
        check(shadow->m_size==1.5f && shadow->m_sizeSpeed==-0.059999999f && shadow->m_alpha==150 && shadow->m_alphaSpeed==-2.5f,
            "Ruwach shadow has original size and alpha profile");
        check(particle->m_animSpeed==4 && shadow->m_animSpeed==2 && particle->m_maxAlpha==254 && particle->m_repeatAnim,
            "Ruwach restores reference constructor motion and alpha defaults");
        check(particle->m_numSegments==1 && particle->m_segments[0].argb==0xFAFFFFFF && shadow->m_segments[0].argb==0x96FFFFFF,
            "Ruwach initializes one colored segment before the first update");
        check(particle->UsesReferenceParticle() && shadow->UsesReferenceParticle(),"Ruwach uses reference particle dispatch");
        check(particle->OnProcess()==1 && particle->m_stateCnt==1 && particle->m_curMotion==1 && std::fabs(particle->m_size-2.9f)<0.00001f
            && particle->m_alpha==247,"Ruwach frame zero advances motion after shrinking and fading");
        check(shadow->OnProcess()==1 && shadow->m_curMotion==1 && shadow->m_alpha==147.5f,"Ruwach shadow also advances motion on zero");
        check(particle->m_segments[0].argb==0xF7FFFFFF && std::fabs(particle->m_pos.y-10)<0.0001f,
            "Ruwach update stores segment alpha and parent-relative position");
        effect.m_effectPos={40,50,60};particle->m_stateCnt=1;particle->OnProcess();
        check(std::fabs(particle->m_pos.x-(40+particle->m_deltaPos2.x))<0.0001f && particle->m_pos.y==40,
            "Ruwach follows persistent parent position without velocity or ground fallback");
        particle->m_stateCnt=19;particle->m_alpha=180;particle->m_alphaSpeed=-3;particle->OnProcess();
        check(particle->m_alphaSpeed==-30 && particle->m_alpha==150,"Ruwach computes fade speed from current alpha at frame nineteen");
        particle->m_stateCnt=20;particle->m_alpha=10;particle->m_alphaSpeed=-30;particle->OnProcess();
        check(particle->m_alpha==0 && particle->m_segments[0].argb==0x00FFFFFF,"Ruwach fading clamps at minimum before storing segment color");
        particle->m_stateCnt=4;particle->m_curMotion=7;particle->OnProcess();
        check(particle->m_curMotion==0,"Ruwach looping animation wraps by actual action motion count");
        particle->m_stateCnt=25;check(particle->OnProcess()==0 && particle->m_stateCnt==26,"Ruwach retains inclusive primitive final frame");
        effect.m_stateCnt=99;particle->m_stateCnt=0;particle->m_alpha=120;particle->m_isDisappear=false;particle->OnProcess();
        check(particle->m_isDisappear && particle->m_alphaSpeed==-120 && particle->m_alpha==0,
            "Ruwach fades immediately when parent has only one frame remaining");
        // Deletion is performed by the world, retaining balanced resource locks.
        effect.ClearPrims();g_world.UpdateGameObjects();
        check(g_resMgr.spr.m_lockCnt==0 && g_resMgr.act.m_lockCnt==0,"Ruwach resource locks release on world deletion");
        g_resMgr.fail=true;effect.m_stateCnt=0;effect.SpawnRuwach();g_resMgr.fail=false;
        check(effect.m_primList.size()==2 && effect.m_primList.front()->UsesReferenceParticle()
            && effect.m_primList.front()->m_particleSprRes==nullptr,"missing Ruwach resources keep reference routing without substitute assets");
        effect.ClearPrims();g_world.UpdateGameObjects();
    }
    {
        CRagEffect effect;effect.m_type=33;effect.m_duration=100;
        CEffectPrim prim;prim.m_master=&effect;prim.m_type=PP_3DPARTICLE;
        prim.m_particleActRes=&g_resMgr.act;g_resMgr.act.Lock();g_resMgr.act.count=3;
        prim.m_animSpeed=2;prim.m_repeatAnim=false;prim.m_stateCnt=2;prim.m_curMotion=2;
        check(prim.OnProcess()==0 && prim.m_stateCnt==2 && prim.m_curMotion==3,
            "reference particle animation completion returns before outer counter increment");
        prim.m_curMotion=2;prim.m_pattern=0x800;
        check(AnimateReferenceParticle(&prim) && prim.m_curMotion==2,"reference particle stop-at-last flag keeps terminal motion");
        prim.m_pattern=0;prim.m_curMotion=3;
        check(AnimateReferenceParticle(&prim) && prim.m_curMotion==4,"reference nonloop completion compares equality rather than clamping overflow");
        prim.m_repeatAnim=true;prim.m_curMotion=2;
        check(AnimateReferenceParticle(&prim) && prim.m_curMotion==0,"reference repeat wraps immediately at motion count");
        prim.m_stateCnt=3;prim.m_curMotion=1;
        check(AnimateReferenceParticle(&prim) && prim.m_curMotion==1,"reference motion is unchanged between animation divisors");
        g_resMgr.act.count=8;
    }
    {
        CRenderObject actor;actor.m_pos={100,50,200};
        CRagEffect effect;effect.m_type=44;effect.m_master=&actor;effect.m_duration=90;
        const int beforeRng=rngDraws,beforeWaves=waves;
        rngValues={1,0,90,0,29,180,2};rngCursor=0;g_resMgr.requests.clear();
        effect.SpawnSmoke();
        check(effect.m_primList.size()==2 && rngDraws-beforeRng==7 && rngCursor==7,"Smoke consumes one count draw then X-angle, Y-angle and speed for each particle");
        check(effect.m_effectPos.x==100 && waves==beforeWaves,"Smoke copies parent position without a wave call");
        auto it=effect.m_primList.begin();auto* first=*it++;auto* second=*it;
        check(first->m_alpha==254 && first->m_maxAlpha==254 && first->m_alphaSpeed==0 && first->m_segments[0].argb==0xFEFFFFFF,
            "Smoke retains original constructor alpha instead of synthetic fade-in");
        check(first->m_duration==90 && first->m_size==1.5f && first->m_deltaPos2.y==-9 && first->m_rollSpeed==0.75f && first->m_fadeOutCnt==60,
            "Smoke preserves source size, offset, roll and two-thirds fade point");
        check(first->m_animSpeed==4 && first->m_repeatAnim && first->m_numSegments==1 && first->UsesReferenceParticle(),
            "Smoke initializes one reference sprite segment with looping divisor four");
        check(std::fabs(first->m_speed-0.3f)<0.000001f && std::fabs(second->m_speed-0.5f)<0.000001f,
            "Smoke speed draws preserve original minimum and maximum");
        check(std::fabs(first->m_matrix.m[2][0]-std::cos(GetRadian(75)))<0.000001f
            && std::fabs(first->m_matrix.m[2][1]+std::sin(GetRadian(75)))<0.000001f,
            "Smoke composes X rotation followed by Y rotation with production matrix code");
        check(std::fabs(second->m_matrix.m[2][1]+std::sin(GetRadian(104)))<0.000001f,
            "Smoke X-angle draw reaches original upper boundary 104 degrees");
        check(g_resMgr.requests.size()==4 && first->m_spriteSprName=="\xC0\xCC\xC6\xD1\xC6\xAE\\\xB1\xBC\xB6\xD2\xBF\xAC\xB1\xE2.spr"
            && first->m_spriteActName=="\xC0\xCC\xC6\xD1\xC6\xAE\\\xB1\xBC\xB6\xD2\xBF\xAC\xB1\xE2.act",
            "Smoke loads only the original recovered sprite and action");
        first->OnProcess();
        check(first->m_curMotion==1 && first->m_alpha==254 && first->m_roll==0.75f
            && std::fabs(first->m_pos.x-(100+0.3f*std::cos(GetRadian(75))))<0.00001f
            && std::fabs(first->m_pos.y-(41-0.3f*std::sin(GetRadian(75))))<0.00001f,
            "Smoke first frame moves along its stored matrix, rotates and advances motion with unchanged alpha");
        first->m_stateCnt=60;first->OnProcess();
        check(std::fabs(first->m_alphaSpeed+254.0f/30)<0.00001f && first->m_alpha<254,
            "Smoke begins source fade exactly at duration minus duration divided by three");
        effect.m_stateCnt=1;actor.m_pos.x=130;const int draws=rngDraws;effect.SpawnSmoke();
        check(effect.m_primList.size()==2 && rngDraws==draws && effect.m_effectPos.x==130,
            "Smoke updates parent position after frame zero without re-emission or RNG");
        effect.ClearPrims();g_world.UpdateGameObjects();
        rngValues.clear();rngCursor=0;
        effect.m_stateCnt=0;effect.SpawnSmoke();
        check(effect.m_primList.size()==1,"Smoke zero count draw emits exactly one particle");
        effect.ClearPrims();g_world.UpdateGameObjects();
        rngValues={3};rngCursor=0;effect.SpawnSmoke();
        check(effect.m_primList.size()==4,"Smoke maximum count draw emits exactly four particles");
        effect.ClearPrims();g_world.UpdateGameObjects();rngValues.clear();rngCursor=0;
        check(g_resMgr.spr.m_lockCnt==0 && g_resMgr.act.m_lockCnt==0,"Smoke world cleanup releases each resource lock");
    }
    {
        CRenderObject actor;actor.m_pos={5,20,25};
        CRagEffect effect;effect.m_type=47;effect.m_master=&actor;effect.m_duration=80;
        effect.m_param[0]=-2;effect.m_param[1]=0.25f;
        const int draws=rngDraws,beforeWaves=waves;g_resMgr.requests.clear();effect.SpawnTorch();
        check(effect.m_primList.size()==1 && rngDraws==draws && waves==beforeWaves,"Torch emits once without RNG or waves");
        auto* prim=effect.m_primList.front();
        check(effect.m_param[0]==1 && effect.m_param[1]==1 && prim->m_size==1 && prim->m_animSpeed==1,
            "Torch repairs invalid size and speed in persistent parent parameters");
        check(prim->m_pattern==0x200 && prim->m_alpha==254 && prim->m_alphaSpeed==0 && prim->m_maxAlpha==254
            && prim->m_fadeOutCnt==-1 && prim->m_repeatAnim && prim->m_numSegments==1,
            "Torch keeps source clip-color pattern and constructor alpha/fade/segment defaults");
        check(prim->m_duration==80 && prim->m_deltaPos2.y==-10 && prim->UsesReferenceParticle(),
            "Torch preserves parent duration and source vertical offset in reference routing");
        check(g_resMgr.requests.size()==2 && prim->m_spriteSprName=="\xC0\xCC\xC6\xD1\xC6\xAE\\torch_01.spr"
            && prim->m_spriteActName=="\xC0\xCC\xC6\xD1\xC6\xAE\\torch_01.act",
            "Torch requests only the original torch_01 sprite and action");
        prim->OnProcess();
        check(prim->m_alpha==254 && prim->m_curMotion==1 && prim->m_pos.y==10 && prim->m_pattern==0x200,
            "Torch first frame retains alpha and clip-color control while advancing motion");
        effect.m_stateCnt=1;actor.m_pos.z=50;effect.SpawnTorch();
        check(effect.m_primList.size()==1 && effect.m_effectPos.z==50,"Torch copies parent on later frames without repeated emission");
        effect.ClearPrims();g_world.UpdateGameObjects();
        effect.m_stateCnt=0;effect.m_param[0]=0.5f;effect.m_param[1]=2.9f;effect.SpawnTorch();prim=effect.m_primList.front();
        check(effect.m_param[0]==0.5f && effect.m_param[1]==2.9f && prim->m_size==0.5f && prim->m_animSpeed==2,
            "Torch retains positive fractional size and truncates the original animation divisor");
        effect.ClearPrims();g_world.UpdateGameObjects();
        g_resMgr.fail=true;effect.SpawnTorch();g_resMgr.fail=false;prim=effect.m_primList.front();
        check(prim->UsesReferenceParticle() && !prim->m_particleSprRes && prim->m_texture.empty(),
            "missing Torch assets keep reference dispatch without color-name or bitmap fallbacks");
        effect.ClearPrims();g_world.UpdateGameObjects();
        check(g_resMgr.spr.m_lockCnt==0 && g_resMgr.act.m_lockCnt==0,"Torch world cleanup releases resource locks");
    }
    {
        CRenderObject actor;actor.m_pos={100,200,300};
        CRenderObject listener;listener.m_pos={-5,900,4};
        auto* oldPlayer=g_world.m_player;g_world.m_player=&listener;
        CRagEffect effect;effect.m_master=&actor;effect.m_type=20;effect.m_duration=200;effect.m_effectPos={10,70,30};
        rngValues={90,5,29,49};rngCursor=0;const int beforeRng=rngDraws;
        waveCalls.clear();g_resMgr.requests.clear();effect.SpawnEnchantPoison();
        check(effect.m_primList.size()==1 && rngDraws-beforeRng==4 && rngCursor==4,
            "EnchantPoison emits one particle using exactly angle, radius, speed and size draws");
        check(waveCalls.size()==2 && waveCalls[0].name=="effect\\assasin_enchantpoison.wav" && waveCalls[1].name=="effect\\EF_PoisonAttack.wav",
            "EnchantPoison plays both original waves in order on frame zero");
        check(waveCalls[0].x==15 && waveCalls[0].y==0 && waveCalls[0].z==26
            && waveCalls[1].y==0 && waveCalls[0].volume==250 && waveCalls[0].range==40 && waveCalls[0].frequency==1,
            "poison sounds use stored effect minus listener horizontally with source zero y and volume parameters");
        check(waveCalls[0].frameRng==beforeRng && waveCalls[1].frameRng==beforeRng,
            "poison wave calls precede particle RNG draws");
        auto* prim=effect.m_primList.front();
        check(std::fabs(prim->m_deltaPos2.x-7)<0.000001f && std::fabs(prim->m_deltaPos2.z)<0.000001f,
            "poison spawn uses the original random Y rotation and radius upper boundary");
        check(std::fabs(prim->m_speed-static_cast<float>(59*0.0099999998))<0.000001f
            && std::fabs(prim->m_size-static_cast<float>(89*0.0099999998))<0.000001f,
            "EnchantPoison speed and size use their exact upper profiles");
        check(std::fabs(prim->m_matrix.m[2][1]+1)<0.000001f && std::fabs(prim->m_matrix.m[2][0])<0.000001f,
            "poison replaces spawn rotation with X ninety-degree motion matrix");
        check(prim->m_maxAlpha==254 && prim->m_alpha==0 && std::fabs(prim->m_alphaSpeed-25.4f)<0.000001f
            && prim->m_duration==40 && prim->m_fadeOutCnt==32 && prim->m_animSpeed==4 && prim->m_repeatAnim && prim->m_numSegments==1,
            "poison retains constructor and source alpha, fade, duration and animation defaults");
        check(g_resMgr.requests.size()==2 && prim->m_spriteSprName=="\xC0\xCC\xC6\xD1\xC6\xAE\\particle3.spr"
            && prim->m_spriteActName=="\xC0\xCC\xC6\xD1\xC6\xAE\\particle3.act" && prim->UsesReferenceParticle(),
            "poison requests only exact particle3 resources and uses reference rendering");
        check(effect.m_effectPos.x==10 && prim->m_segments[0].pos.y==70 && prim->m_segments[0].argb==0x00FFFFFF,
            "poison preserves stored origin and initializes transparent source segment");
        const float speed=prim->m_speed,accel=prim->m_accel;
        prim->OnProcess();
        check(std::fabs(prim->m_speed-(speed+accel))<0.000001f && std::fabs(prim->m_pos.y-(70-speed-accel))<0.00001f
            && prim->m_roll==3 && prim->m_curMotion==1 && std::fabs(prim->m_alpha-25.4f)<0.000001f,
            "poison frame zero accelerates, rises, rotates, increases alpha and advances motion");
        prim->m_stateCnt=32;prim->m_alpha=128;prim->OnProcess();
        check(prim->m_alphaSpeed==-16 && prim->m_alpha==112,"poison frame thirty-two fade derives from current alpha over eight frames");
        effect.m_stateCnt=1;actor.m_pos={900,900,900};const int draws=rngDraws;effect.SpawnEnchantPoison();
        check(effect.m_primList.size()==1 && waveCalls.size()==2 && rngDraws==draws && effect.m_effectPos.x==10,
            "poison non-emission frame neither follows actor nor repeats audio or RNG");
        rngValues.clear();rngCursor=0;effect.m_stateCnt=5;effect.SpawnEnchantPoison();
        check(effect.m_primList.size()==2 && waveCalls.size()==2,"EnchantPoison repeats only particles at five-frame intervals");
        effect.ClearPrims();g_world.UpdateGameObjects();
        effect.m_type=125;effect.m_stateCnt=0;effect.SpawnEnchantPoison();prim=effect.m_primList.front();
        check(prim->UsesReferenceParticle() && std::fabs(prim->m_speed-0.3f)<0.000001f && std::fabs(prim->m_size-0.4f)<0.000001f,
            "effect one-twenty-five shares original EnchantPoison profile including lower RNG boundaries");
        effect.ClearPrims();g_world.UpdateGameObjects();
        g_resMgr.fail=true;effect.SpawnEnchantPoison();g_resMgr.fail=false;prim=effect.m_primList.front();
        check(prim->UsesReferenceParticle() && !prim->m_particleSprRes && prim->m_texture.empty(),
            "missing poison sprite retains reference routing without particle2 or texture substitution");
        effect.ClearPrims();g_world.UpdateGameObjects();g_world.m_player=oldPlayer;
        check(g_resMgr.spr.m_lockCnt==0 && g_resMgr.act.m_lockCnt==0,"poison resource locks release through world ownership");
    }
    {
        CRenderObject actor,listener;listener.m_pos={1,500,2};
        auto* oldPlayer=g_world.m_player;g_world.m_player=&listener;
        CRagEffect effect;effect.m_master=&actor;effect.m_type=493;effect.m_duration=200;effect.m_effectPos={11,40,22};
        waveCalls.clear();rngValues={359,5,49,19};rngCursor=0;
        effect.ProcessEnchantPoison2();auto* prim=effect.m_primList.front();
        check(std::fabs(prim->m_speed-static_cast<float>(79*0.0099999998))<0.000001f
            && std::fabs(prim->m_size-static_cast<float>(39*0.0099999998))<0.000001f,
            "EnchantPoison2 uses its separate speed and size upper profiles");
        check(prim->UsesReferenceParticle() && actor.alpha==123 && actor.red==255 && actor.green==0 && actor.blue==255 && actor.m_BodyLight==1,
            "effect four-ninety-three sets magenta and low light bit without changing actor alpha");
        rngValues.clear();rngCursor=0;
        std::vector<int> soundFrames;
        if(!waveCalls.empty())soundFrames.push_back(0);
        for(int frame=1;frame<=82;++frame) {
            const auto before=waveCalls.size();effect.m_stateCnt=frame;effect.ProcessEnchantPoison2();
            if(waveCalls.size()!=before)soundFrames.push_back(frame);
            if(frame==1)check(actor.green==255 && actor.m_BodyLight==1,"poison two odd frames use shared white-tint branch without toggling light bit");
            if(frame==80)check(actor.green==0 && actor.tintCalls==81,"poison two retains alternating tint through inclusive frame eighty");
        }
        check(soundFrames==std::vector<int>({0,5,11,18,26,40}),"poison two plays only its six original scheduled sound frames");
        check(waveCalls.size()==6 && waveCalls.back().name=="effect\\assasin_enchantpoison.wav"
            && waveCalls.back().x==10 && waveCalls.back().y==0 && waveCalls.back().z==20,
            "poison two audio keeps stored horizontal origin and has no extra poison-attack wave");
        check(effect.m_primList.size()==28,"poison two emits every third frame including frames beyond the tint window");
        check(actor.tintCalls==81 && actor.green==0,"poison two stops tint writes at frame eighty-one without forced restoration");
        effect.m_stateCnt=6;actor.m_BodyLight=6;effect.ProcessEnchantPoison2();
        check(actor.m_BodyLight==7,"poison two sets only missing low light bit while preserving higher flags");
        effect.m_stateCnt=8;effect.ProcessEnchantPoison2();
        check(actor.m_BodyLight==7,"poison two does not increment a light bit that is already set");
        effect.ClearPrims();g_world.UpdateGameObjects();g_world.m_player=oldPlayer;
        check(g_resMgr.spr.m_lockCnt==0 && g_resMgr.act.m_lockCnt==0,"poison two releases all looping particle resource locks");
    }
    {
        CRenderObject actor,listener;actor.m_pos={10,80,30};listener.m_pos={2,900,4};
        auto* oldPlayer=g_world.m_player;g_world.m_player=&listener;
        CRagEffect effect;effect.m_type=42;effect.m_master=&actor;effect.m_duration=150;
        waveCalls.clear();g_resMgr.requests.clear();rngValues={90,6,19};rngCursor=0;
        const int draws=rngDraws;effect.SpawnBlessing();
        check(effect.m_primList.size()==3 && rngDraws-draws==3,"Blessing frame zero emits circle, burst and recurring particle with three RNG draws");
        auto it=effect.m_primList.begin();auto* circle=*it++;auto* burst=*it++;auto* spark=*it;
        check(circle->m_type==PP_3DCIRCLE && burst->m_type==PP_3DPARTICLE && spark->m_type==PP_3DPARTICLE,
            "Blessing preserves circle/burst/spark launch order");
        check(waveCalls.size()==1 && waveCalls[0].name=="effect\\EF_Blessing.wav" && waveCalls[0].x==8 && waveCalls[0].y==0 && waveCalls[0].z==26
            && waveCalls[0].frameRng==draws,"Blessing sound uses horizontal listener delta and precedes particle RNG");
        check(circle->m_pattern==1 && circle->m_renderFlag==0x201 && circle->m_radius==10 && circle->m_latitude==90
            && circle->m_tintColor==RGB(32,176,232),"Blessing circle retains source flags, orientation, radius and tint");
        check(circle->m_duration==150 && circle->m_maxAlpha==100 && std::fabs(circle->m_alphaSpeed-3.3333335f)<0.000001f
            && circle->m_fadeOutCnt==120 && circle->m_numSegments==1 && circle->m_segments[0].argb==0x0020B0E8,
            "Blessing circle initializes reference alpha and segment profile");
        check(circle->m_texture.size()==1 && std::string(g_texMgr.lastPath)=="effect\\alpha_down.tga",
            "Blessing circle requests only original alpha_down texture");
        check(burst->m_duration==150 && burst->m_alpha==200 && burst->m_deltaPos2.y==-25 && burst->m_param[2]==0
            && burst->m_animSpeed==2 && burst->m_fadeOutCnt==-1 && burst->m_repeatAnim && burst->m_numSegments==1,
            "Blessing burst stores source offset and looping constructor profile");
        check(burst->UsesReferenceParticle() && spark->UsesReferenceParticle() && burst->m_maxAlpha==254 && spark->m_maxAlpha==254,
            "both Blessing sprite profiles use reference state and per-clip rendering");
        check(g_resMgr.requests.size()==4 && g_resMgr.requests[0]=="\xC0\xCC\xC6\xD1\xC6\xAE\\\xC3\xE0\xBA\xB9.spr"
            && g_resMgr.requests[1]=="\xC0\xCC\xC6\xD1\xC6\xAE\\\xC3\xE0\xBA\xB9.act"
            && g_resMgr.requests[2]=="\xC0\xCC\xC6\xD1\xC6\xAE\\particle6.act"
            && g_resMgr.requests[3]=="\xC0\xCC\xC6\xD1\xC6\xAE\\particle6.spr",
            "Blessing preserves burst sprite-first and recurring action-first load order with exact assets");
        check(spark->m_duration==80 && spark->m_pattern==0x80 && spark->m_size==0.5f && spark->m_alpha==0
            && spark->m_fadeOutCnt==64 && spark->m_animSpeed==4 && spark->m_repeatAnim,
            "Blessing recurring particle retains twinkle, fade, size and animation profile");
        check(std::fabs(spark->m_deltaPos2.x-8)<0.000001f && spark->m_deltaPos2.y==-25
            && std::fabs(spark->m_speed-static_cast<float>(39*0.0099999998))<0.000001f,
            "Blessing angle/radius/speed RNG draws reach source profile boundaries");
        check(std::fabs(spark->m_matrix.m[2][1]-1)<0.000001f,"Blessing recurring motion uses fresh X minus-ninety matrix");
        burst->OnProcess();
        check(burst->m_curMotion==1 && burst->m_alpha==200 && burst->m_pos.y==55,
            "Blessing burst frame zero advances looping motion and follows parent offset");
        const float speed=spark->m_speed,accel=spark->m_accel;spark->OnProcess();
        check(spark->m_curMotion==1 && std::fabs(spark->m_alpha-25.4f)<0.000001f && std::fabs(spark->m_pos.y-(55+speed+accel))<0.00001f,
            "Blessing recurring particle rises and increases alpha on frame zero");
        circle->OnProcess();
        check(circle->m_pos.y==80 && circle->m_size==10 && std::fabs(circle->m_alpha-3.3333335f)<0.000001f
            && circle->m_segments[0].argb==0x0320B0E8,"Blessing circle follows persistent parent height and stores source alpha before outer increment");
        effect.m_effectPos={20,95,40};circle->m_stateCnt=120;circle->m_alpha=90;circle->OnProcess();
        check(circle->m_pos.y==95 && circle->m_alphaSpeed==-3 && circle->m_alpha==87,
            "Blessing circle uses original current-alpha fade without ground-height substitution");
        spark->m_stateCnt=10;spark->m_alpha=254;spark->m_alphaSpeed=25.4f;ProcessReferenceParticleAlpha(spark);
        check(spark->m_alphaSpeed==-25.4f && std::fabs(spark->m_alpha-228.6f)<0.00001f,
            "reference twinkle reverses alpha speed at maximum before adding");
        spark->m_alpha=0;spark->m_alphaSpeed=-25.4f;ProcessReferenceParticleAlpha(spark);
        check(spark->m_alphaSpeed==25.4f && spark->m_alpha==25.4f,"reference twinkle reverses alpha speed at minimum");
        spark->m_alpha=253;spark->m_alphaSpeed=25.4f;ProcessReferenceParticleAlpha(spark);
        check(spark->m_alpha==254 && spark->m_alphaSpeed==25.4f,"reference twinkle clamps overshoot without reversing until next process");
        spark->m_stateCnt=64;spark->m_alpha=160;spark->m_alphaSpeed=25.4f;ProcessReferenceParticleAlpha(spark);
        check(spark->m_pattern==0 && spark->m_alphaSpeed==-10 && spark->m_alpha==150,"Blessing recurring fade clears twinkle at frame sixty-four");
        spark->m_pattern=0x80;spark->m_isDisappear=false;spark->m_stateCnt=0;spark->m_alpha=100;spark->m_alphaSpeed=25.4f;
        effect.m_stateCnt=145;ProcessReferenceParticleAlpha(spark);
        check(spark->m_pattern==0 && spark->m_isDisappear && spark->m_alphaSpeed==-20 && spark->m_alpha==80,
            "parent-shortened fade overrides twinkle before alpha update");
        effect.ClearPrims();g_world.UpdateGameObjects();
        effect.m_stateCnt=0;waveCalls.clear();rngValues.clear();rngCursor=0;const int before=rngDraws;
        for(int frame=0;frame<=12;++frame){effect.m_stateCnt=frame;effect.SpawnBlessing();}
        int circles=0,bursts=0,sparks=0;std::vector<int> burstAlphas;
        for(auto* prim:effect.m_primList) {
            if(prim->m_type==PP_3DCIRCLE)++circles;
            else if(prim->m_pattern==0x80)++sparks;
            else{++bursts;burstAlphas.push_back(static_cast<int>(prim->m_alpha));}
        }
        check(circles==1 && bursts==4 && sparks==4 && rngDraws-before==12 && waveCalls.size()==1,
            "Blessing preserves independent zero/three/four-frame emission gates");
        check(burstAlphas==std::vector<int>({200,182,164,146}),"Blessing four burst alpha profiles follow original frame arithmetic");
        effect.ClearPrims();g_world.UpdateGameObjects();
        effect.m_stateCnt=1;actor.m_pos.y=123;const int beforeNoEmission=rngDraws;effect.SpawnBlessing();
        check(effect.m_effectPos.y==123 && effect.m_primList.empty() && rngDraws==beforeNoEmission,
            "Blessing copies parent on non-emission frames without extra RNG");
        g_resMgr.fail=true;effect.m_stateCnt=0;effect.SpawnBlessing();g_resMgr.fail=false;
        it=effect.m_primList.begin();++it;burst=*it++;spark=*it;
        check(burst->UsesReferenceParticle() && spark->UsesReferenceParticle() && !burst->m_particleSprRes && spark->m_texture.empty(),
            "missing Blessing sprite assets do not trigger texture substitutions");
        effect.ClearPrims();g_world.UpdateGameObjects();g_world.m_player=oldPlayer;
        check(g_resMgr.spr.m_lockCnt==0 && g_resMgr.act.m_lockCnt==0,"both Blessing resource orders release all locks on world deletion");
    }
    {
        CRenderObject actor;actor.m_pos={10,40,100};actor.m_BodyLight=7;
        CRagEffect effect;effect.m_master=&actor;effect.m_type=22;effect.m_duration=200;effect.m_tlvertZ=333;
        const auto savedRenderer=g_renderer;
        g_renderer.m_hpc=100;g_renderer.m_vpc=100;g_renderer.m_halfWidth=320;g_renderer.m_halfHeight=240;
        g_renderer.m_xoffset=1;g_renderer.m_yoffset=2;
        const int draws=rngDraws,beforeWaves=waves;g_resMgr.requests.clear();
        effect.m_stateCnt=1;effect.SpawnSight();
        check(effect.m_primList.empty() && effect.m_effectPos.y==40 && effect.m_tlvertX==330 && effect.m_tlvertY==280,
            "Sight copies parent and projects screen fields even on odd frames without emission");
        check(effect.m_tlvertZ==333,"Sight projection updates only original x and y fields");
        effect.m_stateCnt=2;effect.SpawnSight();
        check(effect.m_primList.size()==2 && rngDraws==draws && waves==beforeWaves,"Sight emits two particles every even frame without RNG or audio");
        auto it=effect.m_primList.begin();auto* sight=*it++;auto* shadow=*it;
        check(sight->m_type==PP_3DPARTICLE && shadow->m_type==PP_3DPARTICLE && sight->m_pattern==0,
            "Sight uses ordinary particles with no vector-acceleration movement flag");
        check(sight->m_deltaPosAccel.x==0 && sight->m_deltaPosAccel.y==0.1f && sight->m_deltaPosAccel.z==0,
            "Sight preserves original inactive Y-acceleration store");
        check(std::fabs(sight->m_deltaPos2.x-GetSin(-10)*15)<0.000001f
            && std::fabs(sight->m_deltaPos2.z-GetCos(-10)*-15)<0.000001f,
            "Sight orbit offset uses original integer negative-five-times-frame table angle");
        check(sight->m_deltaPos2.y==-20 && shadow->m_deltaPos2.y==0
            && sight->m_deltaPos2.x==shadow->m_deltaPos2.x && sight->m_deltaPos2.z==shadow->m_deltaPos2.z,
            "Sight and Shadow preserve shared horizontal orbit with distinct source heights");
        check(g_resMgr.requests==std::vector<std::string>({"\xC0\xCC\xC6\xD1\xC6\xAE\\Sight.spr","\xC0\xCC\xC6\xD1\xC6\xAE\\Sight.act","Shadow.spr","Shadow.act"}),
            "Sight loads original case-sensitive resources in sprite/action order without basename search");
        check(sight->m_alpha==150 && sight->m_alphaSpeed==-3 && sight->m_size==2.5f && sight->m_sizeSpeed==-0.1f
            && sight->m_animSpeed==3 && sight->m_duration==20,"Sight retains source primary alpha, size, duration and animation profile");
        check(shadow->m_alpha==120 && shadow->m_alphaSpeed==-1 && shadow->m_size==1.5f && shadow->m_sizeSpeed==-0.059999999f
            && shadow->m_animSpeed==2 && shadow->m_duration==20,"Sight retains original Shadow profile");
        check(sight->m_maxAlpha==254 && shadow->m_maxAlpha==254 && sight->m_fadeOutCnt==-1 && shadow->m_fadeOutCnt==-1
            && sight->m_repeatAnim && shadow->m_repeatAnim && sight->m_numSegments==1 && shadow->m_numSegments==1,
            "Sight profiles restore original constructor repeat, alpha, fade and segment defaults");
        check(sight->UsesReferenceParticle() && shadow->UsesReferenceParticle() && sight->m_tintColor==0xFFFFFF
            && sight->m_segments[0].argb==0x96FFFFFF && shadow->m_segments[0].argb==0x78FFFFFF,
            "Sight routes per-clip rendering with original white segment colors instead of orange tint");
        sight->OnProcess();shadow->OnProcess();
        check(sight->m_curMotion==1 && sight->m_alpha==147 && sight->m_pos.y==20 && std::fabs(sight->m_size-2.4f)<0.000001f,
            "Sight frame zero shrinks/fades and advances motion without applying the inactive acceleration");
        check(shadow->m_curMotion==1 && shadow->m_alpha==119 && shadow->m_pos.y==40,"Sight shadow advances independently on frame zero");
        for(int i=0;i<4;++i)sight->OnProcess();
        check(sight->m_pos.y==20 && sight->m_deltaPos.y==0 && sight->m_speed3d.y==0,"Sight remains at fixed relative height across updates without invented gravity");
        actor.m_pos={20,90,200};effect.m_stateCnt=3;effect.SpawnSight();sight->OnProcess();
        check(effect.m_primList.size()==2 && effect.m_effectPos.y==90 && sight->m_pos.y==70,
            "Sight odd-frame parent refresh propagates to existing particles");
        check(actor.m_BodyLight==7 && actor.tintCalls==0,"Sight introduces no actor light or tint changes");
        effect.ClearPrims();g_world.UpdateGameObjects();
        effect.m_stateCnt=0;g_resMgr.fail=true;effect.SpawnSight();g_resMgr.fail=false;sight=effect.m_primList.front();
        check(effect.m_primList.size()==2 && sight->UsesReferenceParticle() && !sight->m_particleSprRes && sight->m_texture.empty(),
            "missing Sight resources retain reference routing without fireball or bitmap replacement");
        effect.ClearPrims();g_world.UpdateGameObjects();
        check(g_resMgr.spr.m_lockCnt==0 && g_resMgr.act.m_lockCnt==0,"Sight releases all resource locks through world cleanup");
        g_renderer=savedRenderer;
    }
    {
        CRenderObject actor;actor.m_pos={5,30,100};
        CRagEffect effect;effect.m_master=&actor;effect.m_type=601;effect.m_duration=20000;
        effect.m_stateCnt=0;effect.SpawnSight2();auto* sight=effect.m_primList.front();
        check(effect.m_primList.size()==1 && sight->m_type==PP_3DPARTICLE && sight->m_alpha==50 && sight->m_alphaSpeed==-3,
            "Sight2 emits one fifty-alpha ordinary particle and no Shadow");
        check(sight->UsesReferenceParticle() && sight->m_deltaPosAccel.y==0.1f && sight->m_animSpeed==3 && sight->m_repeatAnim,
            "Sight2 shares original Sight motion storage and per-clip binding");
        sight->OnProcess();check(sight->m_alpha==47 && sight->m_pos.y==10,"Sight2 first update has no synthesized falling motion");
        effect.m_stateCnt=1;effect.SpawnSight2();
        check(effect.m_primList.size()==1 && effect.m_duration==20000,"Sight2 does not terminate or shorten duration for an absent actor effect-state flag");
        effect.m_stateCnt=9998;effect.SpawnSight2();
        check(effect.m_stateCnt==9998 && effect.m_primList.size()==2,"Sight2 preserves original frame counter without invented wrap at 9998");
        effect.m_stateCnt=9999;effect.SpawnSight2();
        check(effect.m_stateCnt==9999 && effect.m_primList.size()==2,"Sight2 odd-frame gate remains valid after the former wrap boundary");
        effect.ClearPrims();g_world.UpdateGameObjects();
        check(g_resMgr.spr.m_lockCnt==0 && g_resMgr.act.m_lockCnt==0,"Sight2 releases its looping resource locks");
    }
    {
        CRenderObject actor;actor.m_pos={90,80,70};
        CRagEffect effect;effect.m_master=&actor;effect.m_type=321;effect.m_duration=99999999;effect.m_effectPos={10,50,30};
        rngValues={199,90,0,10,9};rngCursor=0;g_resMgr.requests.clear();
        const int draws=rngDraws,beforeWaves=waves;
        effect.ApplyWarpZone2("effect\\ring_blue.tga");
        check(effect.m_primList.size()==2 && rngDraws-draws==5 && rngCursor==5 && waves==beforeWaves,
            "WarpZone2 zero emits circle then orbit with all five original RNG draws and no wave");
        auto it=effect.m_primList.begin();auto* circle=*it++;auto* orbit=*it;
        check(circle->m_type==PP_3DCIRCLE && circle->UsesReferenceCircle() && orbit->m_type==PP_3DPARTICLEORBIT && orbit->UsesReferenceParticle(),
            "WarpZone2 circle and orbit select canonical reference state/render paths");
        check(effect.m_effectPos.x==10 && circle->m_pos.y==50 && circle->m_segments[0].pos.x==10,
            "WarpZone2 frame zero retains stored parent origin rather than refreshing actor position");
        check(circle->m_duration==99999999 && circle->m_pattern==1 && circle->m_radius==15 && circle->m_deltaPos2.y==-1.5f
            && circle->m_latitude==90 && circle->m_fadeOutCnt==99999989 && circle->m_numSegments==1,
            "WarpZone2 initializes original long-lived circle and offset profile");
        check(circle->m_alpha==0 && circle->m_maxAlpha==128 && std::fabs(circle->m_alphaSpeed-12.8f)<0.000001f
            && std::string(g_texMgr.lastPath)=="effect\\alpha_down.tga","WarpZone2 circle retains exact alpha/texture profile without alternate paths");
        check(orbit->m_radius==5.25f && orbit->m_size==0.69999999f && orbit->m_longitude==90 && orbit->m_longSpeed==1.5f,
            "WarpZone2 preserves integer-division radius/size and positive angular speed branch");
        check(orbit->m_duration==70 && orbit->m_fadeOutCnt==47 && orbit->m_alpha==254 && orbit->m_maxAlpha==254
            && orbit->m_animSpeed==4 && orbit->m_repeatAnim && orbit->m_numSegments==1,
            "WarpZone2 restores original orbit constructor alpha, animation and segment defaults");
        check(std::fabs(orbit->m_radiusSpeed-0.00050000002f)<0.000000001f && orbit->m_gravSpeed==-0.30000001f
            && std::fabs(orbit->m_gravAccel-static_cast<float>(-0.30000001f/70.0*-0.66666669))<0.00000001f,
            "WarpZone2 orbit preserves radial drift and original gravity arithmetic");
        check(g_resMgr.requests==std::vector<std::string>({"\xC0\xCC\xC6\xD1\xC6\xAE\\particle1.spr","\xC0\xCC\xC6\xD1\xC6\xAE\\particle1.act"}),
            "WarpZone2 requests exact particle1 sprite then action");
        check(std::fabs(orbit->m_deltaPos2.x-5.25f)<0.000001f && std::fabs(orbit->m_deltaPos2.z)<0.000001f
            && orbit->m_segments[0].argb==0xFEFFFFFF,"WarpZone2 stores initial orbit offset and reference segment color");
        circle->OnProcess();
        check(circle->m_pos.y==48.5f && circle->m_size==15 && std::fabs(circle->m_alpha-12.8f)<0.000001f,
            "WarpZone2 circle uses reference parent-height state before outer counter increment");
        const float oldRadius=orbit->m_radius,oldGrav=orbit->m_gravSpeed,accel=orbit->m_gravAccel;
        orbit->OnProcess();
        const float radius=oldRadius+0.00050000002f;
        check(orbit->m_stateCnt==1 && orbit->m_curMotion==1 && orbit->m_longitude==91.5f && std::fabs(orbit->m_radius-radius)<0.000001f,
            "WarpZone2 orbit updates radius/angular speed before its zero-frame animation advance");
        check(std::fabs(orbit->m_deltaPos.x-radius*std::sin(GetRadian(91)))<0.000001f
            && std::fabs(orbit->m_deltaPos.z-radius*std::cos(GetRadian(91)))<0.000001f,
            "WarpZone2 orbit truncates updated longitude before degree-table lookup");
        check(std::fabs(orbit->m_pos.x-(10+5.25f+orbit->m_deltaPos.x))<0.00001f
            && std::fabs(orbit->m_pos.y-(50+oldGrav+accel))<0.00001f && orbit->m_alpha==254,
            "WarpZone2 keeps both initial offset and current radial displacement with integrated vertical gravity");
        orbit->m_stateCnt=47;orbit->m_alpha=230;orbit->OnProcess();
        check(orbit->m_alphaSpeed==-10 && orbit->m_alpha==220,"WarpZone2 orbit derives fade at frame forty-seven over remaining twenty-three frames");
        effect.m_stateCnt=1;effect.ApplyWarpZone2("effect\\ring_blue.tga");auto* firstCast=effect.m_primList.back();
        check(effect.m_effectPos.x==90 && firstCast->m_type==PP_3DCASTING_4 && firstCast->m_size==11
            && firstCast->m_renderFlag==5 && firstCast->m_pos.y==80,"WarpZone2 frame one copies actor and launches original first casting profile");
        check(firstCast->m_gi[0].distance==2.5f && firstCast->m_gi[0].rotStart==270 && firstCast->m_gi[0].riseAngle==53
            && firstCast->m_gi[3].distance==10 && firstCast->m_gi[3].riseAngle==50,
            "WarpZone2 first casting retains ordered band constants");
        actor.m_pos={100,90,80};effect.m_stateCnt=2;effect.ApplyWarpZone2("effect\\ring_blue.tga");auto* secondCast=effect.m_primList.back();
        check(effect.m_effectPos.y==90 && secondCast->m_size==4 && secondCast->m_gi[0].rotStart==271
            && secondCast->m_gi[1].distance==5.1999998f && secondCast->m_gi[3].riseAngle==49,
            "WarpZone2 frame two copies actor and preserves second casting band profile");
        actor.m_pos={999,999,999};effect.m_stateCnt=3;const auto count=effect.m_primList.size();const int before=rngDraws;
        effect.ApplyWarpZone2("effect\\ring_blue.tga");
        check(effect.m_effectPos.x==100 && effect.m_primList.size()==count && rngDraws==before,
            "WarpZone2 later non-emission frames neither refresh parent origin nor consume RNG");
        effect.m_stateCnt=10;rngValues={99,0,1,0,0};rngCursor=0;effect.ApplyWarpZone2("effect\\ring_blue.tga");auto* secondOrbit=effect.m_primList.back();
        check(secondOrbit->m_radius==4.5f && secondOrbit->m_longSpeed==-2.5f && secondOrbit->m_size==0.69999999f && rngCursor==5,
            "WarpZone2 negative branch retains lower radius boundary and still consumes discarded size draw");
        secondOrbit->m_latitude=0.25f;secondOrbit->m_latiSpeed=0.5f;secondOrbit->m_longSpeed=0;secondOrbit->OnProcess();
        check(secondOrbit->m_latitude==0.75f && std::fabs(secondOrbit->m_matrix.m[0][0]-std::cos(0.75f))<0.000001f,
            "reference orbit applies updated latitude to Z rotation directly as radians");
        effect.ClearPrims();g_world.UpdateGameObjects();
        effect.m_stateCnt=99999998;rngValues.clear();rngCursor=0;effect.ApplyWarpZone2("effect\\ring_blue.tga");
        check(effect.m_stateCnt==0 && effect.m_primList.size()==2,"WarpZone2 preserves original parent reset and zero-frame re-emission");
        effect.ClearPrims();g_world.UpdateGameObjects();
        g_resMgr.fail=true;effect.ApplyWarpZone2("effect\\ring_blue.tga");g_resMgr.fail=false;orbit=effect.m_primList.back();
        check(orbit->UsesReferenceParticle() && !orbit->m_particleSprRes && orbit->m_texture.empty(),
            "missing WarpZone2 orbit assets retain reference routing without soft-disc replacement");
        effect.ClearPrims();g_world.UpdateGameObjects();
        check(g_resMgr.spr.m_lockCnt==0 && g_resMgr.act.m_lockCnt==0,"WarpZone2 releases all orbit resource locks through world cleanup");
    }
    {
        CRenderObject actor,listener;actor.m_pos={900,900,900};listener.m_pos={2,500,4};
        auto* oldPlayer=g_world.m_player;g_world.m_player=&listener;
        CRagEffect effect;effect.m_type=35;effect.m_master=&actor;effect.m_duration=200;effect.m_effectPos={10,50,30};
        waveCalls.clear();const int draws=rngDraws;
        effect.ApplyReadyPortal();auto* ring=effect.m_primList.front();
        check(effect.m_primList.size()==1 && ring->m_type==PP_3DCIRCLE && ring->UsesReferenceCircle(),
            "ReadyPortal thirty-five emits one canonical reference circle on zero");
        check(waveCalls.size()==1 && waveCalls[0].name=="effect\\EF_ReadyPortal.wav" && waveCalls[0].x==8 && waveCalls[0].y==0
            && waveCalls[0].z==26 && rngDraws==draws,"ReadyPortal uses stored horizontal origin for wave with no RNG");
        check(effect.m_effectPos.x==10 && ring->m_segments[0].pos.y==50,
            "ReadyPortal keeps original stored effect position rather than copying a moving actor");
        check(ring->m_duration==30 && ring->m_deltaPos2.y==-1 && ring->m_innerSize==8 && ring->m_latitude==90
            && ring->m_radiusSpeed==1 && std::fabs(ring->m_radiusAccel+1.0f/60)<0.00000001f && ring->m_fadeOutCnt==15,
            "ReadyPortal circle matches original duration, expansion, orientation and fade profile");
        check(ring->m_pattern==0 && ring->m_radius==0 && ring->m_alpha==254 && ring->m_maxAlpha==254
            && ring->m_numSegments==1 && ring->m_renderFlag==1,"ReadyPortal retains source constructor flags, radius, alpha and segment defaults");
        check(ring->m_segments[0].argb==0xFEFFFFFF && std::fabs(ring->m_segments[0].transform.m[1][2]-1)<0.000001f
            && std::string(g_texMgr.lastPath)=="effect\\ring_blue.tga","ReadyPortal initializes rotated segment before exact ring-blue texture request");
        ring->OnProcess();
        check(ring->m_pos.y==49 && std::fabs(ring->m_radius-(1.0f-1.0f/60))<0.000001f && ring->m_size==ring->m_radius
            && ring->m_alpha==254,"ReadyPortal first frame follows stored parent height and fills initial disk from expanding radius");
        for(int i=0;i<10;++i)ring->OnProcess();
        check(ring->m_radius>8 && ring->m_size==8,"ReadyPortal uses inner-size width once radius exceeds it without invented fill pattern");
        ring->m_stateCnt=15;ring->OnProcess();
        check(std::fabs(ring->m_alphaSpeed+254.0f/15)<0.00001f && ring->m_alpha<254,
            "ReadyPortal begins source current-alpha fade exactly at frame fifteen");
        effect.m_stateCnt=13;effect.ApplyReadyPortal();
        check(effect.m_primList.size()==1 && waveCalls.size()==1,"ReadyPortal does not emit between fourteen-frame boundaries");
        effect.m_stateCnt=14;effect.ApplyReadyPortal();
        check(effect.m_primList.size()==2 && waveCalls.size()==2,"ReadyPortal repeats both circle and sound on frame fourteen");
        effect.m_stateCnt=28;effect.ApplyReadyPortal();
        check(effect.m_primList.size()==3 && waveCalls.size()==3 && rngDraws==draws,"ReadyPortal preserves recurring timing without consuming RNG");
        effect.ClearPrims();g_world.UpdateGameObjects();g_world.m_player=oldPlayer;
    }
    {
        CRenderObject actor;actor.m_pos={900,800,700};
        CRagEffect effect;effect.m_type=36;effect.m_master=&actor;effect.m_duration=120;effect.m_effectPos={10,50,30};
        const int draws=rngDraws;waveCalls.clear();
        effect.ApplyPortal();auto it=effect.m_primList.begin();auto* first=*it++;auto* second=*it++;auto* ring=*it;
        check(effect.m_primList.size()==3 && first->m_type==PP_3DCYLINDER && second->m_type==PP_3DCYLINDER && ring->m_type==PP_3DCIRCLE,
            "original Portal emits two cylinders then one circle at zero");
        check(first->m_duration==120 && first->m_arcAngle==36 && first->m_outerSize==3.5f && first->m_innerSize==3.5f
            && first->m_longSpeed==4 && first->m_heightSize==40,"Portal first cylinder preserves source dimensions and rotation");
        check(second->m_duration==120 && second->m_arcAngle==36 && second->m_outerSize==4 && second->m_innerSize==4
            && second->m_longSpeed==5 && second->m_heightSize==50,"Portal second cylinder preserves distinct dimensions and rotation");
        check(first->m_alpha==0 && first->m_maxAlpha==128 && first->m_alphaSpeed==static_cast<float>(128*0.16666667)
            && first->m_fadeOutCnt==114 && second->m_alpha==0 && second->m_maxAlpha==128
            && second->m_alphaSpeed==first->m_alphaSpeed && second->m_fadeOutCnt==114,"Portal cylinders preserve source rise and fade profile");
        check(first->m_renderFlag==1 && second->m_renderFlag==1 && first->m_pattern==0 && second->m_pattern==0
            && first->m_texture.size()==1 && second->m_texture.size()==1 && ring->m_texture.size()==1
            && std::string(g_texMgr.lastPath)=="effect\\ring_blue.tga","Portal uses original flags and exact ring-blue resource");
        check(effect.m_effectPos.x==10 && first->m_pos.y==50 && second->m_pos.y==50 && ring->m_segments[0].pos.y==50,
            "Portal preserves stored origin instead of following live actor");
        check(ring->UsesReferenceCircle() && ring->m_duration==30 && ring->m_deltaPos2.y==-1 && ring->m_innerSize==8
            && ring->m_radiusSpeed==1 && ring->m_fadeOutCnt==15 && ring->m_pattern==0 && ring->m_alpha==254
            && ring->m_numSegments==1,"Portal recurring circle follows original profile and reference dispatch");
        ring->OnProcess();check(ring->m_pos.y==49 && ring->m_radius>0 && ring->m_size==ring->m_radius
            && ring->m_segments[0].argb==0xFEFFFFFF,"Portal circle advances reference state and segment");
        effect.m_stateCnt=1;effect.ApplyPortal();check(effect.m_primList.size()==3,"Portal emits nothing on intervening frame");
        effect.m_stateCnt=14;effect.ApplyPortal();check(effect.m_primList.size()==4 && effect.m_primList.back()->m_type==PP_3DCIRCLE,
            "Portal repeats only circle at fourteen without cylinders");
        effect.m_stateCnt=28;effect.ApplyPortal();check(effect.m_primList.size()==5 && waveCalls.empty() && rngDraws==draws,
            "Portal preserves recurring cadence with no wave or RNG");
        effect.ClearPrims();g_world.UpdateGameObjects();
    }
    {
        CRagEffect effect;effect.m_type=36;effect.m_duration=120;effect.m_effectPos={10,50,30};effect.ApplyPortal();
        auto* prim=effect.m_primList.front();prim->m_animSpeed=4;prim->m_repeatAnim=true;prim->m_alphaDelta=1;
        check(prim->UsesReferenceCylinder() && prim->m_maxHeightSize==100,"original Portal cylinder binds reference state and source maximum height");
        prim->OnProcess();
        check(prim->m_pos.x==10 && prim->m_pos.y==50 && prim->m_pos.z==30 && prim->m_longitude==4,
            "reference cylinder uses stored parent height and increments longitude after matrix");
        check(std::fabs(prim->m_matrix.m[0][0]-1)<0.000001f && prim->m_curMotion==0 && prim->m_stateCnt==1,
            "reference cylinder first frame uses old longitude then wraps one-texture animation");
        check(prim->m_alpha==static_cast<float>(128*0.16666667),"reference cylinder applies original alpha rise on zero");
        for(int i=0;i<6;++i)prim->OnProcess();
        check(prim->m_alpha==128,"reference cylinder clamps alpha rise to original maximum");
        check(prim->m_argb==0x80FFFFFF,"reference alpha update stores packed cylinder color for rendering");
        effect.m_effectPos={20,70,40};prim->OnProcess();
        check(prim->m_pos.x==20 && prim->m_pos.y==70 && prim->m_pos.z==40,"reference cylinder follows persistent effect origin without terrain substitution");
        prim->m_stateCnt=114;prim->OnProcess();
        check(std::fabs(prim->m_alphaSpeed+128.0f/6)<0.00001f && prim->m_alpha<128,"reference cylinder fade uses current alpha at original fade frame");
        prim->m_stateCnt=8;prim->m_latitude=90.9f;prim->m_longitude=0.9f;prim->m_speed=1;prim->m_accel=1;
        prim->m_deltaPos={};prim->m_deltaPos2={1,2,3};prim->m_heightSize=99;prim->m_heightSpeed=2;prim->m_heightAccel=1;
        prim->m_outerSize=4;prim->m_outerSpeed=1;prim->m_outerAccel=2;prim->m_innerSize=4;prim->m_innerSpeed=-2;prim->m_innerAccel=-1;
        prim->OnProcess();
        check(prim->m_speed==2 && std::fabs(prim->m_deltaPos.z+2)<0.000001f && std::fabs(prim->m_deltaPos.y)<0.000001f,
            "reference cylinder integrates acceleration before negative local-Y motion with integer-degree rotation");
        check(prim->m_orgPos.x==21 && prim->m_orgPos.y==72 && prim->m_orgPos.z==43 && prim->m_heightSize==100
            && prim->m_heightSpeed==3 && prim->m_outerSize==7 && prim->m_innerSize==1,"reference cylinder advances sizes and upper-clamps height only");
        prim->m_pattern=1;prim->m_stateCnt=10;prim->m_duration=20;prim->m_fadeOutCnt=10;prim->m_outerSpeed=0;prim->m_outerAccel=0;prim->m_outerSize=22;
        prim->OnProcess();check(prim->m_outerSpeed==-2,"reference cylinder shrink starts after size update with remaining-plus-one divisor");
        prim->m_repeatAnim=false;prim->m_totalTexture=3;prim->m_animSpeed=4;prim->m_curMotion=1;prim->m_stateCnt=1;
        check(UpdateReferenceCylinderState(prim) && prim->m_curMotion==1,"reference cylinder animation preserves non-divisible frame");
        prim->m_stateCnt=4;check(UpdateReferenceCylinderState(prim) && prim->m_curMotion==2,"reference cylinder advances texture on divisible frame");
        check(!UpdateReferenceCylinderState(prim) && prim->m_curMotion==3,"reference cylinder nonrepeating animation terminates at equality");
        check(UpdateReferenceCylinderState(prim) && prim->m_curMotion==4,"reference cylinder retains original equality-only termination");
        effect.ClearPrims();g_world.UpdateGameObjects();
    }
    {
        CRenderObject actor;actor.m_pos={900,800,700};
        CRagEffect effect;effect.m_type=61;effect.m_master=&actor;effect.m_duration=1000;effect.m_effectPos={10,50,30};
        rngValues={199,91,321,9};rngCursor=0;const int draws=rngDraws;waveCalls.clear();g_resMgr.requests.clear();
        effect.ApplyWarpZone();auto it=effect.m_primList.begin();auto* disk=*it++;auto* ring=*it++;auto* orbit=*it;
        check(effect.m_primList.size()==3 && disk->m_type==PP_3DCIRCLE && ring->m_type==PP_3DCIRCLE && orbit->m_type==PP_3DPARTICLEORBIT,
            "original WarpZone emits disk then shrinking ring then orbit at zero");
        check(disk->UsesReferenceCircle() && ring->UsesReferenceCircle() && orbit->UsesReferenceParticle(),"original WarpZone binds reference circle and orbit paths");
        check(disk->m_duration==158 && disk->m_pattern==1 && disk->m_radius==15 && disk->m_deltaPos2.y==-1 && disk->m_latitude==90
            && disk->m_alpha==0 && disk->m_maxAlpha==128 && disk->m_alphaSpeed==static_cast<float>(128*0.1) && disk->m_fadeOutCnt==148,
            "WarpZone disk preserves source lifetime height radius and alpha profile");
        check(disk->m_numSegments==1 && disk->m_segments[0].argb==0x00FFFFFF && disk->m_segments[0].transform.m[1][1]==1,
            "WarpZone disk initializes segment before its first rotation update");
        check(ring->m_duration==80 && ring->m_innerSize==4 && ring->m_radius==14 && ring->m_radiusSpeed==-0.15000001f
            && ring->m_deltaPos2.y==-1 && ring->m_alpha==5 && ring->m_maxAlpha==200 && ring->m_alphaSpeed==10 && ring->m_fadeOutCnt==64,
            "WarpZone ring preserves source shrinking and alpha profile");
        check(ring->m_chPoint==std::vector<float>{20} && ring->m_chVal1==std::vector<float>{0} && !ring->m_chSpeed,
            "WarpZone stores original alpha-key arrays without enabling their control flag");
        check(ring->m_segments[0].argb==0x05FFFFFF && std::fabs(ring->m_segments[0].transform.m[1][2]-1)<0.000001f,
            "WarpZone ring initial segment retains rotated matrix and alpha five");
        check(rngDraws==draws+4 && rngCursor==4 && orbit->m_radius==5.75f && orbit->m_longitude==91 && orbit->m_longSpeed==2 && orbit->m_size==0.69999999f,
            "WarpZone consumes four RNG draws including discarded angular draw and integer-divided size draw");
        check(orbit->m_duration==130 && orbit->m_radiusSpeed==0.00050000002f && orbit->m_gravSpeed==-0.2f
            && std::fabs(orbit->m_gravAccel-static_cast<float>(-0.2f/130.0*-0.66666669))<0.000000001f && orbit->m_fadeOutCnt==87,
            "WarpZone orbit preserves source duration expansion and gravity profile");
        check(orbit->m_alpha==254 && orbit->m_maxAlpha==254 && orbit->m_renderFlag==1 && orbit->m_animSpeed==4 && orbit->m_repeatAnim && orbit->m_numSegments==1,
            "WarpZone orbit preserves constructor alpha render flag and looping sprite profile");
        check(g_resMgr.requests.size()==2 && g_resMgr.requests[0]=="\xC0\xCC\xC6\xD1\xC6\xAE\\particle1.spr"
            && g_resMgr.requests[1]=="\xC0\xCC\xC6\xD1\xC6\xAE\\particle1.act","WarpZone uses exact sprite then action resources");
        check(effect.m_effectPos.x==10 && effect.m_effectPos.y==50 && waveCalls.empty(),"WarpZone preserves stored origin and has no sound");
        disk->OnProcess();ring->OnProcess();orbit->OnProcess();
        check(disk->m_pos.y==49 && disk->m_size==15 && ring->m_pos.y==49 && std::fabs(ring->m_radius-13.85f)<0.000001f && ring->m_size==4,
            "WarpZone first circle update preserves filled disk versus ring width");
        check(orbit->m_stateCnt==1 && orbit->m_longitude==93 && orbit->m_curMotion==1 && orbit->m_segments[0].pos.y<50,
            "WarpZone orbit follows reference integration segment and animation order");
        ring->m_stateCnt=20;ring->m_alpha=100;ring->OnProcess();
        check(ring->m_alphaSpeed==10 && ring->m_alpha==110,"WarpZone inactive frame-twenty alpha key leaves rise unchanged");
        effect.ClearPrims();g_world.UpdateGameObjects();
        effect.m_stateCnt=28;effect.ApplyWarpZone();check(effect.m_primList.size()==1 && effect.m_primList.front()->m_type==PP_3DCIRCLE,
            "WarpZone emits only shrinking ring at twenty-eight");effect.ClearPrims();g_world.UpdateGameObjects();
        effect.m_stateCnt=112;effect.ApplyWarpZone();check(effect.m_primList.size()==1,"WarpZone emits last shrinking ring at one-twelve");
        effect.ClearPrims();g_world.UpdateGameObjects();
        effect.m_stateCnt=140;effect.ApplyWarpZone();check(effect.m_primList.size()==1 && effect.m_primList.front()->m_type==PP_3DPARTICLEORBIT,
            "WarpZone stops shrinking rings after source threshold while orbit cadence continues");effect.ClearPrims();g_world.UpdateGameObjects();
        effect.m_stateCnt=145;effect.ApplyWarpZone();check(effect.m_stateCnt==0 && effect.m_primList.size()==3,
            "WarpZone resets exactly at one-forty-five before zero-frame emission");effect.ClearPrims();g_world.UpdateGameObjects();
        effect.m_stateCnt=146;effect.ApplyWarpZone();check(effect.m_stateCnt==146 && effect.m_primList.empty(),"WarpZone does not replace equality reset with greater-than clamp");
        check(g_resMgr.spr.m_lockCnt==0 && g_resMgr.act.m_lockCnt==0,"WarpZone releases sprite and action locks through world cleanup");
        rngValues.clear();rngCursor=0;
    }
    {
        CRenderObject actor;actor.m_pos={90,80,70};
        CRagEffect effect;effect.m_master=&actor;effect.m_type=221;effect.m_duration=200;effect.m_effectPos={10,50,30};
        waveCalls.clear();const int draws=rngDraws;effect.ApplyPotionPillar(1,50);auto* prim=effect.m_primList.front();
        check(prim->UsesReferenceCylinder() && prim->m_type==PP_3DCYLINDER,"PotionPillar uses reference cylinder state and renderer dispatch");
        check(prim->m_pos.x==10 && prim->m_pos.y==50 && effect.m_effectPos.y==50,"PotionPillar helper preserves stored origin without live actor substitution");
        check(prim->m_duration==50 && prim->m_outerSize==4.5f && prim->m_innerSize==4.5f && prim->m_heightSize==10
            && prim->m_heightSpeed==1 && prim->m_heightAccel==0.0099999998f,"PotionPillar preserves original dimensions and height acceleration");
        check(prim->m_alpha==0 && prim->m_maxAlpha==90 && prim->m_alphaSpeed==static_cast<float>(90*0.050000001)
            && prim->m_fadeOutCnt==40 && std::string(g_texMgr.lastPath)=="effect\\alpha_down.tga","PotionPillar preserves exact alpha arithmetic fade and texture");
        prim->m_animSpeed=4;prim->m_repeatAnim=true;prim->m_alphaDelta=1;prim->OnProcess();
        check(std::fabs(prim->m_heightSize-11.01f)<0.000001f && prim->m_pos.y==50 && prim->m_argb==0x04FFFFFF,
            "PotionPillar integrates height acceleration before rendering stored packed alpha");
        prim->m_stateCnt=40;prim->m_alpha=90;prim->OnProcess();
        check(prim->m_alpha==81 && prim->m_alphaSpeed==-9,"PotionPillar original fade begins at duration minus ten");
        effect.m_stateCnt=1;effect.ApplyPotionPillar(1,50);check(effect.m_primList.size()==2 && waveCalls.empty() && rngDraws==draws,
            "PotionPillar helper emits on every call without audio or RNG");effect.ClearPrims();g_world.UpdateGameObjects();
        for(int type:{218,219,220,221}){effect.m_type=type;CEffectPrim probe;probe.m_master=&effect;probe.m_type=PP_3DCYLINDER;
            check(probe.UsesReferenceCylinder(),"all original potion cylinder callers use migrated cylinder path");probe.m_master=nullptr;}
        effect.m_type=218;effect.m_stateCnt=0;
        check(!effect.ProcessPotionConcentration(),"concentration propagates missing STR termination after emitter side effects");
        check(effect.m_effectPos.x==90 && effect.m_effectPos.y==80 && effect.m_primList.size()==1 && effect.m_primList.front()->m_heightSpeed==2
            && effect.m_primList.front()->m_duration==50,"concentration copies actor before zero-frame pillar");
        check(waveCalls.size()==1 && waveCalls[0].name=="effect\\ac_concentration.wav" && waveCalls[0].x==0 && waveCalls[0].y==0
            && waveCalls[0].z==0 && waveCalls[0].volume==250 && waveCalls[0].range==40,"concentration uses direct zero-position wave before STR");
        actor.m_pos={100,200,300};effect.m_stateCnt=1;effect.ProcessPotionConcentration();
        check(effect.m_effectPos.y==200 && effect.m_primList.size()==1 && waveCalls.size()==1,"concentration refreshes actor on non-emission frames without duplicate wave");
        effect.ClearPrims();g_world.UpdateGameObjects();waveCalls.clear();effect.m_type=219;effect.m_stateCnt=29;
        effect.ProcessPotionConcentration();check(effect.m_primList.empty() && waveCalls.empty(),"awakening does not emit before frame thirty");
        effect.m_stateCnt=30;effect.ProcessPotionConcentration();prim=effect.m_primList.front();
        check(effect.m_primList.size()==1 && prim->m_duration==30 && prim->m_heightSpeed==1.9f && prim->m_fadeOutCnt==20
            && waveCalls.size()==1,"awakening emits its distinct pillar and wave on frame thirty");
        effect.m_stateCnt=31;effect.ProcessPotionConcentration();check(effect.m_primList.size()==1 && waveCalls.size()==1,"awakening does not repeat frame-thirty emission");
        effect.ClearPrims();g_world.UpdateGameObjects();
        auto clip=std::make_unique<KANICLIP>();clip->cLayer=2;KAC_KEYFRAME keys[2];keys[0].iFrame=0;keys[1].iFrame=100;
        clip->aLayer[1].cAniKey=2;clip->aLayer[1].aAniKey=keys;effect.m_ezEffectRes=clip.get();effect.m_aniClips=clip.get();
        effect.InitEZ2STRFrame();effect.m_stateCnt=1;
        check(effect.ProcessPotionConcentration() && effect.m_strScreenOffset.x==0 && effect.m_strScreenOffset.y==-80 && effect.m_strScreenOffset.z==0,
            "potion caller processes original STR offset and propagates its active result");
    }
    {
        CRenderObject actor;actor.m_pos={90,80,70};actor.m_u28.m_sprColor.u.s={123,1,2,3};
        CRagEffect effect;effect.m_master=&actor;effect.m_type=220;effect.m_duration=200;effect.m_effectPos={10,50,30};
        waveCalls.clear();const int draws=rngDraws;const int messages=gameMode.messages;
        check(effect.ApplyPotionBerserk(),"Berserk ignores missing STR completion on zero");
        check(actor.m_isSprArgbFixed==1 && actor.alpha==255 && actor.red==250 && actor.green==120 && actor.blue==120,
            "Berserk fixes actor alpha and tint to original message-one-twenty-eight values");
        check(actor.m_oldColor.a==123 && actor.m_oldColor.r==1 && actor.m_oldColor.g==2 && actor.m_oldColor.b==3
            && actor.m_curColor.a==255,"Berserk preserves pre-tint color and stores original current alpha");
        check(effect.m_primList.size()==1 && effect.m_primList.front()->m_type==PP_3DCYLINDER && effect.m_primList.front()->m_pos.y==50
            && effect.m_effectPos.y==80,"Berserk launches stored-origin pillar before copying parent position");
        check(waveCalls.empty() && rngDraws==draws && gameMode.messages==messages,"Berserk zero-frame pillar adds no audio mode message or RNG");
        effect.ClearPrims();g_world.UpdateGameObjects();
        effect.m_stateCnt=12;effect.ApplyPotionBerserk();check(effect.m_primList.empty(),"Berserk particle window excludes frame twelve");
        rngValues={91,6,49,59};rngCursor=0;effect.m_stateCnt=16;effect.ApplyPotionBerserk();auto* prim=effect.m_primList.front();
        check(prim->m_type==PP_3DCROSSTEXTURE && prim->m_duration==40 && prim->m_gi[0].life==0,"Berserk emits original cross texture at frame sixteen");
        check(rngDraws==draws+4 && rngCursor==4 && prim->m_widthSize==8.9f && prim->m_size==1,
            "Berserk keeps four-draw angle radius speed width order and unnormalized width");
        check(std::fabs(prim->m_deltaPos2.x-8*GetSin(91))<0.000001f && std::fabs(prim->m_deltaPos2.z-8*GetCos(91))<0.000001f
            && prim->m_deltaPos2.y==0,"Berserk spawn offset uses original integer-degree Y matrix");
        check(std::fabs(prim->m_matrix.m[1][2]-1)<0.000001f && prim->m_speed==static_cast<float>(69*0.016666668)
            && prim->m_heightSize==0.2f,"Berserk replaces spawn matrix with X ninety and preserves double-product speed");
        check(prim->m_alpha==0 && prim->m_maxAlpha==180 && prim->m_alphaSpeed==static_cast<float>(180*0.050000001)
            && prim->m_fadeOutCnt==20 && prim->m_tintColor==RGB(240,0,0) && std::string(g_texMgr.lastPath)=="effect\\ac_center2.tga",
            "Berserk retains exact particle alpha tint fade and texture");
        prim->m_alphaDelta=1;prim->OnProcess();
        check(prim->UsesReferenceCrossTexture() && prim->m_stateCnt==1 && prim->m_curMotion==0 && prim->m_argb==0x09F00000,
            "Berserk cross texture updates source alpha packed color and one-texture animation on zero");
        check(std::fabs(prim->m_pos.y-(80-prim->m_speed))<0.00001f && prim->m_widthSize==8.9f && prim->m_heightSize==0.2f,
            "Berserk cross texture follows stored origin and launch-matrix motion without synthetic scaling");
        actor.m_pos.y=200;effect.m_effectPos=actor.m_pos;prim->OnProcess();
        check(std::fabs(prim->m_pos.y-(200-2*prim->m_speed))<0.00001f,"Berserk cross texture follows updated parent with accumulated displacement");
        prim->m_stateCnt=20;prim->m_alpha=180;prim->OnProcess();
        check(prim->m_alpha==171 && prim->m_alphaSpeed==-9 && prim->m_argb==0xABF00000,"Berserk cross texture retains exact frame-twenty fade");
        effect.ClearPrims();g_world.UpdateGameObjects();rngValues.clear();rngCursor=0;
        effect.m_stateCnt=18;auto* oldPlayer=g_world.m_player;g_world.m_player=nullptr;
        effect.ApplyPotionBerserk();g_world.m_player=oldPlayer;
        check(waveCalls.size()==1 && waveCalls[0].name=="effect\\ac_concentration.wav" && waveCalls[0].x==0 && waveCalls[0].y==0 && waveCalls[0].z==0,
            "Berserk frame-eighteen wave uses direct zero coordinates without player dependency");
        check(gameMode.messages==messages+1 && gameMode.lastMsg==135 && gameMode.lastArg==1,"Berserk sends original mode message at frame eighteen");
        effect.m_stateCnt=108;effect.ApplyPotionBerserk();check(effect.m_primList.size()==1,"Berserk emits last divisible particle frame below one-ten");
        effect.ClearPrims();g_world.UpdateGameObjects();effect.m_stateCnt=112;effect.ApplyPotionBerserk();
        check(effect.m_primList.empty(),"Berserk particle window stops after one-ten");
        effect.m_stateCnt=150;check(effect.ApplyPotionBerserk() && actor.m_isSprArgbFixed==1,"Berserk keeps tint and effect alive through frame one-fifty");
        effect.m_stateCnt=151;check(!effect.ApplyPotionBerserk() && actor.m_isSprArgbFixed==0 && actor.m_oldColor.a==0 && actor.m_oldColor.r==0,
            "Berserk terminates after one-fifty and clears fixed-tint bookkeeping");
        check(actor.red==250 && actor.green==120 && actor.blue==120,"Berserk clear preserves current sprite channels like source message one-twenty-nine");
        auto clip=std::make_unique<KANICLIP>();clip->cLayer=2;KAC_KEYFRAME keys[2];keys[0].iFrame=0;keys[1].iFrame=100;
        clip->aLayer[1].cAniKey=2;clip->aLayer[1].aAniKey=keys;effect.m_ezEffectRes=clip.get();effect.m_aniClips=clip.get();
        effect.InitEZ2STRFrame();effect.m_stateCnt=1;
        check(effect.ApplyPotionBerserk() && effect.m_strScreenOffset.y==-80 && effect.m_strScreenOffset.x==0 && effect.m_strScreenOffset.z==0,
            "Berserk processes original STR offset on non-emission frame");
        actor.m_oldColor={55,66,77,88};effect.m_stateCnt=0;effect.ApplyPotionBerserk();
        check(actor.m_oldColor.a==55 && actor.m_oldColor.r==66,"Berserk does not overwrite already saved actor color");
        effect.ClearPrims();g_world.UpdateGameObjects();
    }
    {
        CRenderObject actor,listener;actor.m_pos={10,50,30};listener.m_pos={2,500,4};
        auto* oldPlayer=g_world.m_player;g_world.m_player=&listener;auto* oldAttr=g_world.m_attr;SilentAttr attr;g_world.m_attr=&attr;
        CRagEffect effect;effect.m_type=26;effect.m_master=&actor;effect.m_duration=200;effect.m_count=2;
        waveCalls.clear();const int draws=rngDraws;effect.m_stateCnt=11;effect.ApplyIceArrow();
        check(effect.m_primList.empty() && waveCalls.empty() && effect.m_count==2 && effect.m_effectPos.y==50,
            "IceArrow copies actor before frame twelve without emission count scaling or audio");
        rngValues={2,1,8,3,0};rngCursor=0;effect.m_stateCnt=12;effect.ApplyIceArrow();auto* arrow=effect.m_primList.front();
        check(effect.m_count==20 && effect.m_primList.size()==1 && waveCalls.size()==1 && waveCalls[0].name=="effect\\EF_IceArrow3.wav",
            "IceArrow selects runtime wave and scales count on frame twelve before projectile gate");
        check(waveCalls[0].x==8 && waveCalls[0].y==0 && waveCalls[0].z==26 && waveCalls[0].frameRng==draws+1,
            "IceArrow wave uses horizontal actor-listener delta before projectile RNG");
        check(rngDraws==draws+5 && rngCursor==5 && arrow->m_deltaPos2.x==33 && arrow->m_deltaPos2.y==-60 && arrow->m_deltaPos2.z==16,
            "IceArrow draws Z spawn before X and consumes wave plus four projectile draws");
        const float expectedLongitude=static_cast<float>(-EffectHeading(33,16,-2,1));
        const float expectedLatitude=static_cast<float>(EffectHeading(-60,static_cast<float>(std::sqrt(1345.0)),0,0));
        check(arrow->m_longitude==expectedLongitude && arrow->m_latitude==expectedLatitude && effect.m_longitude==expectedLongitude && effect.m_latitude==expectedLatitude,
            "IceArrow draws Z aim before X and stores source longitude and latitude on both primitive and effect");
        matrix expected;MatrixIdentity(expected);MatrixAppendXRotation(expected,GetRadian(static_cast<int>(expectedLatitude)));
        MatrixAppendYRotation(expected,GetRadian(static_cast<int>(expectedLongitude)));
        check(std::fabs(arrow->m_matrix.m[2][0]-expected.m[2][0])<0.000001f && std::fabs(arrow->m_matrix.m[2][1]-expected.m[2][1])<0.000001f,
            "IceArrow truncates both source angles before launch matrix construction");
        check(arrow->m_type==PP_3DCROSSTEXTURE && arrow->UsesReferenceCrossTexture() && arrow->m_duration==70 && arrow->m_pattern==20
            && arrow->m_speed==-2.75f && arrow->m_widthSize==11.5f && arrow->m_heightSize==3.8f && arrow->m_fadeOutCnt==50,
            "IceArrow preserves original cross-texture dimensions duration motion pattern and fade");
        check(arrow->m_alpha==254 && arrow->m_maxAlpha==254 && arrow->m_renderFlag==1 && arrow->m_animSpeed==4 && arrow->m_repeatAnim
            && arrow->m_gi[0].life==0 && std::string(g_texMgr.lastPath)=="effect\\icearrow.tga","IceArrow retains constructor profile and exact texture");
        arrow->m_alphaDelta=1;arrow->OnProcess();
        check(attr.calls==1 && attr.lastX==arrow->m_pos.x && attr.lastZ==arrow->m_pos.z && effect.m_flag==0 && arrow->m_pattern==20,
            "IceArrow queries ground after world position integration and retains in-flight control above terrain threshold");
        check(effect.m_deltaPos.x==arrow->m_deltaPos.x+33 && effect.m_deltaPos.y==arrow->m_deltaPos.y-60 && effect.m_deltaPos.z==arrow->m_deltaPos.z+16,
            "IceArrow propagates offset plus accumulated displacement to parent before collision");
        check(arrow->m_stateCnt==1 && arrow->m_curMotion==0 && arrow->m_argb==0xFEFFFFFF,"IceArrow frame-zero state keeps original alpha and wrapped texture motion");
        attr.height=arrow->m_pos.y;arrow->m_speed=0;arrow->OnProcess();
        check(arrow->m_pattern==20 && effect.m_flag==0,"IceArrow ground equality does not trigger strict less-than contact");
        attr.height=-1000;arrow->m_speed=-2.75f;arrow->OnProcess();
        check(arrow->m_speed==0 && arrow->m_pattern==16 && effect.m_flag==1 && arrow->m_stateCnt==46,
            "IceArrow contact clears only ground bit sets hit flag and replaces state with duration-minus-twenty-five before increment");
        const int queries=attr.calls;const vector3d stopped=arrow->m_deltaPos;arrow->OnProcess();
        check(attr.calls==queries && arrow->m_deltaPos.x==stopped.x && arrow->m_deltaPos.y==stopped.y && arrow->m_deltaPos.z==stopped.z,
            "IceArrow stops motion and further terrain queries after first hit");
        effect.m_stateCnt=13;effect.ApplyIceArrow();auto* ring=effect.m_primList.back();
        check(effect.m_primList.size()==2 && ring->m_type==PP_3DCIRCLE && ring->UsesReferenceCircle() && effect.m_flag==0,
            "IceArrow consumes hit flag and emits reference circle on next effect call independent of flight gate");
        check(ring->m_duration==30 && ring->m_latitude==90 && ring->m_radiusSpeed==1.2f
            && ring->m_radiusAccel==static_cast<float>(1.2f/70.0*-2.0) && ring->m_innerSize==10 && ring->m_alpha==0
            && ring->m_alphaSpeed==static_cast<float>(254*0.1) && ring->m_fadeOutCnt==20,
            "IceArrow impact circle preserves exact expansion and fade profile");
        check(ring->m_numSegments==1 && ring->m_segments[0].argb==0x00FFFFFF && ring->m_segments[0].transform.m[1][1]==1
            && std::string(g_texMgr.lastPath)=="effect\\ring_blue.tga","IceArrow impact initializes source identity segment before rotation update");
        ring->m_alphaDelta=1;ring->OnProcess();check(ring->m_pos.y==50 && ring->m_size==ring->m_radius && ring->m_alpha>0,
            "IceArrow impact circle uses reference state without terrain-height substitution");
        effect.ApplyIceArrow();check(effect.m_primList.size()==2,"IceArrow does not duplicate consumed impact flag");
        rngValues={4,7,2,1};rngCursor=0;actor.m_pos={20,60,40};effect.m_stateCnt=22;effect.ApplyIceArrow();
        check(effect.m_primList.size()==3 && waveCalls.size()==1 && effect.m_effectPos.y==60 && rngCursor==4,
            "IceArrow emits second projectile ten frames later without repeating audio or count scaling");
        effect.m_stateCnt=32;effect.ApplyIceArrow();check(effect.m_primList.size()==3,"IceArrow count bound excludes flight equal to scaled count");
        check(std::fabs(EffectHeading(0,0,1,0)-90.00001872397214)<0.000001,"four-coordinate heading preserves original double divisor constant");
        effect.ClearPrims();g_world.UpdateGameObjects();rngValues.clear();rngCursor=0;g_world.m_attr=oldAttr;g_world.m_player=oldPlayer;
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
