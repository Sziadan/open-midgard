#pragma once
#include "Mode.h"
#include "audio/Audio.h"
#include <map>
#include <list>

// Forward declarations
class CView;
class CMousePointer;
class CGameActor;
class UINameBalloonText;
class UIPlayerGage;

struct NamePair {
    std::string name;
    std::string nick;
};

struct CellPos {
    int x, y;
};

struct ColorCellPos {
    int x, y;
    u32 color;
};

struct ColorCellPos2 {
    int x, y;
    u32 color;
    u32 color2;
};

struct SKILL_USE_INFO {
    int id;
    int level;
};

struct SHOW_IMAGE_INFO {
    std::string name;
    int x, y;
};

struct ChatRoomInfo {
    int id;
    std::string title;
};

struct GroundItemState {
    u32 objectId = 0;
    u32 itemId = 0;
    u16 amount = 0;
    u16 tileX = 0;
    u16 tileY = 0;
    u8 identified = 0;
    u8 subX = 0;
    u8 subY = 0;
    u8 pendingDropAnimation = 0;
};

//===========================================================================
// CGameMode  –  Handles the main in-game world state
//===========================================================================
class CGameMode : public CMode {
public:
    enum MapLoadingStage {
        MapLoading_None = 0,
        MapLoading_PresentScreen,
        MapLoading_BootstrapAssets,
        MapLoading_CreateView,
        MapLoading_SendLoadAck,
        MapLoading_AwaitActors,
    };

    enum GameInputMessage {
        GameMsg_LButtonDown = 20000,
        GameMsg_LButtonUp = 20001,
        GameMsg_MouseMove = 20002,
        GameMsg_RButtonDown = 20003,
        GameMsg_RButtonUp = 20004,
        GameMsg_MouseWheel = 20005,
        GameMsg_SubmitChat = 20006,
        GameMsg_RequestReturnToCharSelect = 20007,
        GameMsg_RequestExitToWindows = 20008,
        GameMsg_RequestReturnToSavePoint = 20009,
        GameMsg_RequestEquipInventoryItem = 20010,
        GameMsg_RequestUnequipInventoryItem = 20011,
    };

    CGameMode();
    virtual ~CGameMode();

    virtual void OnInit(const char* worldName) override;
    virtual void OnExit() override;
    virtual int  OnRun() override;
    virtual void OnUpdate() override;
    virtual int  SendMsg(int msg, int wparam, int lparam, int extra) override;
    virtual void OnChangeState(int newState) override;

    // Memory layout from HighPriest.exe.h:30992
    int m_areaLeft, m_areaRight, m_areaTop, m_areaBottom;
    char m_rswName[40];
    char m_minimapBmpName[60];
    CWorld* m_world;
    CView* m_view;
    CMousePointer* m_mousePointer;
    u32 m_leftBtnClickTick;
    int m_oldMouseX, m_oldMouseY;
    int m_rBtnClickX, m_rBtnClickY;
    u32 m_lastRButtonClickTick;
    int m_lastRButtonClickX, m_lastRButtonClickY;
    int m_rButtonDragged;
    u32 m_lastPcGid, m_lastMonGid, m_lastLockOnMonGid;
    int m_isAutoMoveClickOn;
    int m_isWaitingWhisperSetting;
    int m_isWaitingEnterRoom;
    int m_isWaitingAddExchangeItem;
    u32 m_waitingWearEquipAck;
    u32 m_waitingTakeoffEquipAck;
    int m_isReqUpgradeSkillLevel;
    int m_exchangeItemCnt;
    int m_isWaitingCancelExchangeItem;
    std::string m_refuseWhisperName;
    std::string m_streamFileName;
    std::string m_lastExchangeCharacterName;
    
    std::map<u32, NamePair> m_actorNameList;
    std::map<u32, u32> m_actorNameReqTimer;
    std::map<u32, NamePair> m_actorNameListByGID;
    std::map<u32, u32> m_actorNameByGIDReqTimer;
    std::map<u32, int> m_guildMemberStatusCache;
    std::map<u32, CellPos> m_actorPosList;
    std::map<u32, CGameActor*> m_runtimeActors;
    std::map<u32, GroundItemState> m_groundItemList;
    std::list<u32> m_pickupReqItemNaidList;
    std::map<u32, u32> m_aidList;
    std::map<u32, ColorCellPos> m_partyPosList;
    std::map<u32, ColorCellPos> m_guildPosList;
    std::map<u32, ColorCellPos2> m_compassPosList;
    
    std::vector<int> m_menuIdList;
    std::list<u32> m_visibleTrapList;
    std::list<u32> m_emblemReqGdidQueue;
    u32 m_lastEmblemReqTick;
    u32 m_lastNameWaitingListTick;
    
    std::vector<PLAY_WAVE_INFO> m_playWaveList;
    std::vector<u32> m_KillerList;
    
    std::string m_lastWhisperMenuCharacterName;
    std::string m_lastWhisper;
    std::string m_lastWhisperName;
    int m_noMove;
    u32 m_noMoveStartTick;
    int m_isOnQuest;
    int m_isPlayerDead;
    int m_canRotateView;
    int m_hasViewPoint;
    s16 ViewPointData[9];
    
    int m_receiveSyneRequestTime;
    u32 m_syncRequestTime;
    u32 m_usedCachesUnloadTick;
    u32 m_reqEmotionTick;
    u32 m_reqTickChatRoom;
    u32 m_reqTickMerchantShop;
    int m_isReqEmotion;
    float m_fixedLongitude;
    u32 m_lastCouplePacketAid;
    u32 m_lastCouplePacketGid;
    char m_CoupleName[24];
    
    UINameBalloonText* m_nameBalloon;
    UINameBalloonText* m_targetNameBalloon;
    UITransBalloonText* m_broadcastBalloon;
    UIPlayerGage* m_playerGage;
    UITransBalloonText* m_skillNameBalloon;
    UITransBalloonText* m_skillMsgBalloon;
    UITransBalloonText* m_skillUsedMsgBalloon;
    
    u32 m_skillUsedTick;
    u32 m_broadCastTick;
    int m_nameDisplayed;
    int m_nameDisplayed2;
    int m_waitingUseItemAck;
    int m_waitingItemThrowAck;
    int m_waitingReqStatusAck;
    u32 m_nameActorAid;
    u32 m_lastNaid;
    u32 m_menuTargetAID;
    int m_nameBalloonWidth;
    int m_nameBalloonHeight;
    int m_dragType;
    DRAG_INFO m_dragInfo;
    ChatRoomInfo m_lastChatroomInfo;
    SKILL_USE_INFO m_skillUseInfo;
    SHOW_IMAGE_INFO m_showImageInfo;
    std::string m_lastChat;
    int m_sameChatRepeatCnt;
    int m_numNotifyTime;
    int m_isCheckGndAlpha;
    int m_lastCardItemIndex;
    int m_SkillBallonSkillId;
    u32 m_nameBalloonType;
    u32 m_showTimeStartTick;
    int m_recordChatNum;
    std::string m_recordChat[11];
    u32 m_recordChatTime[11];
    int m_strikeNum;
    u32 m_strikeTime[3];
    u32 m_doritime[6];
    int m_isCtrlLock;
    int m_sentLoadEndAck;
    int m_isLeftButtonHeld;
    int m_lastMoveRequestCellX;
    int m_lastMoveRequestCellY;
    int m_heldMoveTargetCellX;
    int m_heldMoveTargetCellY;
    int m_hasHeldMoveTarget;
    u32 m_lastMoveRequestTick;
    u32 m_lastAttackRequestTick;
    u32 m_lastPickupRequestTick;
    u32 m_lastAttackChaseHintTick;
    u32 m_attackChaseTargetGid;
    int m_attackChaseTargetCellX;
    int m_attackChaseTargetCellY;
    int m_attackChaseSourceCellX;
    int m_attackChaseSourceCellY;
    int m_attackChaseRange;
    int m_hasAttackChaseHint;
    int m_mapLoadingStage;
    u32 m_mapLoadingStartTick;
    u32 m_mapLoadingAckTick;
    u32 m_lastActorBootstrapPacketTick;
    std::string m_loadingWallpaperName;
};
