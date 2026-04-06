#pragma once
#include "Types.h"
#include "res/Sprite.h"
#include "res/ActRes.h"
#include "Granny.h"
#include <string>
#include <vector>
#include <list>

// Forward declarations
class CGameObject;
class CRagEffect;
class CMsgEffect;
class UIBalloonText;
class UIChatRoomTitle;
class UIMerchantShopTitle;
class UIRechargeGage;
class UIPcGage;
class C3dActor;
class CTexture;

struct ACTOR_COLOR {
    u8 a, r, g, b;
};

struct COLOR {
    union {
        struct { u8 b, g, r, a; } s;
        u32 color;
    } u;
};

struct RENDER_INFO_RECT {
    float left, top, right, bottom, oow;
};

// struct matrix is in Types.h

struct PathCell {
    int x = 0;
    int y = 0;
    unsigned int arrivalTime = 0;
};

struct CPathInfo {
    std::vector<PathCell> m_cells;

    void Reset() { m_cells.clear(); }
    unsigned int GetTotalExpectedMovingTime() const {
        return m_cells.empty() ? 0u : m_cells.back().arrivalTime;
    }
};

struct WBA {
    u32 gid = 0;
    u32 time = 0;
    int message = 0;
    int attackedMotionTime = 0;
    float damageDestX = 0.0f;
    float damageDestZ = 0.0f;
    char waveName[128] = {};
};
struct _MSG2AI { u8 gap[32]; };
struct PROCEEDTYPE { int type; };

constexpr int kGameActorAttackStateId = 2;
constexpr int kGameActorDeathStateId = 3;
constexpr int kGameActorPickupStateId = 5;
constexpr int kGameActorSkillStateId = 7;
constexpr int kGameActorCastingStateId = 8;
constexpr int kGameActorCastingLoopStateId = 13;

//===========================================================================
// Actor Hierarchy
//===========================================================================

class CGameObject {
public:
    CGameObject() {}
    virtual ~CGameObject() {}
    virtual u8   OnProcess() { return 1; }
    virtual void SendMsg(CGameObject* src, int msg, msgparam_t par1, msgparam_t par2, msgparam_t par3) {}
    virtual void Render(matrix* m) {}
    virtual int  Get8Dir(float rot);
};

class CRenderObject : public CGameObject {
public:
    CRenderObject();

    vector3d m_pos;
    std::string m_bodyPaletteName;
    int m_baseAction;
    int m_curAction;
    int m_curMotion;
    int m_oldBaseAction;
    int m_oldMotion;
    int m_bodyPalette;
    float m_roty;
    float m_zoom;
    float m_shadowZoom;
    float m_motionSpeed;
    float m_lastPixelRatio;
    float m_loopCountOfmotionFinish;
    float m_modifyFactorOfmotionSpeed;
    float m_modifyFactorOfmotionSpeed2;
    int m_motionType;
    int m_stateId;
    int m_oldstateId;
    int m_sprShift;
    int m_sprAngle;
    int m_offsetOow;
    u32 m_colorOfSingleColor;
    u32 m_singleColorStartTick;
    u32 m_stateStartTick;
    ACTOR_COLOR m_oldColor;
    ACTOR_COLOR m_curColor;
    union {
        u32 m_sprArgb;
        COLOR m_sprColor;
    } m_u28;
    u8 m_isLieOnGround;
    u8 m_isMotionFinished;
    u8 m_isMotionFreezed;
    u8 m_isSingleColor;
    u8 m_isVisible;
    u8 m_isVisibleBody;
    u8 m_alwaysTopLayer;
    u8 m_isSprArgbFixed;
    u8 m_shadowOn;
    u8 m_shouldAddPickInfo;
    int m_isPc;
    int m_lastTlvertX;
    int m_lastTlvertY;
    u8 m_forceAct;
    u8 m_forceMot;
    u8 m_forceAct2[5];
    u8 m_forceMot2[5];
    u8 m_forceMaxMot;
    u8 m_forceAnimSpeed;
    u8 m_forceFinishedAct;
    u8 m_forceFinishedMot;
    u8 m_forceStartMot;
    int m_isForceState;
    int m_isForceAnimLoop;
    int m_isForceAnimation;
    int m_isForceAnimFinish;
    int m_isForceState2;
    int m_isForceState3;
    u32 m_forceStateCnt;
    u32 m_forceStateEndTick;
    int m_BodyLight;
    char m_BeZero;
    char m_BodyFlag;
    u8 m_BodySin, m_BodySin2, m_BodySin3, m_BodySin4, m_BodySin5;
    u16 m_BodyTime;
    u8 m_BodyTime2, m_BodyTime3;
    u16 m_FlyMove;
    u8 m_FlyNow;
    char m_camp;
    u16 m_charfont;
    u8 m_BodyAni, m_BodyAct, m_BodyAniFrame;
    CSprRes* m_sprRes;
    CActRes* m_actRes;

    virtual void SetRenderInfo(RENDER_INFO_RECT* rect, float f1, float f2);
    virtual void SetTlvert(float x, float y);
    virtual void SetAction(int act, int mot, int type);
    virtual void ProcessMotion();
};

class CAbleToMakeEffect : public CRenderObject {
public:
    CAbleToMakeEffect();
    virtual ~CAbleToMakeEffect();

    CRagEffect* LaunchEffect(int effectId, vector3d deltaPos = vector3d{ 0.0f, 0.0f, 0.0f }, float fRot = 0.0f);
    void DetachEffects();

    int m_efId;
    int m_Sk_Level;
    int m_isLoop;
    std::list<CRagEffect*> m_effectList;
    CRagEffect* m_beginSpellEffect;
    CRagEffect* m_magicTargetEffect;
};

class CGameActor : public CAbleToMakeEffect {
public:
    CGameActor();
    virtual ~CGameActor();

    int m_moveDestX, m_moveDestY;
    int m_moveSrcX, m_moveSrcY;
    u32 m_speed;
    int m_isCounter, m_isTrickDead, m_isPlayHitWave, m_isAsuraAttack;
    char* m_emblemWnd;
    char* m_WordDisplayWnd;
    char m_hitWaveName[128];
    u32 m_colorEndTick;
    u16 m_clevel, m_MaxHp, m_Hp, m_MaxSp, m_Sp;
    int m_Exp;
    u16 m_Str, m_Int, m_Dex, m_Vit, m_Luk, m_Agi;
    vector3d m_accel;
    CPathInfo m_path;
    u32 m_moveStartTime;
    u8 m_isNeverAnimation;
    int m_pathStartCell;
    float m_dist;
    u32 m_lastProcessStateTime, m_lastServerTime, m_chatTick, m_targetGid;
    float m_attackMotion;
    int m_isBladeStop;
    u32 m_gid;
    int m_job, m_sex;
    UIBalloonText* m_balloon;
    UIChatRoomTitle* m_chatTitle;
    UIMerchantShopTitle* m_merchantShopTitle;
    UIRechargeGage* m_skillRechargeGage = nullptr;
    u32 m_freezeEndTick, m_petEmotionStartTick, m_skillRechargeEndTick = 0, m_skillRechargeStartTick = 0;
    int m_chatWidth, m_chatHeight, m_nameWidth, m_xSize, m_ySize, m_headType;
    std::list<WBA> m_willBeAttackList, m_willBeAttackedList;
    int m_willBeDead, m_is99;
    char m_99;
    int m_bodyState, m_effectState, m_healthState, m_pkState;
    int m_isSitting;
    float m_damageDestX, m_damageDestZ;
    u32 m_effectLaunchCnt, m_vanishTime;
    int m_actorType, m_bIsMemberAndVisible, m_gdid, m_emblemVersion;
    void* m_homunAI; // CMercenaryAI
    void* m_merAI;   // CMercenaryAI
    u8 m_objectType;
    _MSG2AI m_homunMsg, m_homunResMsg, m_merMsg, m_merResMsg;
    CMsgEffect* m_birdEffect;
    std::list<CMsgEffect*> m_msgEffectList;
    vector3d m_moveStartPos;
    vector3d m_moveEndPos;
    u32 m_moveEndTime;
    int m_isMoving;

    virtual u8   ProcessState();
    virtual void SendMsg(CGameObject* src, int msg, msgparam_t par1, msgparam_t par2, msgparam_t par3) override;
    virtual int  Get8Dir(float rot) override;
    virtual void SetState(int state);
    virtual void ProcessWillBeAttacked();
    virtual void SetModifyFactorOfmotionSpeed(int attackMT);
    virtual int GetAttackMotion();
    virtual void RegisterPos();
    virtual void UnRegisterPos();
    void QueueWillBeAttacked(const WBA& hitInfo);
    void DeleteMatchingEffect(CMsgEffect* effect);
    void DeleteTotalNumber(int kind);
    void ProcessSkillRechargeGageOverlay(int screenCenterX, int screenTopY, int clientHeight);
    void DestroySkillRechargeGage();
};

class CMsgEffect : public CGameObject {
public:
    CMsgEffect();
    ~CMsgEffect() override;

    u8 OnProcess() override;
    void SendMsg(CGameObject* sender, int msg, msgparam_t par1, msgparam_t par2, msgparam_t par3) override;
    void Render(matrix* viewMatrix) override;

    int m_msgEffectType;
    int m_digit;
    int m_numberValue;
    int m_sprShift;
    int m_alpha;
    u32 m_masterGid;
    u32 m_colorArgb;
    u32 m_stateStartTick;
    vector3d m_pos;
    vector3d m_orgPos;
    vector3d m_destPos;
    vector3d m_destPos2;
    float m_zoom;
    float m_orgZoom;
    CGameActor* m_masterActor;
    u8 m_isVisible;
    u8 m_isDisappear;
    u8 m_removedFromOwner;
};

void SetRuntimeActorCameraLongitude(float longitude);

class CGrannyPc : public CGameActor {
public:
    std::string m_imfName;
    int m_honor;
    int m_virtue;
    int m_headDir;
    int m_head;
    int m_headPalette;
    int m_weapon;
    int m_accessory;
    int m_accessory2;
    int m_accessory3;
    int m_shield;
    int m_shoe;
    int m_renderWithoutLayer;
    std::string m_headPaletteName;
    void* m_gage; // UIPcGage
    int m_pk_rank;
    int m_pk_total;
    C3dGrannyModelRes* m_GrannyActorRes;
    C3dGrannyModelRes* m_GrannyPartRes[10];
    granny_model_instance* m_Instance;
    void* m_rp[30]; // RPMesh
    void* m_rpPart[10][30];
    granny_pnt332_vertex* m_matVer[30];
    granny_pnt332_vertex* m_matVerPart[10][30];
    u32 m_fAniCnt[4]; // granny_system_clock
    u32 m_fLastAniCnt[4];
    float m_GameClock;
    int m_curAction;
    int m_baseAction;
    float m_fCurRot;
    u8 m_RenderAlpha;
    u32 m_nVertCol;
    int m_nLastActAnimation;
    int m_curFrame;
    char m_strJobFn[260];
    int m_nRenderType;
    void* m_pCellTex; // CTexture
    granny_control* m_Control[20];
    u8 m_isFirstProcess;
    int m_nUpdateAniFlag;
    void* m_pTex[10][30];
    void* m_pFaceArr[10][30];
    int m_nIndexNo[10][30];
    matrix m_matPose[30];
    char m_strJobSymbol[3];
    char m_strPartSymbol[8][30];
    char m_strBoneSymbol[3];
    void* m_pWorldPose; // granny_world_pose
    void* m_shadowTex; // CTexture
    std::vector<vector3d> m_shadowDotList;
    HairInfo m_hair;

    CGrannyPc();
    virtual ~CGrannyPc();
    virtual void SetAction(int act, int mot, int type) override;
    virtual void Render();
};

class CPc : public CGameActor {
public:
    CPc();
    virtual ~CPc();
    virtual void SetState(int state) override;
    virtual void SetModifyFactorOfmotionSpeed(int attackMT) override;

    void InvalidateBillboard();
    bool EnsureBillboardTexture(float cameraLongitude);
    void WarmupCommonBillboardCache();

    std::string m_imfName;
    int m_honor, m_virtue, m_headDir, m_head, m_headPalette, m_weapon;
    int m_accessory, m_accessory2, m_accessory3, m_shield, m_shoe, m_shoe_count;
    vector3d shoe_pos;
    int m_renderWithoutLayer;
    std::string m_headPaletteName;
    UIPcGage* m_gage;
    int m_pk_rank, m_pk_total;
    std::vector<CSprRes*> m_sprResList;
    std::vector<CActRes*> m_actResList;
    CTexture* m_billboardTexture;
    int m_billboardTextureOwned;
    int m_billboardTextureWidth;
    int m_billboardTextureHeight;
    int m_billboardAnchorX;
    int m_billboardAnchorY;
    tagRECT m_billboardOpaqueBounds;
    int m_cachedBillboardBodyAction;
    int m_cachedBillboardHeadMotion;
    int m_cachedBillboardJob;
    int m_cachedBillboardHead;
    int m_cachedBillboardSex;
    int m_cachedBillboardBodyPalette;
    int m_cachedBillboardHeadPalette;
    int m_cachedBillboardWeapon;
    int m_cachedBillboardShield;
    int m_cachedNonPcResourceJob;
    CActRes* m_cachedNonPcActRes;
    CSprRes* m_cachedNonPcSprRes;
};

class CPlayer : public CPc {
public:
    CPlayer();
    virtual ~CPlayer();

    void ProcessPreMove();

    int m_destCellX, m_destCellZ;
    u32 m_attackReqTime, m_preMoveStartTick;
    PROCEEDTYPE m_proceedType;
    int m_preMoveOn, m_attackMode, m_isAttackRequest, m_isWaitingMoveAck, m_isPreengageStateOfMove;
    u32 m_proceedTargetGid, m_totalAttackReqCnt, m_tickOfMoveForAttack, m_moveReqTick, m_standTick;
    int m_skillId, m_skillAttackRange, m_skillUseLevel, m_gSkillDx, m_gSkillDy, m_preengageXOfMove, m_preengageYOfMove;
    CRagEffect* m_statusEffect;
};

class CSkill : public CGameActor {
public:
    u32 m_launchCnt, m_SkillStartTime, m_aid;
    C3dActor* m_3dactor;
    CRagEffect* m_LoopEffect;
    int m_effectId;
};

class CItem : public CRenderObject {
public:
    CItem();
    ~CItem() override;

    u8 OnProcess() override;
    void Render(matrix* viewMatrix) override;
    int Get8Dir(float rot) override;

    bool EnsureBillboardTexture();
    void InvalidateBillboard();
    void TriggerDropAnimation();

    std::string m_itemName;
    std::string m_resourceName;
    u32 m_aid;
    u32 m_itemId;
    u16 m_amount;
    u16 m_tileX;
    u16 m_tileY;
    u8 m_identified;
    u8 m_subX;
    u8 m_subY;
    int m_isJumping;
    float m_sfallingSpeed, m_sPosY;
    CTexture* m_billboardTexture;
    int m_billboardTextureOwned;
    int m_billboardTextureWidth;
    int m_billboardTextureHeight;
    int m_billboardAnchorX;
    int m_billboardAnchorY;
    tagRECT m_billboardOpaqueBounds;
    int m_cachedBillboardMotion;
};
