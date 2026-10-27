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
class CGameActor;
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

// 2008 WBA is 40 bytes. attackedSpeedFactor sits at offset 32 (HighPriest message 27
// writes a float there). targetGid holds the attacker's gid. Message 27/131/134
// overwrite that float's bits with the running damage and change message to
// 111/130/133 when the running damage is non-zero.
struct WBA {
    int message = 0;
    int damage = 0;
    u32 time = 0;
    u32 targetGid = 0;
    int tCellX = -1;
    int tCellY = -1;
    int effectId1 = -1;
    int effectId2 = -1;
    float attackedSpeedFactor = 1.0f;
    int effectLevel = 1;
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
    virtual void OnActorDeleted(const CGameActor* actor) {}
    virtual void SendMsg(CGameObject* src, int msg, msgparam_t par1, msgparam_t par2, msgparam_t par3) {}
    virtual void Render(matrix* m) {}
    virtual int  Get8Dir(float rot);
};

class CRenderObject : public CGameObject {
public:
    CRenderObject();

    void SetForceAnimation(int act, int mot, int maxMot, int finishAct, int finishMot, int isAnim, u32 endTick, int isLoop);
    void SetForceAnimEndTick(u32 tick);
    void ProcessForcedMotion();
    void SetArgb(int alpha, int red, int green, int blue);

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
    void RemoveEffectById(int effectId);
    void SendMsg(CGameObject* sender, int msg, msgparam_t par1, msgparam_t par2, msgparam_t par3) override;

    int m_efId;
    int m_Sk_Level;
    int m_isLoop;
    std::list<CRagEffect*> m_effectList;
    CRagEffect* m_beginSpellEffect;
    CRagEffect* m_magicTargetEffect;
};

class CDummyEffectLauncher : public CAbleToMakeEffect {
public:
    u8 OnProcess() override;
    void SendMsg(CGameObject* sender, int msg, msgparam_t par1, msgparam_t par2, msgparam_t par3) override;
    void Initialize(const vector3d& pos, int effectId, int lastTime, int extra);

private:
    int m_lastCnt = 300;
};

class CGroundScope : public CAbleToMakeEffect {
public:
    CGroundScope();

    u8 OnProcess() override;
    void Render(matrix* viewMatrix) override;
    void SendMsg(CGameObject* sender, int message, msgparam_t arg1, msgparam_t arg2, msgparam_t arg3) override;

private:
    int GetScopeSize(int skillId) const;

    u32 m_masterGid = 0;
    int m_groundPosX = 0;
    int m_groundPosY = 0;
    u32 m_lastingTime = 0;
    int m_scopeSize = 1;
    float m_rot = 0.0f;
    float m_rotSpeed = 0.0f;
};

class CGameActor : public CAbleToMakeEffect {
public:
    CGameActor();
    virtual ~CGameActor();

    void SetChatBubbleText(const std::string& text, u32 untilTick);
    void ClearChatBubbleText();
    bool HasActiveChatBubble(u32 now) const;
    const std::string& GetChatBubbleText() const { return m_chatBubbleText; }
    void SetGuildInfo(int guildId, int emblemVersion);

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
    u32 m_hideFadeStartTick, m_hideFadeEndTick;
    int m_actorType, m_bIsMemberAndVisible, m_gdid, m_emblemVersion;
    void* m_homunAI; // CMercenaryAI
    void* m_merAI;   // CMercenaryAI
    u8 m_objectType;
    _MSG2AI m_homunMsg, m_homunResMsg, m_merMsg, m_merResMsg;
    CMsgEffect* m_birdEffect;
    CMsgEffect* m_iceEffect;
    CMsgEffect* m_curseEffect;
    CMsgEffect* m_sleepEffect;
    CMsgEffect* m_stunEffect;
    CMsgEffect* m_silenceEffect;
    CMsgEffect* m_angelusEffect;
    CMsgEffect* m_cartEffect;
    CMsgEffect* m_lastDamageNumberEffect;
    CMsgEffect* m_chatProhibitEffect = nullptr;
    CMsgEffect* m_pkEffect = nullptr;
    std::list<CMsgEffect*> m_msgEffectList;
    vector3d m_moveStartPos;
    vector3d m_moveEndPos;
    u32 m_moveEndTime;
    int m_isMoving;
    std::string m_chatBubbleText;

    virtual u8   ProcessState();
    virtual void SendMsg(CGameObject* src, int msg, msgparam_t par1, msgparam_t par2, msgparam_t par3) override;
    virtual int  Get8Dir(float rot) override;
    virtual void SetState(int state);
    virtual void ProcessWillBeAttacked();
    virtual void SetModifyFactorOfmotionSpeed(int attackMT);
    virtual int GetAttackMotion();
    virtual int GetWeapon() const { return -1; } // Ref/GameActor.cpp:2194
    virtual void RegisterPos();
    virtual void UnRegisterPos();
    void StartHideFade(u32 durationMs);
    void CancelHideFade();
    bool IsHideFadeActive(u32 now) const;
    u32 GetHideFadeAlpha(u32 now) const;
    void QueueWillBeAttacked(const WBA& hitInfo);
    void EnqueueWillBeAttacked(WBA* hit, int runningDamage, int forcedMessage);
    void HandleAttacked(const WBA& hit);
    void Am_Action(const int* arg);
    void Am_Skill(const int* arg);
    void Am_Groundskill(const int* arg);
    void StartMoveToCell(int cellX, int cellY, int pathFlag);
    void SetAttrState(int bodyState, int effectState, int healthState, bool changedNow);
    CMsgEffect* SpawnMsgSprite(int processType, const char* spritePath, const char* actPath, int action, float zoom);
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
    void OnActorDeleted(const CGameActor* actor) override;
    void SendMsg(CGameObject* sender, int msg, msgparam_t par1, msgparam_t par2, msgparam_t par3) override;
    void Render(matrix* viewMatrix) override;

    int m_msgEffectType;
    int m_digit;
    int m_numberValue;
    int m_sprShift;
    int m_spriteActionIndex;
    int m_spriteMotionIndex;
    int m_lastProcessedSpriteMotionIndex;
    int m_alpha;
    u32 m_masterGid;
    u32 m_colorArgb;
    u32 m_stateStartTick;
    vector3d m_pos;
    vector3d m_orgPos;
    vector3d m_destPos;
    vector3d m_destPos2;
    vector3d m_followRenderPos;
    float m_zoom;
    float m_orgZoom;
    float m_followTravelDistance;
    u32 m_followLastRenderFrame;
    u32 m_followRenderTick;
    std::string m_customSpritePath;
    std::string m_customActPath;
    CGameActor* m_masterActor;
    u8 m_isVisible;
    u8 m_isDisappear;
    u8 m_removedFromOwner;
    u8 m_followInitialized;
    u8 m_followRenderInitialized;
    u8 m_followRenderAfterOwner;
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
    int GetWeapon() const override { return m_weapon; } // Ref/Pc.cpp:2454
    virtual ~CGrannyPc();
    virtual void SetAction(int act, int mot, int type) override;
    virtual void Render();
};

class CPc : public CGameActor {
public:
    CPc();
    int GetWeapon() const override { return m_weapon; } // Ref/Npc.cpp:859
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

struct SkillStandEntry {
    u32 aid = 0;
    u32 creatorAid = 0;
    int job = 0;
    int cellX = 0;
    int cellY = 0;
    u8 isVisible = 0;
};

class CSkill : public CGameActor {
public:
    CSkill();
    ~CSkill() override;

    u8 OnProcess() override;
    void Render(matrix* viewMatrix) override;
    void SendMsg(CGameObject* sender, int message, msgparam_t arg1, msgparam_t arg2, msgparam_t arg3) override;

    u32 m_launchCnt = 0;
    u32 m_SkillStartTime = 0;
    u32 m_aid = 0;
    C3dActor* m_3dactor = nullptr;
    CRagEffect* m_LoopEffect = nullptr;
    int m_effectId = -1;

private:
    void HideModel();
    void ShowModel(const char* modelName);
    const char* TrapModelName() const;
    void ApplyStandEntry(const SkillStandEntry& entry);
    void LaunchPeriodic(int effectId, int subtract);
    void LaunchLoop(int effectId);
    void PlayUnitWave(const char* name) const;
    bool EffectExists(int effectId) const;
};

class CGraffiSkill : public CSkill {
public:
    CGraffiSkill();
    ~CGraffiSkill() override;

    u8 OnProcess() override;
    void Render(matrix* viewMatrix) override;
    void SendMsg(CGameObject* sender, int message, msgparam_t arg1, msgparam_t arg2, msgparam_t arg3) override;

private:
    void OnInit();

    int m_offsX = 0;
    int m_offsY = 0;
    int m_groundPosX = 0;
    int m_groundPosY = 0;
    u32 m_lastingTime = 0;
    float m_rot = 0.0f;
    CTexture* m_tex = nullptr;
    char m_strMsg[80];
};

// Ref/Skill.cpp CArrowEffect (0x689150). Message 14 aims at an actor id.
// Message 64 aims at a world position. Skill 47 uses message 64.
class CArrowEffect : public CRenderObject {
public:
    CArrowEffect();
    ~CArrowEffect() override;

    u8 OnProcess() override;
    void Render(matrix* viewMatrix) override;
    void SendMsg(CGameObject* sender, int message, msgparam_t arg1, msgparam_t arg2, msgparam_t arg3) override;

    // The 2008 SendMsg has a fourth argument (monster job). This client's
    // message signature does not, so MakeArrowEffect stores the job here first.
    void SetSpriteJob(int job);

private:
    void ApplySpriteForJob(int job);
    void LoadSpriteResources();
    bool EnsureBillboard();

    vector3d m_sPos;
    vector3d m_tPos;
    vector3d m_moveSpeed;
    float m_hideTime;
    float m_fastFactor;
    u32 m_targetGid;
    int m_process;
    int m_spriteJob;
    std::string m_sprName;
    std::string m_actName;
    CTexture* m_billboardTexture;
    int m_cachedBillboardMotion;
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
