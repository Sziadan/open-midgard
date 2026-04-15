#pragma once

#include "GameActor.h"
#include "res/EzEffectRes.h"
#include "res/WorldRes.h"

#include <array>
#include <list>
#include <string>
#include <vector>

class CTexture;

enum EFFECTPRIMID {
    PP_3DCIRCLE = 0,
    PP_3DCYLINDER,
    PP_3DPARTICLEORBIT,
    PP_3DPARTICLE,
    PP_3DPARTICLEGRAVITY,
    PP_3DPARTICLESPLINE,
    PP_3DTEXTURE,
    PP_3DCROSSTEXTURE,
    PP_3DSPHERE,
    PP_3DQUADHORN,
    PP_2DCIRCLE,
    PP_2DFLASH,
    PP_2DTEXTURE,
    PP_MAPMAGICZONE,
    PP_MAPPARTICLE,
    PP_SUPERANGEL,
    PP_RADIALSLASH,
    PP_SLASH1,
    PP_PORTALSTACK,
    PP_CASTINGRING4,
    PP_EFFECTSPRITE,
    PP_HEAL,
    PP_PORTAL,
    PP_WIND,
    PP_CLOUD,
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

struct EffectSegmentState {
    vector3d pos{};
    float radius = 0.0f;
    float size = 0.0f;
    float alpha = 0.0f;
    matrix transform{};
};

constexpr int kRagEffectPortal = 50001;
constexpr int kRagEffectReadyPortal = 50002;
constexpr int kRagEffectWarpZone = 50003;

class CRagEffect;

class CEffectPrim {
public:
    CEffectPrim();

    void Init(CRagEffect* master, EFFECTPRIMID effectPrimId, const vector3d& deltaPos);
    bool OnProcess();
    void Render(matrix* viewMatrix);

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
    bool m_spriteRepeat;
    bool m_repeatAnim;
    bool m_isDisappear;
    int m_bandController;
    std::array<EffectBandState, 4> m_bands;
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
    void DetachFromMaster();
    int GetEffectType() const { return m_type; }
    int GetStateCount() const { return m_stateCnt; }
    int GetDuration() const { return m_duration; }

    u8 OnProcess() override;
    void Render(matrix* viewMatrix) override;
    void SendMsg(CGameObject*, int, msgparam_t, msgparam_t, msgparam_t) override;

    vector3d ResolveBasePosition() const;
    float ResolveBaseRotation() const;
    bool ResolveCullSphere(vector3d* outCenter, float* outRadius) const;

private:
    enum class Handler {
        None,
        EzStr,
        Entry2,
        JobLevelUp50,
        Portal,
        ReadyPortal,
        WarpZone,
        Portal2,
        ReadyPortal2,
        WarpZone2,
        MapMagicZone,
        MapParticle,
        SuperAngel,
        RecoveryHp,
        RecoverySp,
        BeginCasting,
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
        Sight,
        SightState,
        FireBall,
        Ruwach,
        SightAura,
        FireBoltRain,
        WeatherCloud,
    };

    class CWorldAnchor : public CRenderObject {
    public:
        explicit CWorldAnchor(const vector3d& position);
    };

    void ClearPrims();
    void LoadEzEffect(const char* fName);
    void InitEZ2STRFrame();
    bool ProcessEZ2STR();
    void RenderAniClip(int layerIndex, const KAC_LAYER& layer, const KAC_XFORMDATA& xform, matrix* viewMatrix);

    void SpawnEntry2();
    void SpawnJobLevelUp50();
    void SpawnPortal();
    void SpawnReadyPortal();
    void SpawnWarpZone();
    void SpawnPortal2();
    void SpawnReadyPortal2();
    void SpawnWarpZone2();
    void SpawnMapMagicZone();
    void SpawnMapParticle();
    void SpawnSuperAngelVariant(int variant, int birthFrame);
    void SpawnSuperAngelBurst(int startAngle);
    void UpdateSuperAngel();
    void SpawnRecoveryHp();
    void SpawnRecoverySp();
    void SpawnBeginCasting();
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
    void SpawnSight();
    void SpawnSightState();
    void SpawnFireBall();
    void SpawnRuwach();
    void SpawnSightAura();
    void SpawnFireBoltRain();
    void SpawnWeatherCloud();

    std::list<CEffectPrim*> m_primList;
    CRenderObject* m_master;
    bool m_ownsMaster;
    Handler m_handler;
    int m_type;
    int m_level;
    int m_flag;
    int m_count;
    int m_stateCnt;
    int m_duration;
    int m_cEndLayer;
    int m_iCurLayer;
    int m_worldType;
    bool m_loop;
    u32 m_lastProcessTick;
    float m_tickCarryMs;
    vector3d m_cachedPos;
    vector3d m_deltaPos;
    vector3d m_targetPos;
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
    std::array<int, 128> m_activeAniKeyFrame;
    std::array<int, 128> m_activeAniKeyAppliedState;
    std::array<u8, 128> m_activeAniKeyZeroBlend;
    bool m_hasTargetPos;
};

CRagEffect* CreateWorldRagEffect(const C3dWorldRes::effectSrcInfo& source);
