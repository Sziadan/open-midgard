#pragma once

#include "GameActor.h"
#include "res/EzEffectRes.h"
#include "res/WorldRes.h"

#include <array>
#include <list>
#include <optional>
#include <string>
#include <vector>

class CTexture;
class CSprRes;
class CActRes;

// HighPriest.exe.h:2866-2997. All 129 original ids, with explicit values.
enum EFFECTPRIMID {
    PP_2DTEXTURE = 0x0,
    PP_2DTEXTUREORBIT = 0x1,
    PP_2DQUAD = 0x2,
    PP_2DFLASH = 0x3,
    PP_2DCIRCLE = 0x4,
    PP_3DPARTICLE = 0x5,
    PP_3DPARTICLEGRAVITY = 0x6,
    PP_3DPARTICLEORBIT = 0x7,
    PP_3DPARTICLESPLINE = 0x8,
    PP_3DQUADHORN = 0x9,
    PP_3DSPLINE = 0xA,
    PP_3DSPIRAL = 0xB,
    PP_3DCIRCLE = 0xC,
    PP_3DCYLINDER = 0xD,
    PP_3DSPHERE = 0xE,
    PP_3DTEXTURE = 0xF,
    PP_3DCROSSTEXTURE = 0x10,
    PP_3DSHINELIGHT = 0x11,
    PP_3DCYLINDERHALF = 0x12,
    PP_3DFLASH = 0x13,
    PP_3DLINE = 0x14,
    PP_3DPARTICLE_NOMASTER = 0x15,
    PP_3DTEXTURE_NOMASTER = 0x16,
    PP_3DCASTING = 0x17,
    PP_3DCASTING_2 = 0x18,
    PP_3DCASTING_3 = 0x19,
    PP_MAPMAGICZONE = 0x1A,
    PP_MAPPARTICLE = 0x1B,
    PP_3DAURA = 0x1C,
    PP_3DAURA_2 = 0x1D,
    PP_GI_1 = 0x1E,
    PP_GI_2 = 0x1F,
    PP_CHOOKGI = 0x20,
    PP_CLOUD = 0x21,
    PP_GI_3 = 0x22,
    PP_GUMGANG = 0x23,
    PP_GI_4 = 0x24,
    PP_GRANDCROSS2 = 0x25,
    PP_LINELINK = 0x26,
    PP_LANDPROTECTOR = 0x27,
    PP_SAKURA = 0x28,
    PP_MAPPILLAR = 0x29,
    PP_SHIELDBOOMERANG = 0x2A,
    PP_INTIMIDATE = 0x2B,
    PP_TEIHIT1 = 0x2C,
    PP_TEIHIT2 = 0x2D,
    PP_CHIMTO = 0x2E,
    PP_BOTTOM = 0x2F,
    PP_BOTTOM2 = 0x30,
    PP_BOTTOMOUT = 0x31,
    PP_GI_5 = 0x32,
    PP_BOTTOMSPR = 0x33,
    PP_POKJUK = 0x34,
    PP_THROWITEM = 0x35,
    PP_CHEMICALPROTECTION = 0x36,
    PP_CHEMICAL2 = 0x37,
    PP_CHEMICAL3 = 0x38,
    PP_TELEPORTATION2 = 0x39,
    PP_FIRSTAID = 0x3A,
    PP_PARTICLE_UP = 0x3B,
    PP_HEAL = 0x3C,
    PP_GUARD = 0x3D,
    PP_WIND = 0x3E,
    PP_PORTAL = 0x3F,
    PP_RECT_UP = 0x40,
    PP_3DCASTING_4 = 0x41,
    PP_FOREST = 0x42,
    PP_FOOT = 0x43,
    PP_ASURACASTING = 0x44,
    PP_TRIPLEATTACK = 0x45,
    PP_HITLINE = 0x46,
    PP_BLIND = 0x47,
    PP_POISON = 0x48,
    PP_MAGNUM2 = 0x49,
    PP_CALLZONE = 0x4A,
    PP_SLASH1 = 0x4B,
    PP_SAINTWING = 0x4C,
    PP_SPHEREWIND = 0x4D,
    PP_COLORPAPER = 0x4E,
    PP_WATERFALL = 0x4F,
    PP_WATERFALLPARTICLE = 0x50,
    PP_LIGHTSPHERE = 0x51,
    PP_EFFECTTEXTURE = 0x52,
    PP_MINI_TETRIS = 0x53,
    PP_GHOST = 0x54,
    PP_SUPERANGEL = 0x55,
    PP_SOULBREAKER = 0x56,
    PP_PRESSURE = 0x57,
    PP_GROUNDSHAKE = 0x58,
    PP_BASH3D = 0x59,
    PP_RECT_UP2 = 0x5A,
    PP_SPHEREWIND2 = 0x5B,
    PP_SOULBREAKER2 = 0x5C,
    PP_RAINBOW = 0x5D,
    PP_PEONG = 0x5E,
    PP_EFFECTTEXTURE2 = 0x5F,
    PP_PEONGUP = 0x60,
    PP_TANJI2 = 0x61,
    PP_AIRTEXTURE = 0x62,
    PP_DOUBLEGUMGANG = 0x63,
    PP_HITLINE3 = 0x64,
    PP_ELECTRIC = 0x65,
    PP_ELECTRIC2 = 0x66,
    PP_STORMKICK = 0x67,
    PP_HALFSPHERE = 0x68,
    PP_ATTACKENERGY = 0x69,
    PP_ATTACKENERGY2 = 0x6A,
    PP_3DCASTING_5 = 0x6B,
    PP_FLOWERCAST = 0x6C,
    PP_SOULLINK = 0x6D,
    PP_HEAL_LOW = 0x6E,
    PP_SOULPARTICLE = 0x6F,
    PP_GROUNDSAMPLE = 0x70,
    PP_GI_EXPLOSION = 0x71,
    PP_HERMODE = 0x72,
    PP_3DAURA_3 = 0x73,
    PP_SHIELDBOOMERANG3 = 0x74,
    PP_3DQUADHORN2 = 0x75,
    PP_ACIDDEMON = 0x76,
    PP_STIN = 0x77,
    PP_SMA = 0x78,
    PP_SMA2 = 0x79,
    PP_STIN2 = 0x7A,
    PP_TWILIGHT = 0x7B,
    PP_KOUENKA = 0x7C,
    PP_STIN4 = 0x7D,
    PP_STIN5 = 0x7E,
    PP_BUBBLE_DROP = 0x7F,
    PP_EFFECTTEXTURE_ANI = 0x80,
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

constexpr int kPrimTypeParticleUp = PP_PARTICLE_UP;
constexpr int kPrimTypeBottom = PP_BOTTOM;
constexpr int kPrimTypeBottomOut = PP_BOTTOMOUT;
constexpr int kPrimTypeHeal = PP_HEAL;
constexpr int kPrimTypeHealLow = PP_HEAL_LOW;
constexpr int kPrimTypeGiExplosion = PP_GI_EXPLOSION;
constexpr int kPrimTypeAcidDemon = PP_ACIDDEMON;
constexpr int kPrimTypeFlowerCast = PP_FLOWERCAST;
constexpr int kPrimTypeRectUp = PP_RECT_UP;
constexpr int kPrimTypeBlind = PP_BLIND;
constexpr int kPrimTypeCasting4 = PP_3DCASTING_4;
constexpr int kPrimTypeCasting5 = PP_3DCASTING_5;
constexpr int kPrimTypeThrowItem = PP_THROWITEM;
constexpr int kPrimTypeEffectTexture = PP_EFFECTTEXTURE;
constexpr int kPrimTypeChemical = PP_CHEMICALPROTECTION;
constexpr int kPrimTypeChemical2 = PP_CHEMICAL2;
constexpr int kPrimTypeChemical3 = PP_CHEMICAL3;
constexpr int kPrimTypeTeleport2 = PP_TELEPORTATION2;
constexpr int kPrimTypeFirstAid = PP_FIRSTAID;
constexpr int kPrimTypeWind = PP_WIND;
constexpr int kPrimTypePortal = PP_PORTAL;
constexpr int kPrimTypePoison = PP_POISON;
constexpr int kPrimTypeForest = PP_FOREST;
constexpr int kPrimTypeSaintWing = PP_SAINTWING;
constexpr int kPrimTypeMagnum2 = PP_MAGNUM2;
constexpr int kPrimTypeSphereWind = PP_SPHEREWIND;
constexpr int kPrimTypeColorPaper = PP_COLORPAPER;
constexpr int kPrimTypeGi1 = PP_GI_1;
constexpr int kPrimTypeGi2 = PP_GI_2;
constexpr int kPrimTypeGi4 = PP_GI_4;
constexpr int kPrimTypeStormKick = PP_STORMKICK;
constexpr int kPrimTypeHalfSphere = PP_HALFSPHERE;
constexpr int kPrimTypeAttackEnergy = PP_ATTACKENERGY;
constexpr int kPrimTypeElectric = PP_ELECTRIC;
constexpr int kPrimTypeElectric2 = PP_ELECTRIC2;
constexpr int kPrimTypeAttackEnergy2 = PP_ATTACKENERGY2;
constexpr int kPrimTypeGi5 = PP_GI_5;
constexpr int kPrimTypeCasting = PP_3DCASTING;
constexpr int kPrimTypeCasting2 = PP_3DCASTING_2;
constexpr int kPrimTypeMapPillar = PP_MAPPILLAR;
constexpr int kPrimTypeCasting3 = PP_3DCASTING_3;

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
    float longitude = 0.0f;
    float size = 0.0f;
    float alpha = 0.0f;
    int process = 0;
    u32 argb = 0;
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
    bool UsesLegacyBandPayload() const;
    bool UsesReferenceParticle() const;
    bool UsesReferenceCircle() const;
    bool UsesReferenceCylinder() const;
    bool UsesReferenceCrossTexture() const;
    bool UsesReferenceTexture() const;
    bool UsesReferenceScreenTexture() const;
    bool UsesReferenceScreenQuad() const;
    bool UsesReferenceQuadHorn() const;
    bool UsesReferenceFlash() const;
    bool UsesReferenceScreenCircle() const;
    bool UsesReferenceSphere() const;
    void SetParticleResources(const char* sprName, const char* actName, bool actionFirst = false);
    void SendMsg(CGameObject*, int msg, msgparam_t, msgparam_t, msgparam_t) override;
    void Render(matrix* viewMatrix) override;

    CRagEffect* m_master;
    EFFECTPRIMID m_type;
    vector3d m_deltaPos;
    vector3d m_deltaPos2;
    vector3d m_deltaPos3;
    vector3d m_deltaPosAccel;
    u32 m_renderFlag;
    int m_stateCnt;
    int m_duration;
    int m_fadeOutCnt;
    int m_pattern;
    int m_commonPattern = 0;
    int m_totalTexture;
    int m_spawnCount;
    float m_alpha;
    float m_alphaSpeed;
    float m_maxAlpha;
    float m_alphaDelta;
    float m_minAlpha;
    float m_widthSize;
    float m_widthSpeed;
    float m_widthAccel;
    float m_size;
    float m_sizeSpeed;
    float m_sizeAccel;
    float m_radius;
    float m_radiusSpeed;
    float m_radiusAccel;
    float m_innerSize;
    float m_outerSize;
    float m_heightSize;
    float m_maxHeightSize;
    u32 m_argb;
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
    float m_latiSpeed;
    float m_outerSpeed;
    float m_outerAccel;
    float m_innerSpeed;
    float m_innerAccel;
    float m_roty;
    float m_roll;
    float m_rollSpeed;
    float m_rollAccel;
    std::array<float, 3> m_rotAxis{};
    COLORREF m_tintColor;
    vector3d m_orgPos;
    vector3d m_pos;
    vector3d m_speed3d;
    std::array<vector3d, 4> m_segPos{};
    matrix m_matrix;
    matrix m_rotMat;
    std::array<float, 4> m_param;
    std::vector<CTexture*> m_texture;
    std::vector<float> m_chPoint;
    std::vector<float> m_chVal1;
    bool m_chRadiSpeed = false;
    bool m_chSpeed = false;
    bool m_AlphaShow = false;
    int m_chCnt = 0;
    int m_count = 0;
    int m_any = 0;
    int m_red = 255, m_green = 255, m_blue = 255;
    int m_redSpeed = 0, m_greenSpeed = 0, m_blueSpeed = 0;
    int m_rgbCycleCnt = 0, m_rgbCycleDelay = 0;
    float m_v1 = 0.0f, m_v2 = 1.0f;
    float m_latiAccel = 0.0f;
    int m_changePoint = -1;
    bool m_isChangeProcessed = false;
    int m_fadeInCnt = -1;
    float m_changeSpeed = 0.000099999997f;
    float m_changeAccel = 0.000099999997f;
    float m_changeHeightSpeed = 0.000099999997f;
    float m_changeWidthSpeed = 0.000099999997f;
    float m_changeWidthAccel = 0.000099999997f;
    float m_changeHeightAccel = 0.000099999997f;
    float m_changeSpeedY = 0.0f;
    float m_changeAccelY = 0.0f;
    vector3d m_changeSpeed3d{};
    vector3d m_changeAccel3d{};
    int m_playWaveType = 0;
    std::vector<float> m_chVal2;
    std::string m_spriteActName;
    std::string m_spriteSprName;
    int m_spriteAction;
    int m_spriteMotionBase;
    int m_curMotion;
    int m_animSpeed;
    float m_spriteFrameDelay;
    float m_spriteScale;
    CSprRes* m_particleSprRes;
    CActRes* m_particleActRes;
    bool m_spriteRepeat;
    bool m_repeatAnim;
    bool m_isDisappear;
    bool m_isHit = false;
    int m_bandController;
    std::array<EffectBandState, 4> m_bands;
    std::array<EffectGi, 4> m_gi;
    int m_numSegments;
    std::array<EffectSegmentState, 50> m_segments;
    std::vector<std::array<vector3d, 4>> m_segmentQuadPositions = std::vector<std::array<vector3d, 4>>(50);
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
    bool IsWorldEffect() const { return m_isWorldEffect; }
    const vector3d& EffectPosition() const { return m_effectPos; }
    int GetEffectType() const { return m_type; }
    int GetStateCount() const { return m_stateCnt; }
    int GetDuration() const { return m_duration; }
    void AddDuration(int frames) { m_duration += frames; }
    vector3d ScreenPosition() const { return {m_tlvertX,m_tlvertY,m_tlvertZ}; }
    const vector3d& TargetPosition() const { return m_deltaPos; }
    void SetGroundHitFlag() { m_flag = 1; }
    void AbsorbPrimDelta(const vector3d& delta) { m_deltaPos = delta; }
    void SetAction(int action, int motion, int type) { m_effectAction = action; m_effectActionMotion = motion; m_effectActionType = type; }

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
        Smoke,
        Torch,
    };

    class CWorldAnchor : public CRenderObject {
    public:
        explicit CWorldAnchor(const vector3d& position);
    };

    void ClearPrims();
    void LoadEzEffect(const char* fName);
    void InitEZ2STRFrame();
    bool ProcessEZ2STR(float x = 0.0f, float y = -80.0f, float z = 0.0f);
    bool ProcessProvoke();
    bool ProcessLexDivina();
    bool ProcessStrTerminalEffect();
    bool ProcessClipWaveStrEffect();
    bool ProcessEffectSprite(int spriteVariant);
    bool ProcessEffectSpriteAndStr();
    bool ProcessTargetPositionStr();
    bool OnProcess2();
    bool OnProcess3();
    bool OnProcess2Legacy();
    bool ProcessEffectFrame();
    bool ProcessEffectFrameLegacy();
    bool UnportedEffect();
    bool ProcessWorldEffectFrame();
    bool FinishEffectFrame();
    u8 ProcessWorldEffect();
    void ApplyHeal(const char* textureName, int variant);
    void ApplyHeal2(const char* textureName, int variant);
    void ApplyExit2(const char* textureName);
    bool ApplyGlassWall2(const char* textureName);
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
    void ApplyStin4();
    void SpawnStin4(const char* textureName, char variant);
    void ApplyThunderStorm2();
    void ApplyStin5();
    void SpawnStin5(const char* textureName);
    void ApplyRgCoin3();
    void SpawnRgCoin2(const char* textureName, float size);
    void ApplyBash3D5();
    void SpawnChookgi(char variant);
    void ApplyEarthSpike(char variant);
    void ApplyColorPaper(const char* textureName);
    void SpawnStatusState();
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
    bool ProcessPotionConcentration();
    bool ProcessTarotEffect();
    bool ProcessEffectTextureAnimation();
    bool ProcessEffectTextureSet();
    bool ProcessEffect695MapFireworks();
    bool ProcessLevel99Effect();
    bool ProcessLevel99_2Effect();
    bool ProcessLevel99_3Effect();
    bool ProcessGumGangEffect();
    bool ProcessBash3DEffect();
    bool ProcessHitLineEffect(short f1 = 2, bool applyBodyLight = true);
    bool ProcessHitLine4Effect();
    bool ProcessHitLine5Effect();
    bool ProcessHitLine3Effect();
    bool ProcessHitLine3Basic434();
    bool ProcessFoot447();
    bool ProcessFoot448();
    bool ProcessFoot326_327(const char* textureName);
    bool ProcessCallZone340();
    bool ProcessBeginAsura1_467();
    bool ProcessBeginAsura1Variant(const char* textureName);
    bool ProcessTeiHitEffects();
    bool ProcessChimtoEffect(int variant);
    bool ProcessStinSmaEffects();
    bool ProcessShieldBoomerangEffects();
    bool ProcessRgCoinEffects();
    void SpawnRgCoin(const char* textureName, float size, u8 colorMode);
    bool ProcessGrandCrossEffects();
    void SpawnGrandCross(int variant);
    bool ProcessLineLinkEffects();
    void SpawnLineLink(int variant);
    bool ProcessTripleAttackEffects();
    void SpawnTripleAttack(int variant);
    bool ProcessSoulParticle506();
    bool ProcessEffect502();
    bool ProcessEffect505();
    bool ProcessSma2Effect552();
    void SpawnSma2();
    bool ProcessParticlePath556();
    void SpawnParticlePath556();
    bool ProcessEffect537();
    bool ProcessEffect273();
    bool ProcessEffect440();
    bool ProcessLandProtector(int variant);
    bool ProcessBottomVertical(int variant);
    bool ProcessTanji(bool tanji2);
    bool ProcessBasilica();
    bool ProcessRainbow410();
    bool ProcessHitLine3Zero428();
    bool ProcessPokJuk297();
    bool ProcessEffect365();
    void SpawnPressure365();
    void SpawnGroundShake365();
    bool ProcessEffect599();
    bool ProcessEffect602();
    bool ProcessDemonstration302();
    void SpawnDemonstration302();
    bool ProcessMaple333();
    void SpawnMaple333();
    bool ProcessEntry6();
    void SpawnEntry6();
    bool ProcessWarp8();
    void SpawnWarp8();
    bool ProcessEnhance9();
    void SpawnEnhance9();
    bool ProcessLord90();
    bool ProcessIceWall74();
    bool ProcessGlassWall13();
    bool ProcessMeteorStorm92();
    bool ProcessNpcStopEffects();
    void SpawnNpcStop(int variant);
    bool ProcessGuardEffects();
    void SpawnGuard(int variant);
    bool ProcessTwilightEffects();
    void SpawnTwilight(int variant);
    bool ProcessParticleTimeEffects();
    void SpawnParticleTime(int variant);
    bool ProcessParticleSpreadEffects();
    void SpawnParticleSpread();
    bool ProcessSphereWind2Effect();
    void SpawnSphereWind2(short time);
    bool ProcessSoulBreaker2Effect();
    bool ProcessSoulBreaker361Effect();
    void SpawnSoulBreaker361();
    void SpawnSoulBreaker2(short time);
    bool ProcessPeongEffects();
    void SpawnPeongUp(const char* textureName, short variant);
    void SpawnPeong(const char* textureName, int time);
    bool ProcessBeginAsuraEffect();
    bool ProcessBeginAsura2Effect();
    void SpawnBeginAsura(int variant, bool secondBurst);
    void SpawnSoulLinkLight(const char* textureName);
    void SpawnBeginAsura2(bool secondBurst);
    bool ProcessEnergyDrain2Effect();
    bool ProcessEnergyDrain3Effect();
    bool ProcessPortalWind389();
    bool ProcessRunStop444();
    void SpawnRunStop(short time);
    bool ProcessCartBoost456();
    bool ProcessExit7();
    bool ProcessEndure11();
    bool ProcessSoulStrike15(int variant);
    bool ProcessDetoxication21();
    bool ProcessYufitel93();
    bool ProcessYufitelHit(int variant);
    bool ProcessFlasher99();
    bool ProcessRemoveTrap100();
    bool ProcessBlitzBeat115();
    bool ProcessBottom114();
    bool ProcessBottom2Effect137();
    bool ProcessCloaking120();
    bool ProcessMakeBlur();
    bool ProcessMakeBlur2();
    void SpawnEnergyDrain2();
    void SpawnEnergyDrain3();
    void SpawnStin();
    void SpawnStin2(int variant);
    void SpawnSma(int variant);
    void SpawnTeiHit1(const char* textureName, char variant);
    void SpawnTeiHit2();
    void SpawnTeiHit3(const char* textureName, char variant);
    bool ProcessTier3StrEffect();
    bool ProcessThrowItemEffect();
    bool ProcessHealBlue();
    bool ApplyPotionBerserk();
    void ApplyIceArrow();
    void ApplyFireArrow();
    void ApplyKouenka();
    void SpawnBash3D(const char* textureName, char f1, char f2);
    void ApplyFrostDiver();
    void ApplyFrostDiver2();
    void ApplyHasteUp();
    void ApplyHealSP();
    void ApplyHeavensDrive();
    void ApplyOverThrust();
    void ApplyTeleportation();
    void ApplyMagnumBreak();
    void ApplySonicBlow();
    void ApplySonicBlowHit();
    void ApplyBash();
    bool ProcessSignum();
    bool ProcessAngelus();
    bool ProcessFireHit();
    std::optional<bool> ProcessReferenceStrOnly();
    void SpawnFireSplashHit();
    void SpawnColdHit();
    void SpawnDetecting();
    void SpawnSprayPond();
    void ApplyProvidence(const char* textureName);
    void ApplyPairedSaintCasting(const char* textureName, int variant, bool playWave, float waveYOffset);
    void ApplyBeginCasting(const char* textureName, int option, bool playWave);
    void ApplyElementalChangeCasting();
    void ApplyBottomMusic(const char* textureName, int variant);
    void ApplyBottomLight(const char* textureName, int size);
    void ApplyBottomOut(const char* textureName, int variant);
    void ApplyAuraBladeCasting();
    void ApplyBlind(int level);
    void ApplyPoison(int variant);
    void ApplyEntry2();
    void ApplyPortal2(int variant);
    void ApplyReadyPortal();
    void ApplyPortal();
    void ApplyReadyPortal2();
    void ApplyWarpZone2(const char* textureName);
    void ApplyWarpZone();
    void ApplyBlackDevil(const char* textureName);
    void ApplyPortalWind(int variant);
    void ApplyPortalWind402();
    void ApplyKyrieGuard();
    void ApplyBodyLight442();
    void ApplyBodyLight446();
    void ApplyBodyLight449();
    void ApplyBodyLight368();
    void ApplyActorLight();
    void ApplyEffectTexture(const char* textureName, int variant);
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
    void SpawnHideStart();
    void SpawnGrimTooth();
    void SpawnGrimToothAtk();
    void SpawnIncAgility();
    void SpawnDecAgility();
    void SpawnBeginSpell();
    void SpawnPoisonHit();
    void SpawnAqua();
    void SpawnEnchantPoison();
    void SpawnEnchantPoison2();
    void ProcessEnchantPoison2();
    void SpawnBlessing();
    void SpawnSmoke();
    void SpawnDragonSmoke();
    void SpawnLockOn();
    void SpawnSpearBmr();
    void SpawnTurnUndead();
    void SpawnWaterBall2();
    bool ProcessRevive140();
    bool ProcessAspersio86();
    bool ProcessStormGust89();
    bool ProcessKyrie112();
    bool ProcessRepairWeapon101();
    bool ProcessMaxPower104();
    bool ProcessIntimidate227();
    bool ProcessShockwave145();
    bool ProcessMagicRod244();
    bool ProcessShieldCharge246();
    bool ProcessStripEquipment269To272();
    bool ProcessAirTexture416();
    bool ProcessEffect380();
    bool ProcessEffect403();
    bool ProcessEffect413();
    bool ProcessEffect445();
    bool ProcessEffect457();
    bool ProcessEffect466();
    bool ProcessEffect501();
    bool ProcessEffect704();
    bool ProcessWaterBall116();
    bool ProcessFireIvy118();
    bool ProcessFirePillar138();
    bool ProcessBanjjakii165();
    void SpawnDarkBreath();
    void SpawnSummonSlave();
    void SpawnBloodDrain();
    void SpawnEnergyDrain();
    void SpawnTestEffect();
    void SpawnTopRank();
    void SpawnParty();
    void SpawnSnow();
    void SpawnSakura();
    void SpawnTorch();
    void ApplySight(float initialAlpha, bool includeShadow);
    void SpawnSight();
    void SpawnSight2();
    void ApplySightrasher();
    void ApplyBarrier();
    void ApplyCone();
    void ApplySphere();
    void ApplyBowlingBash();
    bool ApplyCartRevolution();
    void ApplyVenomDust2();
    void ApplyCurseAttack();
    void SpawnFireBall();
    void SpawnFireFly();
    void SpawnSandWind();
    void SpawnSteal();
    bool ProcessCoin();
    void SpawnHit1();
    void SpawnHitDark();
    void SpawnHit(bool dark);
    void SpawnPierce();
    void SpawnHit2(int option);
    bool ProcessHit2Blue();
    void ApplyNapalmHit();
    void SpawnNapalmBeat();
    void SpawnHit3();
    void SpawnHit4();
    void SpawnHit5();
    void SpawnHit6();
    void SpawnCrossLensHit(bool compact);
    void SpawnRuwach();
    void SpawnBlueFall();
    void SpawnWeatherCloud();
    void ProcessGroundSample();

    std::list<CEffectPrim*> m_primList;
    CRenderObject* m_master;
    bool m_ownsMaster;
    bool m_masterIsActor;
    u32 m_masterActorGid;
    Handler m_handler;
    int m_type;
    int m_flag;
    int m_count;
    int m_level;
    int m_stateCnt;
    bool m_shadowOn;
    int m_duration;
    int m_cEndLayer;
    int m_iCurLayer;
    int m_effectAction = 0;
    int m_effectActionMotion = 0;
    int m_effectActionType = 0;
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
    vector3d m_deltaPos3;
    vector3d m_targetPos;
    float m_param[4];
    float m_emitSpeed;
    float m_latitude;
    float m_longitude;
    float m_roty;
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
