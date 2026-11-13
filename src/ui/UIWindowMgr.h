#pragma once
#include "render/DC.h"
#include "UIWindow.h"
#include "UISubChatWnd.h"
#include "core/ChatNotice.h"
#include "ItemInfoSource.h"
#include <array>
#include <map>

class UIGroundSkillTextWnd;

constexpr int kShortcutKeyBindingCount = 27;
constexpr unsigned char kShortcutKeyModifierControl = 1u << 0;
constexpr unsigned char kShortcutKeyModifierShift = 1u << 1;
constexpr unsigned char kShortcutKeyModifierAlt = 1u << 2;

struct ShortcutKeyBinding {
    int virtualKey = 0;
    unsigned char modifiers = 0;
};

struct InventoryWindowInfo {
    int x = 0;
    int y = 120;
    int width = 280;
    int height = 120;
    int originalHeight = 120;
    int tab = 0;
    int radio1 = 3;
    int radio2 = 3;
    int radio3 = 4;
};

struct ItemDetailWindowInfo {
    int x = 280;
    int y = 126;
    int width = 280;
    int height = 120;
};

// Forward declarations for all windows managed by UIWindowMgr
class UILoadingWnd;
class UIRoMapWnd;
class UIMinimapZoomWnd;
class UIStatusWnd;
class UINewChatWnd;
class UIChatRoomWnd;
class UIWhisperWnd;
class UIChatRoomMenuWnd;
class UIAutoMessageWnd;
class UIFriendOptionWnd;
class UIChatRoomSettingsWnd;
class UIChatRoomMessageBox;
class UIPetEggListWnd;
class UIChatRoomPasswordWnd;
class UILoginWnd;
class UISelectServerWnd;
class UISelectCharWnd;
class UIMakeCharWnd;
class UIWaitWnd;
class UIItemWnd;
class UIStorageWnd;
class UIItemInfoWnd;
class UINotifyItemObtainWnd;
class UIItemCollectionWnd;
class UIItemCompositionWnd;
class UIItemIdentifyWnd;
class UISkillSelectWnd;
class UIMonsterInfoWnd;
class UIQuestWnd;
class UIBasicInfoWnd;
class UINotifyLevelUpWnd;
class UINotifyJobLevelUpWnd;
class UIEquipWnd;
class UISkillDescribeWnd;
class UISkillListWnd;
class UIOptionWnd;
class UIShortCutWnd;
class UIItemDropCntWnd;
class UISayDialogWnd;
class UINpcMenuWnd;
class UIPlayerContextMenuWnd;
class UIPetInfoWnd;
class UICompanionStatusWnd;
class UICompanionSkillListWnd;
class UIPetTamingDeceiveWnd;
class UIJoinPartyAcceptWnd;
class UIMessengerGroupWnd;
class UIPartyOptionWnd;
class UIGuildWnd;
class UINpcInputWnd;
class UIChooseWnd;
class UIChooseSellBuyWnd;
class UISelectCartWnd;
class UIItemShopWnd;
class UIItemPurchaseWnd;
class UIItemSellWnd;
class CBitmapRes;
class CSurface;
struct ITEM_INFO;
struct PLAYER_SKILL_INFO;

struct CSnapInfo {
    int x, y;
};

struct UIChatEvent {
    std::string text;
    u32 color;
    u8 channel;
    u32 tick;
    ro::chat::NoticeSource source{};
};

//===========================================================================
// UIWindowMgr  –  The primary UI controller
//===========================================================================
class UIWindowMgr {
public:
    // Window IDs (Ref-faithful, from decompiled address space)
    enum WindowId {
        WID_TALKBOXTRAPINPUTWND = 84,
        WID_GRAFFISTRBOXWND = 101,
        WID_AUTOMESSAGEWND = 116,
        WID_FRIENDOPTIONWND = 68,
        WID_SUBCHATST = 137,
        WID_SUBCHATBT = 138,
        WID_SUBCHATMINI = 140,
        WID_BATTLEMSGOPTION = 139,
        WID_CHATROOMMAKE = 27,
        WID_CHATROOMCHANGE = 30,
        WID_CHATROOMWND = 28,
        WID_CHATROOMPASSWORD = 29,
        WID_BASICINFOWND      = 0,
        WID_LOGINWND          = 1,
        WID_SELECTSERVERWND   = 3,
        WID_SELECTCHARWND     = 4,
        WID_MAKECHARWND       = 5,
        WID_WAITWND           = 7,
        WID_ITEMWND           = 8,
        WID_NOTICECONFIRMWND  = 8,
        WID_ITEMINFOWND       = 24,
        WID_CARDINFOWND       = 82,
        WID_NOTIFYITEMOBTAINWND = 58,
        WID_ITEMCOLLECTIONWND = 25,
        WID_SKILLDESCRIBEWND  = 26,
        WID_ITEMIDENTIFYWND   = 0x49,
        WID_ITEMCOMPOSITIONWND = 0x4A,
        WID_SAYDIALOGWND      = 9,
        WID_LOADINGWND        = 10,
        WID_STATUSWND         = 11,
        WID_NPCMENUWND        = 12,
        WID_JOINPARTYACCEPTWND = 0x23,
        WID_PLAYERCONTEXTMENUWND = 0x94,
        WID_MESSENGERGROUPWND = 0x22,
        WID_PARTYOPTIONWND    = 0x95,
        WID_GUILDWND          = 0x96,
        WID_OPTIONWND         = 13,
        WID_TIPOFTHEDAYWND    = 38,
        WID_COMMANDLISTWND    = 112,
        WID_PETINFOWND        = 88,
        WID_HOMUNINFOWND      = 113,
        WID_HOMUNSKILLLISTWND = 114,
        WID_MERINFOWND        = 132,
        WID_MERSKILLLISTWND   = 133,
        WID_SELECTPETEGGWND   = 90,
        WID_PETTAMINGDECEIVEWND = 91,
        WID_EQUIPWND          = 14,
        WID_SKILLLISTWND      = 15,
        WID_SPELLLISTWND      = 99,
        WID_NPCINPUTWND       = 16,
        WID_CHOOSESELLBUYWND  = 18,
        WID_ITEMSHOPWND       = 19,
        WID_ITEMPURCHASEWND   = 20,
        WID_NOTIFYLEVELUPWND  = 21,
        WID_ITEMSELLWND       = 22,
        WID_SHORTCUTWND       = 23,
        WID_SELECTCARTWND     = 0x5F,
        WID_SKILLSELECTWND    = 0x60,
        WID_MONSTERINFOWND    = 77,
        WID_STORAGEWND        = 50,
        WID_CHOOSEWND         = 17,
        WID_ROMAPWND          = 0x93,
        WID_NOTIFYJOBLEVELUPWND = 49,
    };

    UIWindowMgr();
    ~UIWindowMgr();

    bool Init();
    RECT GetChatClientRect() const;
    void ShowRoomValidationMessage(int messageId);
    bool OpenChatRoomMake(bool toggle=false);
    bool OpenWhisperWindow(const std::string& name,const char* label=nullptr);
    bool AddWhisperChatToWhisperWnd(const std::string& name,const std::string& text,u32 colorRef);
    void AddWhisperCharName(const std::string& name);
    bool IsChatAuxiliaryAtPoint(int x, int y) const;
    void ShowChatWindow(bool show);
    void LoadChatWindowPreferences();
    void SaveChatWindowPreferences() const;
    void InitializeChatAuxiliaries();
    void UpdateChatAuxiliaryVisibility();
    int GetInventoryTabSelection();
    void SetInventoryTabSelection(int tab);
    InventoryWindowInfo GetInventoryWindowInfo();
    void StoreInventoryWindowInfo(const InventoryWindowInfo& info);
    void CloseInventoryWindow();
    bool GetInventoryWindowShowState();
    ItemDetailWindowInfo GetItemDetailWindowInfo(ItemDetailKind kind = ItemDetailKind::Main);
    void RestoreItemDetailWindowPlacement(int* x, int* y, ItemDetailKind kind = ItemDetailKind::Main);
    void StoreItemDetailWindowInfo(const ItemDetailWindowInfo& info, ItemDetailKind kind = ItemDetailKind::Main);
    void ShowCardInfoWindow(const ITEM_INFO& item, ItemInfoAction action = ItemInfoAction::Select);
    void ShowItemObtainNotification(const ITEM_INFO& item);
    void Reset();
    void OnProcess();
    void OnDraw();
    void InvalidateUpdateNeededUI();
    void DrawVisibleWindowsToHdc(HDC targetDC, bool includeRoMap, bool includeGroundPrompts = true);
#if RO_PLATFORM_WINDOWS
    bool BuildGroundSkillPromptOverlay(ArgbDibSurface& surface, RECT client, u32 tick, RECT& bounds);
#endif
    bool HasDirtyVisualState() const;
    bool HasDirtyVisualStateExcludingRoMap() const;
    bool HasRoMapDirtyVisualState() const;
    void ClearDirtyVisualState();
    void ClearDirtyVisualStateExcludingRoMap();
    void OnDrawExcludingRoMapToHdc(HDC targetDC, bool includeGroundPrompts = true);
    bool DrawRoMapToHdc(HDC targetDC, int x, int y);
    bool GetRoMapRect(RECT* outRect) const;
    void RenderWallPaper();
    void DrawWallpaperToDC(HDC targetDC, int width, int height);
    void SetWallpaper(CBitmapRes* bitmap);
    bool SetWallpaperFromGameData(const std::string& wallpaperName);
    void ShowLoadingScreen(const std::string& wallpaperName, const std::string& message, float progress);
    void UpdateLoadingScreen(const std::string& message, float progress);
    void HideLoadingScreen();
    void SendMsg(int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t noticeType = 0, msgparam_t talker = 0);
    void PushSourceChatEvent(const char* text, u32 uiRgb, u8 channel, ro::chat::NoticeRoute route,
        ro::chat::NoticeType type, const char* talker = nullptr, u32 tick = 0);
    const std::vector<SubChatRow>& GetSourceChatBacklog() const { return m_sourceChatBacklog; }
    unsigned long long GetSourceChatGeneration() const { return m_sourceChatGeneration; }
    void PushChatEvent(const char* text, u32 color, u8 channel, u32 tick = 0, ro::chat::NoticeSource source = {});
    void SetLoginStatus(const std::string& status);
    void SetLoginWallpaper(const std::string& wallpaperName);
    const std::string& GetLoginStatus() const;
    const std::vector<UIChatEvent>& GetChatEvents() const;
    std::vector<UIChatEvent> GetChatPreviewEvents(size_t maxCount) const;
    void ClearChatEvents();
    void SetChatMsg(const std::string& text) { m_chatMsg.assign(text.c_str()); }
    const char* GetChatMsg() const { return m_chatMsg.c_str(); }
    void ClearChatMsg() { m_chatMsg.clear(); }
    std::string m_chatMsg;
    UIWindow* m_commandListWnd=nullptr;
    UIPetInfoWnd* m_petInfoWnd=nullptr;
    UICompanionStatusWnd* m_homunInfoWnd = nullptr;
    UICompanionStatusWnd* m_merInfoWnd = nullptr;
    UICompanionSkillListWnd* m_homunSkillListWnd = nullptr;
    UICompanionSkillListWnd* m_merSkillListWnd = nullptr;
    SubChatWindowInfo m_petInfoWindowInfo{150,100,280,200};
    UIWindow* m_tipOfTheDayWnd=nullptr;
    bool m_tipStartupChecked=false;
    bool OpenSourceStartupTip();
    void InitializeSourceCommandFlags();

    UIWindow* MakeWindow(int windowId);
    bool ContainsWindow(const UIWindow* window) const;
    bool ProcessSourceCommandWindowKey(int virtualKey);
    bool ToggleWindow(int windowId);
    void RemoveAllWindows();
    void DeleteWindow(UIWindow* window);
    void CloseCompanionWindows(bool homunculus);
    void PostQuit(UIWindow* window);
    void ProcessSourceHoverFrame(int logicalX,int logicalY);
    void AddWindowFront(UIWindow* window);

    // Input routing
    void OnLBtnDown(int x, int y);
    void OnLBtnDblClk(int x, int y);
    void OnLBtnUp(int x, int y);
    void OnRBtnDown(int x, int y);
    void OnRBtnUp(int x, int y);
    void OnMouseMove(int x, int y);
    bool OnWheel(int x, int y, int delta);
    void OnChar(char c);
    UIGroundSkillTextWnd* GetFocusedGroundSkillText() const;
    UIEditCtrl* GetFocusedCompanionNameEdit() const;
    void RefreshGroundPromptSkins();
    void OnKeyDown(int virtualKey);
    bool OnQtKeyDown(int virtualKey, bool isAltDown, bool isCtrlDown, bool isShiftDown);
    ShortcutKeyBinding GetShortcutKeyBinding(int slotIndex) const;
    std::string GetShortcutKeyBindingLabel(int slotIndex) const;
    bool SetShortcutKeyBinding(int slotIndex, const ShortcutKeyBinding& binding);
    bool IsCapturingShortcutKeyBinding() const;
    bool HasWindowAtPoint(int x, int y) const;
    bool HasGameplayInputModal(bool allowProductionInventory = false) const;
    bool HasNativePopupAtPoint(int x,int y) const;
    void ClampWindowToClient(int* x, int* y, int w, int h) const;
    void RepositionManagedWindowsForUiScale(int previousPercent, int nextPercent);
    void SnapWindowToNearby(UIWindow* window, int* x, int* y) const;
    void EnsureChatWindowVisible();
    bool HasActiveNpcDialog() const;
    bool CanRouteQtChatInput() const;
    void CloseNpcDialogWindows();
    void ClosePlayerContextMenu();
    void CloseNpcShopWindows();
    void CloseNpcShopQuantityPrompt();
    void CloseStorageWindows();
    void ShowItemInfoWindow(const ITEM_INFO& item, int preferredX, int preferredY,
        ItemInfoSource source = ItemInfoSource::Snapshot, ItemInfoAction action = ItemInfoAction::Select);
    void ShowItemCollectionWindow(const ITEM_INFO& item, int preferredX, int preferredY);
    void ShowSkillDescribeWindow(const PLAYER_SKILL_INFO& skillInfo, int preferredX, int preferredY);

    // Memory layout from HighPriest.exe.h:10334
    int m_chatWndX, m_chatWndY;
    int m_chatWndHeight;
    int m_chatWndShow;
    int m_gronMsnWndShow;
    int m_gronMsgWndShow;
    int m_chatWndStatus;
    bool m_chatWndStickOn = true;
    bool m_onStSubChat = false, m_onBtSubChat = false;
    std::array<SubChatWindowInfo, 2> m_subChatInfo{};
    SubChatWindowInfo m_chatRoomWindowInfo{500,180,280,120};
    UIChatRoomSettingsWnd* m_chatRoomMakeWnd=nullptr;
    UIChatRoomSettingsWnd* m_chatRoomChangeWnd=nullptr;
    UIChatRoomMessageBox* m_roomMessageBox=nullptr;
    UIPetEggListWnd* m_petEggListWnd=nullptr;
    UIPetTamingDeceiveWnd* m_petTamingDeceiveWnd=nullptr;
    UIChatRoomWnd* m_chatRoomWnd = nullptr;
    std::map<std::string,UIWhisperWnd*> m_whisperWindows;
    SubChatWindowInfo m_whisperWindowInfo{0,347,280,120};
    UIChatRoomMenuWnd* m_chatRoomMenuWnd=nullptr;
    UIAutoMessageWnd* m_autoMessageWnd=nullptr;
    UIFriendOptionWnd* m_friendOptionWnd=nullptr;
    SubChatWindowInfo m_friendOptionWindowInfo{200,120,280,120};
    bool IsWhisperPreferenceInputWindow(const UIWindow* window) const;
    bool WriteWhisperChatToFile(const std::string& name);
    bool AddFriendCharName(const std::string& name);
    UIChatRoomPasswordWnd* m_chatRoomPasswordWnd = nullptr;
    UISubChatHisWnd* m_subChatWndST = nullptr;
    UISubChatHisWnd* m_subChatWndBT = nullptr;
    UISubChatMiniWnd* m_subChatMiniWnd = nullptr;
    UIBattleMsgOptionWnd* m_battleMsgOptionWnd = nullptr;
    float m_miniMapZoomFactor;
    u32 m_miniMapArgb;
    int m_isDrawCompass;
    int m_isDragAll;
    int m_conversionMode;
    bool m_isInvalidatedByForce;
    
    std::list<UIWindow*> m_children;
    std::list<UIWindow*> m_quitWindow;
    std::list<UIWindow*> m_nameWaitingList;
    std::map<UIWindow*, CSnapInfo> m_snapInfo;
    
    UIWindow* m_captureWindow;
    UIWindow* m_editWindow;
    UIWindow* m_modalWindow;
    UIWindow* m_lastHitWindow;

    // Specialized windows
    UILoadingWnd* m_loadingWnd;
    UIRoMapWnd* m_roMapWnd;
    UIMinimapZoomWnd* m_minimapZoomWnd;
    UIStatusWnd* m_statusWnd;
    UISayDialogWnd* m_sayDialogWnd;
    UINpcMenuWnd* m_npcMenuWnd;
    UIPlayerContextMenuWnd* m_playerContextMenuWnd;
    UIJoinPartyAcceptWnd* m_joinPartyAcceptWnd;
    UIMessengerGroupWnd* m_messengerGroupWnd;
    UIPartyOptionWnd* m_partyOptionWnd;
    UIGuildWnd* m_guildWnd=nullptr;
    UINpcInputWnd* m_npcInputWnd;
    std::array<UIGroundSkillTextWnd*,2> m_groundSkillTextWindows{};
    UIChooseSellBuyWnd* m_chooseSellBuyWnd;
    UIItemShopWnd* m_itemShopWnd;
    UIItemPurchaseWnd* m_itemPurchaseWnd;
    UIItemSellWnd* m_itemSellWnd;
    UIStorageWnd* m_storageWnd;
    UIShortCutWnd* m_shortCutWnd;
    UINewChatWnd* m_chatWnd;
    UILoginWnd* m_loginWnd;
    UISelectServerWnd* m_selectServerWnd;
    UISelectCharWnd* m_selectCharWnd;
    UIMakeCharWnd* m_makeCharWnd;
    UIWaitWnd* m_waitWnd;
    UIChooseWnd* m_chooseWnd;
    UISelectCartWnd* m_selectCartWnd;
    UIOptionWnd* m_optionWnd;
    UIItemWnd* m_itemWnd;
    UIItemInfoWnd* m_itemInfoWnd;
    UIItemInfoWnd* m_cardInfoWnd = nullptr;
    UINotifyItemObtainWnd* m_itemObtainWnd = nullptr;
    UIItemCollectionWnd* m_itemCollectionWnd;
    UIItemCompositionWnd* m_itemCompositionWnd;
    UIItemIdentifyWnd* m_itemIdentifyWnd;
    UISkillSelectWnd* m_skillSelectWnd;
    UIMonsterInfoWnd* m_monsterInfoWnd = nullptr;
    UIQuestWnd* m_questWnd;
    UIBasicInfoWnd* m_basicInfoWnd;
    UINotifyLevelUpWnd* m_notifyLevelUpWnd;
    UINotifyJobLevelUpWnd* m_notifyJobLevelUpWnd;
    UIEquipWnd* m_equipWnd;
    UISkillDescribeWnd* m_skillDescribeWnd;
    UISkillListWnd* m_skillListWnd;
    std::string m_loginStatus;
    std::string m_loginWallpaper;
    std::string m_loadedWallpaperPath;
    CSurface* m_wallpaperSurface;
    ArgbDibSurface m_uiComposeSurface;

    std::vector<UIChatEvent> m_chatEvents;
    std::vector<SubChatRow> m_sourceChatBacklog;
    unsigned long long m_sourceChatGeneration = 0;
    std::vector<std::string> m_chatInputHistory;
    std::string m_chatWhisperTargetText;
    std::string m_chatInputText;
    int m_chatActiveInputField;
    int m_chatScrollLineOffset;
    std::array<ShortcutKeyBinding, kShortcutKeyBindingCount> m_shortcutKeyBindings{};

private:
    std::vector<UIWindow*> m_postedQuits;
    void InitializeInventoryWindowInfo();
    void ValidateInventoryWindowInfo();
    bool m_inventoryWindowInfoInitialized = false;
    InventoryWindowInfo m_inventoryWindowInfo;
    bool m_inventoryWindowShown = false;
    void SetInventoryWindowShowState(bool shown);
    void DeleteWindowImpl(UIWindow* window, bool preserveInventoryShow);
    ItemDetailWindowInfo m_itemDetailWindowInfo;
    ItemDetailWindowInfo m_cardDetailWindowInfo{280,278,280,120};
    UIWindow* HitTestWindow(int x, int y) const;
    bool HasFrontMenuUiVisible() const;
    bool HandleHotkeyBeforeFocusedUi(int virtualKey, bool isAltDown, bool isCtrlDown, bool hasFrontMenuUi);
    bool HandleHotkeyAfterFocusedUi(int virtualKey, bool isAltDown, bool isCtrlDown, bool isShiftDown, bool hasFrontMenuUi);
    void LoadShortcutKeyBindings();
    bool HasBlockingUiForGameplayHotkeys() const;
    void ReleaseComposeSurface();
    bool EnsureComposeSurface(int width, int height);
};

extern UIWindowMgr g_windowMgr;
