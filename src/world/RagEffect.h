#pragma once

#include "GameActor.h"
#include "res/EzEffectRes.h"
#include "res/WorldRes.h"

#include <array>
#include <list>
#include <string>
#include <vector>

class CTexture;
class CSprRes;
class CActRes;

// Remaining reconstructed selectors retain their existing values during the
// incremental migration. Full reference enum/dispatch reconciliation is pending.
enum EFFECTPRIMID {
    PP_3DCIRCLE = 0,
    PP_3DCYLINDER = 1,
    PP_3DPARTICLEORBIT = 2,
    PP_3DPARTICLE = 3,
    PP_3DPARTICLEGRAVITY = 4,
    PP_3DPARTICLESPLINE = 5,
    PP_3DTEXTURE = 6,
    PP_3DCROSSTEXTURE = 7,
    PP_3DSPHERE = 8,
    PP_3DQUADHORN = 9,
    PP_2DCIRCLE = 10,
    PP_2DFLASH = 11,
    PP_2DTEXTURE = 12,
    PP_MAPPILLAR = 13,
    PP_MAPMAGICZONE = 14,
    PP_MAPPARTICLE = 15,
    PP_SUPERANGEL = 0x55,
    PP_SLASH1 = 0x4B,
    PP_HEAL = 22,
    PP_PORTAL = 23,
    PP_WIND = 24,
    PP_WATERFALL = 25,
    PP_CLOUD = 26,
};

struct EffectBandState {
    bool active = false;
    int process = 0;
    float maxHeight = 0.0f;
    float rotStart = 0.0f;
    float alpha = 0.0f;
    float distance = 0.0f;
    float riseAngle = 0.0f;
    float fadeThreshold = -1.0f;
    float radius = 0.0f;
    std::array<float, 21> heights{};
    std::array<u8, 21> modes{};
};

constexpr int kPrimTypeParticleUp = 0x3B;
constexpr int kPrimTypeBottom = 0x2F;
constexpr int kPrimTypeBottomOut = 0x31;
constexpr int kPrimTypeHeal = 0x3C;
constexpr int kPrimTypeHealLow = 0x6E;
constexpr int kPrimTypeGiExplosion = 0x71;
constexpr int kPrimTypeAcidDemon = 0x76;
constexpr int kPrimTypeFlowerCast = 0x6C;
constexpr int kPrimTypeRectUp = 0x40;
constexpr int kPrimTypeBlind = 0x47;
constexpr int kPrimTypeCasting4 = 0x41;
constexpr int kPrimTypeCasting5 = 0x6B;
constexpr int kPrimTypeThrowItem = 0x35;
constexpr int kPrimTypeEffectTexture = 0x52;
constexpr int kPrimTypeChemical = 0x36;
constexpr int kPrimTypeChemical2 = 0x37;
constexpr int kPrimTypeChemical3 = 0x38;
constexpr int kPrimTypeTeleport2 = 0x39;
constexpr int kPrimTypeFirstAid = 0x3A;
constexpr int kPrimTypeWind = 0x3E;
constexpr int kPrimTypePortal = 0x3F;
constexpr int kPrimTypePoison = 0x48;
constexpr int kPrimTypeForest = 0x42;
constexpr int kPrimTypeSaintWing = 0x4C;
constexpr int kPrimTypeMagnum2 = 0x49;
constexpr int kPrimTypeSphereWind = 0x4D;
constexpr int kPrimTypeColorPaper = 0x4E;
constexpr int kPrimTypeGi1 = 0x1E;
constexpr int kPrimTypeGi2 = 0x1F;
constexpr int kPrimTypeGi4 = 0x24;
constexpr int kPrimTypeStormKick = 0x67;
constexpr int kPrimTypeHalfSphere = 0x68;
constexpr int kPrimTypeAttackEnergy = 0x69;
constexpr int kPrimTypeElectric = 0x65;
constexpr int kPrimTypeElectric2 = 0x66;
constexpr int kPrimTypeAttackEnergy2 = 0x6A;
constexpr int kPrimTypeGi5 = 0x32;
constexpr int kPrimTypeCasting = 0x17;
constexpr int kPrimTypeCasting2 = 0x18;
constexpr int kPrimTypeMapPillar = 0x29;
constexpr int kPrimTypeCasting3 = 0x19;

// One 180-byte m_GI slot from the 2008 CEffectPrim. Offsets were taken from the
// stores in CRagEffect::Heal at 0x6319B0. The spark quad uses distance as its
// size, height[2] as its vertical speed, and sparkX/Y/Z as its world position.
struct EffectGi {
    u8 life = 0;
    u8 unused0 = 0;
    short alphaB = 0;
    short alphaT = 0;
    short fullDisplayAngle = 0;
    float maxHeight = 0.0f;
    int process = 0;
    short rotStart = 0;
    short unused1 = 0;
    std::array<float, 21> height{};
    std::array<u8, 21> flag{};
    u8 unused2 = 0;
    short riseAngle = 0;
    float distance = 0.0f;
    float sparkX = 0.0f;
    float sparkY = 0.0f;
    float sparkZ = 0.0f;
    float vecTNowX = 0.0f;
    float vecTNowY = 0.0f;
    float vecTNowZ = 0.0f;
    float vecBPreX = 0.0f;
    float arcBaselineY = 0.0f;
    float vecBPreZ = 0.0f;
    float vecTPreX = 0.0f;
    float vecTPreY = 0.0f;
    float vecTPreZ = 0.0f;
};

struct EffectSegmentState {
    vector3d pos{};
    float radius = 0.0f;
    float size = 0.0f;
    float alpha = 0.0f;
    int process = 0;
    float displayAngle = 0.0f;
    float phaseDegrees = 0.0f;
    matrix transform{};
};

constexpr int kRagEffectPortal = 50001;
constexpr int kRagEffectReadyPortal = 50002;
constexpr int kRagEffectWarpZone = 50003;

class CRagEffect;

class CEffectPrim : public CGameObject {
public:
    CEffectPrim();
    ~CEffectPrim() override;

    void Init(CRagEffect* master, EFFECTPRIMID effectPrimId, const vector3d& deltaPos);
    u8 OnProcess() override;
    bool ProcessPrimitiveFrame();
    void SendMsg(CGameObject*, int msg, msgparam_t, msgparam_t, msgparam_t) override;
    void Render(matrix* viewMatrix) override;

    CRagEffect* m_master;
    EFFECTPRIMID m_type;
    vector3d m_deltaPos;
    vector3d m_deltaPos2;
    u32 m_renderFlag;
    int m_stateCnt;
    int m_duration;
    int m_fadeOutCnt;
    int m_pattern;
    int m_totalTexture;
    int m_spawnCount;
    float m_alpha;
    float m_alphaSpeed;
    float m_maxAlpha;
    float m_alphaDelta;
    float m_minAlpha;
    float m_size;
    float m_sizeSpeed;
    float m_sizeAccel;
    float m_radius;
    float m_radiusSpeed;
    float m_radiusAccel;
    float m_innerSize;
    float m_outerSize;
    float m_heightSize;
    float m_heightSpeed;
    float m_heightAccel;
    float m_longitude;
    float m_longSpeed;
    float m_longAccel;
    float m_speed;
    float m_accel;
    float m_gravSpeed;
    float m_gravAccel;
    float m_emitSpeed;
    float m_arcAngle;
    float m_latitude;
    float m_outerSpeed;
    float m_outerAccel;
    float m_innerSpeed;
    float m_innerAccel;
    float m_roll;
    float m_rollSpeed;
    float m_rollAccel;
    COLORREF m_tintColor;
    vector3d m_orgPos;
    vector3d m_pos;
    vector3d m_speed3d;
    matrix m_matrix;
    std::array<float, 4> m_param;
    std::vector<CTexture*> m_texture;
    std::string m_spriteActName;
    std::string m_spriteSprName;
    int m_spriteAction;
    int m_spriteMotionBase;
    int m_curMotion;
    int m_animSpeed;
    float m_spriteFrameDelay;
    float m_spriteScale;
    CSprRes* m_superAngelSprRes;
    CActRes* m_superAngelActRes;
    bool m_spriteRepeat;
    bool m_repeatAnim;
    bool m_isDisappear;
    int m_bandController;
    std::array<EffectBandState, 4> m_bands;
    std::array<EffectGi, 4> m_gi;
    int m_numSegments;
    std::array<EffectSegmentState, 12> m_segments;
};

class CRagEffect : public CGameObject {
public:
    CRagEffect();
    ~CRagEffect() override;

    void Init(CRenderObject* master, int effectId, const vector3d& deltaPos);
    void InitAtWorldPosition(int effectId, const vector3d& position);
    void InitWorld(const C3dWorldRes::effectSrcInfo& source);
    CEffectPrim* LaunchEffectPrim(EFFECTPRIMID effectPrimId, const vector3d& deltaPos);
    void DiscardPrimitive(CEffectPrim* prim);
    const vector3d& EffectPosition() const { return m_effectPos; }
    int GetEffectType() const { return m_type; }
    int GetStateCount() const { return m_stateCnt; }
    int GetDuration() const { return m_duration; }
    void AddDuration(int frames) { m_duration += frames; }
    void SetGroundHitFlag() { m_flag = 1; }
    void AbsorbPrimDelta(const vector3d& delta) { m_deltaPos = delta; }

    u8 OnProcess() override;
    void OnActorDeleted(const CGameActor* actor) override;
    void Render(matrix* viewMatrix) override;
    void SendMsg(CGameObject*, int, msgparam_t, msgparam_t, msgparam_t) override;

    vector3d ResolveBasePosition() const;
    vector3d ActorPosition() const;
    float ResolveBaseRotation() const;
    bool ResolveCullSphere(vector3d* outCenter, float* outRadius) const;

private:
    enum class Handler {
        None,
        EzStr,
        Portal,
        ReadyPortal,
        WarpZone,
        MapPillar,
        MapMagicZone,
        MapParticle,
        RecoveryHp,
        RecoverySp,
        HealLight,
        HealMedium,
        HealLarge,
        HideStart,
        GrimTooth,
        GrimToothAtk,
        IncAgility,
        EnchantPoison,
        EnchantPoison2,
        Blessing,
        Smoke,
        Torch,
        Sight,
        SightState,
        FireBall,
        Ruwach,
        SightAura,
        BlueFall,
        WeatherCloud,
    };

    class CWorldAnchor : public CRenderObject {
    public:
        explicit CWorldAnchor(const vector3d& position);
    };

    void ClearPrims();
    void LoadEzEffect(const char* fName);
    void InitEZ2STRFrame();
    bool ProcessEZ2STR(float x = 0.0f, float y = -80.0f, float z = 0.0f);
    bool ProcessTargetPositionStr();
    bool OnProcess2();
    bool ProcessEffectFrame();
    bool FinishEffectFrame();
    u8 ProcessWorldEffect();
    void ApplyHeal(const char* textureName, int variant);
    void ApplyHeal2(const char* textureName, int variant);
    void ApplyExit2(const char* textureName);
    void ApplyGlassWall2(const char* textureName);
    void ApplyParticleUpBurst();
    void ApplyHealTime(int variant);
    void ApplyFirstAid();
    void ApplyChemicalProtection(short time);
    void ApplyChemical2(short time);
    void ApplyChemical3(short time);
    void ApplyTeleportation2(const char* textureName, short time);
    void ApplyThrowItem(const char* textureName, int variant);
    void ApplyBottomMagnus(const char* textureName, int variant);
    void ApplyForestLight(const char* textureName, int variant);
    void ApplySaintWing(const char* textureName);
    void ApplyMagnum2(const char* textureName);
    void ApplySphereWind(const char* textureName, short time, int variant);
    void ApplyColorPaper(const char* textureName);
    void ApplySaintCasting(short time, const char* textureName, int variant);
    void ApplyCoupleCasting(const char* textureName);
    void ApplyHeartCasting(const char* textureName, float offsetX, float offsetZ);
    void ApplyGiExplosion(const char* textureName);
    void ApplyAcidDemon(const char* textureName);
    void ApplyFlowerCasting(const char* textureName, float size, int variant);
    void ApplyDefender(const char* textureName);
    void ApplyMapPillar(const char* textureName, float size, bool wide);
    void ApplyVolcano(const char* textureName, short variant, bool fastSpin);
    void ApplyBottomVolcano(const char* textureName, float size);
    void ApplyWind();
    void ApplyStormKick(int variant);
    void ApplyHalfSphere(const char* textureName);
    void ApplyAttackEnergy(const char* textureName);
    void ApplyElectric(short baseRise);
    void ApplyElectric2();
    void ApplyAttackEnergy2(const char* textureName);
    void ApplyPotionPillar(float speed, int duration);
    void ApplyPotionBerserk();
    void ApplyIceArrow();
    void ApplyProvidence(const char* textureName);
    void ApplyPairedSaintCasting(const char* textureName, int variant, bool playWave, float waveYOffset);
    void ApplyBeginCasting(const char* textureName, int option, bool playWave);
    void ApplyBottomMusic(const char* textureName, int variant);
    void ApplyBottomLight(const char* textureName, int size);
    void ApplyBottomOut(const char* textureName, int variant);
    void ApplyAuraBladeCasting();
    void ApplyBlind(int level);
    void ApplyPoison(int variant);
    void ApplyEntry2();
    void ApplyPortal2(int variant);
    void ApplyReadyPortal2();
    void ApplyWarpZone2(const char* textureName);
    void ApplyBlackDevil(const char* textureName);
    void ApplyPortalWind(int variant);
    void ApplyPortalWind402();
    void ApplyKyrieGuard();
    void ApplyBodyLight442();
    void ApplyBodyLight446();
    void ApplyBodyLight449();
    void ApplyActorLight();
    void ApplyTarot(const char* textureName, int variant);
    void LaunchParticleUp(const vector3d& origin, float sizeBase, float speedBase, bool secondTexture, u8 switchTexture);
    void RenderAniClip(int layerIndex, const KAC_LAYER& layer, const KAC_XFORMDATA& xform, matrix* viewMatrix);
    CRenderObject* ResolveCurrentMaster() const;
    CAbleToMakeEffect* ResolveCurrentEffectOwner() const;

    bool ApplyJobLvUp50(int time);
    bool ApplyJobLvUp50_2(short time);
    void ProcessJobLvUp50_2();
    void SpawnPortal();
    void SpawnReadyPortal();
    void SpawnWarpZone();
    void SpawnMapPillar();
    void SpawnMapMagicZone();
    void SpawnMapParticle();
    void ApplySuperAngel(signed char variant, signed char time);
    void ProcessSuperAngelEffect();
    void SpawnRecoveryHp();
    void SpawnRecoverySp();
    void SpawnHealLight();
    void SpawnHealMedium();
    void SpawnHealLarge();
    void SpawnHideStart();
    void SpawnGrimTooth();
    void SpawnGrimToothAtk();
    void SpawnIncAgility();
    void SpawnEnchantPoison();
    void SpawnEnchantPoison2();
    void SpawnBlessing();
    void SpawnSmoke();
    void SpawnTorch();
    void SpawnSight();
    void SpawnSightState();
    void SpawnFireBall();
    void SpawnRuwach();
    void SpawnSightAura();
    void SpawnBlueFall();
    void SpawnWeatherCloud();

    std::list<CEffectPrim*> m_primList;
    CRenderObject* m_master;
    bool m_ownsMaster;
    bool m_masterIsActor;
    u32 m_masterActorGid;
    Handler m_handler;
    int m_type;
    int m_flag;
    int m_count;
    int m_stateCnt;
    int m_duration;
    int m_cEndLayer;
    int m_iCurLayer;
    int m_worldType;
    bool m_loop;
    bool m_isWorldEffect;
    bool m_strFrameActive;
    u32 m_lastProcessTick;
    float m_tickCarryMs;
    vector3d m_strScreenOffset;
    vector3d m_strAnchorPosition;
    vector3d m_effectPos;
    vector3d m_cachedPos;
    vector3d m_deltaPos;
    vector3d m_deltaPos2;
    vector3d m_targetPos;
    vector3d m_grimToothOrigin;
    vector3d m_grimToothStep;
    vector3d m_grimToothRemaining;
    float m_param[4];
    float m_emitSpeed;
    float m_longitude;
    float m_tlvertX;
    float m_tlvertY;
    float m_tlvertZ;
    COLORREF m_tintColor;
    std::string m_effectName;
    CEZeffectRes* m_ezEffectRes;
    KANICLIP* m_aniClips;
    std::array<KAC_XFORMDATA, 128> m_actXformData;
    std::array<u8, 128> m_isLayerDrawn;
    std::array<int, 128> m_aiCurAniKey;
    bool m_hasTargetPos;
};

CRagEffect* CreateWorldRagEffect(const C3dWorldRes::effectSrcInfo& source);
