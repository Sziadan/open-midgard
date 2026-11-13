#pragma once
#include "core/ChatRoomPackets.h"
#include "core/SourceChatRouting.h"
#include "core/Globals.h"
#include "Mode.h"
#include "audio/Audio.h"
#include <array>
#include <map>
#include <list>
#include <string>
#include <vector>

// Forward declarations
class CView;
class CMousePointer;
class CGameActor;
class CPlayer;
class UINameBalloonText;
class UIPlayerGage;
class UIRechargeGage;
class CRagConnection;
class CGameModePacketRouter;
class UIWindow;
struct GroundSkillTextRequest { std::array<int,4> values{}; const char* text=nullptr; };
struct SourceChatInputServices { ro::chat::SourceChatModifiers (*readModifiers)()=nullptr; u32 (*ticks)()=nullptr; u16 (*languageId)()=nullptr; };
struct GameModePacketLoopResult;
struct MapTransitionServices;

struct NamePair {
    std::string name;
    std::string nick;
    std::string partyName;
    std::string guildName;
    std::string guildTitle;
};

struct GuildEmblemImage {
    u32 version=0;
    std::vector<unsigned char> bitmap;
    bool decoded=false;
    std::string imageUri;
};

struct CellPos {
    int x, y;
};

struct ColorCellPos {
    int x, y;
    u32 color;
};

struct CompassMarker {
    int x, y;
    u32 color;
    u32 type;
    u32 startTime;

    bool IsExpired(u32 now) const { return type == 0 && now - startTime > 15000u; }
    bool IsVisible(u32 now) const { return !IsExpired(now) && (now - startTime) % 1000u < 500u; }
};

struct SKILL_USE_INFO {
    int id;
    int level;
    int attackRange=0;
    int useType=0;
};

struct SHOW_IMAGE_INFO {
    std::string name;
    int type = -1;
    int x, y;
};

using ChatRoomInfo = ro::chat::RoomInfo;

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
    // Character IDs (item producer/creator), distinct from visible actor IDs.
    std::string ResolveCharacterName(u32 characterId);
    bool ResolveCursorDrawPosition(int rawX, int rawY, int* drawX, int* drawY) const;
    bool m_hasCursorSnap = false;
    int m_cursorSnapRawX = 0, m_cursorSnapRawY = 0;
    int m_cursorSnapDrawX = 0, m_cursorSnapDrawY = 0;
    bool SendSkillToGroundWithText(u16 skillId, u16 skillLevel, int cellX, int cellY, const std::string& contents);
    bool SendWarpMapSelection(const std::string& mapName);
    bool CancelWarpMapSelection();
    bool SendMakingArrowSelection(u16 itemId);
    bool SendProductionSelection(u16 itemId, const std::array<u16, 3>& materials);
    bool SendCookingSelection(u16 makeType, u16 itemId);
    bool SendWeaponRefineSelection(u32 itemIndex);
    bool SendItemRepairSelection(const std::array<u8, 13>& row);
    void ClearCraftingSelection();
    void ClearItemComposition();
    void ClearItemIdentification();
    bool SendTimeSyncRequest(bool syncNow);
    bool SendAutoSpellSelection(u32 skillId, const std::vector<unsigned int>& offeredSkillIds);
    bool SendStarPlaceAcceptance(u8 which);
    void ShowStarSkillNotice(const std::string& text, u32 now);
    void UpdateStarSkillNotice(u32 now);
    void HelpBalloonDeleted(UITransBalloonText* window) override;
    void SynchronizePlayerSkillProceed(CPlayer* player,bool retire=false);
    void ResetCompanionMapState();
    void DeactivateHomunculus();
    void RetireMercenaryContract();
    void UpdateFriendInvitation();
    void CloseFriendInvitation();
    void ReceiveAdoptionInvitation(u32 parent, u32 partner, std::string name);
    void UpdateAdoptionInvitation();
    void ClearAdoptionInvitation();
    bool CanRequestAdoption(u32 candidate) const;
    bool OpenPlayerContextMenu(u32 targetAid, const std::string& targetName, int x, int y);

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
        GameMsg_RequestUseInventoryItem = 20012,
        GameMsg_RequestDropInventoryItem = 20027,
        GameMsg_ToggleSitStand = 20013,
        GameMsg_RequestUpgradeSkillLevel = 20014,
        GameMsg_RequestIncreaseStatus = 20015,
        GameMsg_RequestNpcContact = 20016,
        GameMsg_RequestNpcSelectMenu = 20017,
        GameMsg_RequestNpcNext = 20018,
        GameMsg_RequestNpcInputNumber = 20019,
        GameMsg_RequestNpcInputString = 20020,
        GameMsg_RequestNpcCloseDialog = 20021,
        GameMsg_RequestShopDealType = 20022,
        GameMsg_RequestShopBuyList = 20023,
        GameMsg_RequestShopSellList = 20024,
        GameMsg_RequestShortcutUpdate = 20025,
        GameMsg_RequestShortcutUse = 20026,
            GameMsg_RequestStorageStoreItem = 20028,
            GameMsg_RequestStorageWithdrawItem = 20029,
            GameMsg_RequestStorageClose = 20030,
            GameMsg_RequestIdentifyInventoryItem = 20031,
                GameMsg_RequestComposeCardIntoEquipment = 20032,
                GameMsg_RequestPlayerContextAction = 20033,
                GameMsg_RequestCartOff = 20039,
                GameMsg_RequestChangeCart = 20040,
                GameMsg_RequestShopAdjustDeal = 20041,
                GameMsg_RequestShopCancel = 20042,
            GameMsg_RequestPartyCreate = 20034,
            GameMsg_RequestPartyChangeOptions = 20035,
            GameMsg_RequestPartyDisband = 20036,
                GameMsg_RequestPartyInvite = 20037,
                GameMsg_RequestPartyInviteReply = 20038,
        GameMsg_RequestStandingResurrection = 20043,
        GameMsg_ConfirmCashShopContribution = 20044,
        GameMsg_RequestQuestActive = 20045,
    };

    enum PlayerContextAction {
        PlayerContext_CheckEquipmentInfo = 0,
        PlayerContext_RequestDeal,
        PlayerContext_AskJoinParty,
        PlayerContext_InviteJoinGuild,
        PlayerContext_SendWhisper,
        PlayerContext_RegisterFriend,
        PlayerContext_SendMail,
        PlayerContext_RejectWhispering,
        PlayerContext_AllowWhispering=9, // 8 is an existing commerce request action.
        PlayerContext_RequestAdoption = 10,
        PlayerContext_RequestGuildAlliance,
        PlayerContext_DeclareGuildOpposition,
    };

    CGameMode();
    bool MakeCorpse(CGameActor& actor);
    virtual ~CGameMode();

    virtual void OnInit(const char* worldName) override;
    virtual void OnExit() override;
    virtual int  OnRun() override;
    virtual void OnUpdate() override;
    // Production frame gate, kept independent of world/audio initialization.
    bool ProcessNetworkUpdate(CRagConnection& connection, CGameModePacketRouter& router,
        int packetBudget, u32 nowTick, GameModePacketLoopResult* outResult = nullptr);
    void StartNpcProgress(u32 seconds, u32 now);
    void FinishNpcProgress(CRagConnection& connection);
    void UpdateNpcProgress(CRagConnection& connection, u32 now);
    void ClearNpcProgress();
    UIRechargeGage* GetNpcProgressGauge() const { return m_npcProgressGauge; }
    // Borrowed services must remain alive through dispatch; nullptr restores defaults.
    void SetMapTransitionServices(const MapTransitionServices* services) { m_mapTransitionServices = services; }
    const MapTransitionServices* GetMapTransitionServices() const { return m_mapTransitionServices; }
    void ProcessPlayWave();
    virtual msgresult_t SendMsg(int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t extra) override;
    virtual void OnChangeState(int newState) override;
    void MakeFog(int fogOn);
    bool CanRequestStandingResurrection() const;
    // Ref/GameMode.cpp:11527. hideTime is in 24 Hz frames.
    void MakeArrowEffectPos(const vector3d& sourcePos, const vector3d& targetPos, float hideTime);
    void MakeArrowEffect(const vector3d& sourcePos, unsigned int targetGid, float hideTime, int job);
    void MakeMandraAttackEffect(unsigned int targetGid, int job, float hideTime);
    void PlaceEffect(const vector3d& pos, int effectId, int lastTime, int extra);
    void GetServerCoor(float clientX, float clientZ, int& serverX, int& serverY) const;
    void GetClientCoor(int serverX, int serverY, float& clientX, float& clientZ) const;

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
    u32 m_selectedPcTargetGid;
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
    std::map<u32, std::string> m_characterNameCache;
    std::map<u32, u32> m_characterNameReqTimer;
    std::map<u32, int> m_guildMemberStatusCache;
    std::map<u32, CellPos> m_actorPosList;
    std::map<u32, CGameActor*> m_runtimeActors;
    std::map<u32, GroundItemState> m_groundItemList;
    std::list<u32> m_pickupReqItemNaidList;
    std::map<u32, u32> m_aidList;
    std::map<u32, ColorCellPos> m_partyPosList;
    std::map<u32, ColorCellPos> m_guildPosList;
    std::map<u32, ColorCellPos> m_battlegroundPosList;
    u16 m_battlegroundCamp = 0;
    s16 m_battlegroundScoreA = 0;
    s16 m_battlegroundScoreB = 0;
    void ResetBattlegroundMapState();
    std::map<u32, CompassMarker> m_compassPosList;
    void PruneCompassMarkers(u32 now);
    
    std::vector<int> m_menuIdList;
    u16 m_warpSkillId = 0;
    std::vector<std::string> m_warpMapDestinations;
    std::list<u32> m_visibleTrapList;
    std::list<u32> m_emblemReqGdidQueue;
    std::map<u32, GuildEmblemImage> m_guildEmblems;
    std::map<u32, u32> m_emblemRequestTicks;
    u32 m_pendingEmblemGuild=0, m_pendingEmblemVersion=0, m_pendingEmblemTick=0;
    std::vector<unsigned char> m_pendingEmblemData;
    u32 m_lastEmblemReqTick;
    u32 m_lastNameWaitingListTick;
    
    std::vector<PLAY_WAVE_INFO> m_playWaveList;
    std::vector<PLAY_WAVE_INFO> m_ambientWaveList;
    std::vector<u32> m_KillerList;
    
    std::string m_lastWhisperMenuCharacterName;
    std::string m_lastWhisper;
    std::string m_lastWhisperName;
    s8& m_hiddenWhisperAcknowledgements=g_MessageHi; // Alias; constructing a mode must not reset it.
    std::string m_sourceLastSubmittedChat;
    int m_sourceSameChatRepeatCount=0;
    int m_sourceAutoSaveChatCount=0;
    bool SendSourceWhisper(const std::string& target,const std::string& text);
    std::string GetSourceChatCharset() const;
    bool SubmitSourceChat(const std::string& text,const std::string& target,UIWindow* origin=nullptr);
    msgresult_t ProcessSourceTalk(int type,const std::array<std::string,3>& parameters);
    bool SendBufferedSourceChat(int operation,const std::string& target={});
    u32 GetSourceChatTick() const;
    void SetSourceChatInputServices(const SourceChatInputServices* services) { m_sourceChatInputServices=services; }
    const SourceChatInputServices* m_sourceChatInputServices=nullptr;
    std::array<u8,6> m_SiyeonStatus{};

    bool ToggleSourceChatAutoSave();
    // Selected PE ProcessTalkType table; secondary dhxj header is shifted.
    static constexpr int SourceTalk_AutoMessage=152;
    bool ToggleSourceAutoMessage();
    bool OpenPetContextMenu(CGameActor* actor,int logicalX,int logicalY);
    bool OpenHomunculusContextMenu(CGameActor* actor,int logicalX,int logicalY);
    bool OpenMercenaryContextMenu(CGameActor* actor,int logicalX,int logicalY);
    bool WriteSourceChat(const std::string& text);
    std::string GetSourceAutoSaveChatPath() const;
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
    bool SendChatRoomSettings(const ro::chat::RoomInfo& info,bool change);
    bool RequestEnterChatRoom(u32 roomId,const char* password,u32 now);
    u32 m_reqTickChatRoom;
    u32 m_reqTickMerchantShop;
    int m_isReqEmotion;
    float m_fixedLongitude;
    u32 m_lastCouplePacketAid;
    u32 m_lastCouplePacketGid;
    std::string m_recallPartnerName;
    u32 m_pvpRankStartTick = 0;
    struct AuctionSearchRow {
        u32 auctionId = 0;
        std::string seller;
        std::string buyer;
        u16 itemId = 0;
        u32 itemType = 0;
        u16 amount = 0;
        bool identified = false;
        bool damaged = false;
        u8 refine = 0;
        std::array<u16, 4> cards{};
        u32 price = 0;
        u32 buyNow = 0;
        u32 expiresAt = 0;
    };
    enum class AuctionSearchKind : u16 { Armor, Weapon, Card, Etc, Name, Price, OwnSales, OwnBids };
    struct AuctionSearchQuery {
        AuctionSearchKind kind = AuctionSearchKind::Name;
        u16 page = 1;
        u32 maximumPrice = 0;
        std::string name;
    };
    bool RequestAuctionSearch(const AuctionSearchQuery& query);
    void CloseAuctionSearch();
    bool RequestAuctionStageItem(u16 inventoryIndex);
    bool CancelAuctionStaging();
    bool ApplyAuctionStageResult(u16 index, u8 result);
    enum class AuctionTransaction { None, Register, Bid, Cancel, Stop };
    bool RequestAuctionRegistration(u32 startingPrice, u32 buyNow, u16 hours);
    bool RequestAuctionTransaction(AuctionTransaction action, const AuctionSearchRow& quote, u32 bid = 0);
    void ApplyAuctionTransactionResult(u16 opcode, u16 result);
    AuctionTransaction m_auctionTransaction = AuctionTransaction::None;
    struct AuctionStagingState {
        u16 pendingIndex = 0;
        u32 pendingItemId = 0;
        u16 stagedIndex = 0;
        u32 stagedItemId = 0;
        bool discardReply = false;
    } m_auctionStaging;
    struct AuctionSearchState {
        bool open = false;
        bool awaitingReply = false;
        bool discardReply = false;
        u32 pages = 0;
        AuctionSearchQuery query;
        std::vector<AuctionSearchRow> rows;
    } m_auctionSearch;
    struct AdoptionInvitation {
        u32 parent = 0;
        u32 partner = 0;
        std::string name;
        bool reviewingConsequences = false;
    } m_adoptionInvitation;
    
    UINameBalloonText* m_nameBalloon;
    UINameBalloonText* m_targetNameBalloon;
    UITransBalloonText* m_starSkillNotice;
    u32 m_starSkillNoticeTick;
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
    std::map<u32,u32> m_chatRoomOwners;
    SKILL_USE_INFO m_skillUseInfo;
    SHOW_IMAGE_INFO m_showImageInfo;
    std::string m_lastChat;
    int m_sameChatRepeatCnt;
    int m_numNotifyTime;
    int m_isCheckGndAlpha;
    int m_lastCardItemIndex;
    unsigned m_pendingIdentifyIndex = 0;
    unsigned m_pendingIdentifyItemId = 0;
    struct ItemCompositionRequest {
        unsigned cardIndex=0, equipmentIndex=0, cardId=0, equipmentId=0;
        std::array<int,4> equipmentCards{};
        u64 completionSync = 0;
    } m_pendingItemComposition;
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
    u32 m_skillChaseTargetGid;
    u32 m_lastSkillChaseRequestTick;
    int m_skillChaseSkillId;
    int m_skillChaseSkillLevel;
    int m_skillChaseRange;
    int m_hasSkillChase;
    int m_groundSkillCellX;
    int m_groundSkillCellY;
    int m_groundSkillId;
    int m_groundSkillLevel;
    int m_groundSkillRange;
    int m_hasGroundSkillChase;
    u32 m_touchSkillTargetAid;
    int m_touchSkillId;
    int m_touchSkillLevel;
    int m_touchSkillRange;
    int m_hasTouchSkillChase;
    int m_mapLoadingStage;
    u32 m_mapLoadingStartTick;
    u32 m_mapLoadingAckTick;
    u32 m_lastActorBootstrapPacketTick;
    u32 m_worldProcessTick;
    float m_worldProcessCarryMs;
    u32 m_lastHoverRefreshTick;
    std::string m_loadingWallpaperName;
    bool m_pendingPartyCreateExpShare;
    std::string m_pendingPartyCreateName;
    std::vector<CGameActor*> m_deferredActorDeletes;

private:
    UIRechargeGage* m_npcProgressGauge = nullptr;
    u32 m_npcProgressStartTick = 0;
    u32 m_npcProgressDuration = 0;
    bool m_terminalNetworkHandled = false;
    u64 m_networkModeOperationGeneration = 0;
    const MapTransitionServices* m_mapTransitionServices = nullptr;
};

struct PlayerVitalsScreenMetrics {
    int barWidth = 0;
    int barHeight = 0;
    int border = 0;
    int namePadding = 0;
    int totalHeight = 0;
    int topDelta = 0;
    int hoverNameExtra = 0;
    int qtNameExtra = 0;
};

PlayerVitalsScreenMetrics ResolvePlayerVitalsScreenMetrics(const CView* view);
// True when the actor draws the dual HP/SP overlay (local player or owned companion).
bool ActorHasPlayerStyleVitalsOverlay(const CGameActor* actor);
// Qt/HDC nameplate offset so hover names sit just below those bars at any zoom.
int ResolvePlayerStyleVitalsNameOffsetY(const CView* view);

bool ArmPendingSkillUseFromSkillList(int skillId);
bool ArmPendingSkillUseFromSkillList(int skillId,int family);
