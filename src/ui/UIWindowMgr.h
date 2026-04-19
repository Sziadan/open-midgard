#pragma once
#include "render/DC.h"
#include "UIWindow.h"
#include <map>

// Forward declarations for all windows managed by UIWindowMgr
class UILoadingWnd;
class UIRoMapWnd;
class UIMinimapZoomWnd;
class UIStatusWnd;
class UINewChatWnd;
class UILoginWnd;
class UISelectServerWnd;
class UISelectCharWnd;
class UIMakeCharWnd;
class UIWaitWnd;
class UIItemWnd;
class UIStorageWnd;
class UIItemInfoWnd;
class UIItemCollectionWnd;
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
class UINpcInputWnd;
class UIChooseWnd;
class UIChooseSellBuyWnd;
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
};

//===========================================================================
// UIWindowMgr  –  The primary UI controller
//===========================================================================
class UIWindowMgr {
public:
    // Window IDs (Ref-faithful, from decompiled address space)
    enum WindowId {
        WID_BASICINFOWND      = 0,
        WID_LOGINWND          = 1,
        WID_SELECTSERVERWND   = 3,
        WID_SELECTCHARWND     = 4,
        WID_MAKECHARWND       = 5,
        WID_WAITWND           = 7,
        WID_ITEMWND           = 8,
        WID_NOTICECONFIRMWND  = 8,
        WID_ITEMINFOWND       = 24,
        WID_ITEMCOLLECTIONWND = 25,
        WID_SKILLDESCRIBEWND  = 26,
        WID_SAYDIALOGWND      = 9,
        WID_LOADINGWND        = 10,
        WID_STATUSWND         = 11,
        WID_NPCMENUWND        = 12,
        WID_OPTIONWND         = 13,
        WID_EQUIPWND          = 14,
        WID_SKILLLISTWND      = 15,
        WID_NPCINPUTWND       = 16,
        WID_CHOOSESELLBUYWND  = 18,
        WID_ITEMSHOPWND       = 19,
        WID_ITEMPURCHASEWND   = 20,
        WID_NOTIFYLEVELUPWND  = 21,
        WID_ITEMSELLWND       = 22,
        WID_SHORTCUTWND       = 23,
        WID_STORAGEWND        = 50,
        WID_CHOOSEWND         = 17,
        WID_ROMAPWND          = 0x93,
        WID_NOTIFYJOBLEVELUPWND = 49,
    };

    UIWindowMgr();
    ~UIWindowMgr();

    bool Init();
    void Reset();
    void OnProcess();
    void OnDraw();
    void DrawVisibleWindowsToHdc(HDC targetDC, bool includeRoMap);
    bool HasDirtyVisualState() const;
    bool HasDirtyVisualStateExcludingRoMap() const;
    bool HasRoMapDirtyVisualState() const;
    void ClearDirtyVisualState();
    void ClearDirtyVisualStateExcludingRoMap();
    void OnDrawExcludingRoMapToHdc(HDC targetDC);
    bool DrawRoMapToHdc(HDC targetDC, int x, int y);
    bool GetRoMapRect(RECT* outRect) const;
    void RenderWallPaper();
    void DrawWallpaperToDC(HDC targetDC, int width, int height);
    void SetWallpaper(CBitmapRes* bitmap);
    bool SetWallpaperFromGameData(const std::string& wallpaperName);
    void ShowLoadingScreen(const std::string& wallpaperName, const std::string& message, float progress);
    void UpdateLoadingScreen(const std::string& message, float progress);
    void HideLoadingScreen();
    void SendMsg(int msg, msgparam_t wparam, msgparam_t lparam);
    void PushChatEvent(const char* text, u32 color, u8 channel, u32 tick = 0);
    void SetLoginStatus(const std::string& status);
    void SetLoginWallpaper(const std::string& wallpaperName);
    const std::string& GetLoginStatus() const;
    const std::vector<UIChatEvent>& GetChatEvents() const;
    std::vector<UIChatEvent> GetChatPreviewEvents(size_t maxCount) const;
    void ClearChatEvents();
    UIWindow* MakeWindow(int windowId);
    bool ToggleWindow(int windowId);
    void RemoveAllWindows();
    void DeleteWindow(UIWindow* window);
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
    void OnKeyDown(int virtualKey);
    bool OnQtKeyDown(int virtualKey, bool isAltDown, bool isCtrlDown, bool isShiftDown);
    bool HasWindowAtPoint(int x, int y) const;
    void ClampWindowToClient(int* x, int* y, int w, int h) const;
    void RepositionManagedWindowsForUiScale(int previousPercent, int nextPercent);
    void SnapWindowToNearby(UIWindow* window, int* x, int* y) const;
    void EnsureChatWindowVisible();
    bool HasActiveNpcDialog() const;
    void CloseNpcDialogWindows();
    void CloseNpcShopWindows();
    void CloseStorageWindows();
    void ShowItemInfoWindow(const ITEM_INFO& item, int preferredX, int preferredY);
    void ShowItemCollectionWindow(const ITEM_INFO& item, int preferredX, int preferredY);
    void ShowSkillDescribeWindow(const PLAYER_SKILL_INFO& skillInfo, int preferredX, int preferredY);

    // Memory layout from HighPriest.exe.h:10334
    int m_chatWndX, m_chatWndY;
    int m_chatWndHeight;
    int m_chatWndShow;
    int m_gronMsnWndShow;
    int m_gronMsgWndShow;
    int m_chatWndStatus;
    float m_miniMapZoomFactor;
    u32 m_miniMapArgb;
    int m_isDrawCompass;
    int m_isDragAll;
    int m_conversionMode;
    
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
    UINpcInputWnd* m_npcInputWnd;
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
    UIOptionWnd* m_optionWnd;
    UIItemWnd* m_itemWnd;
    UIItemInfoWnd* m_itemInfoWnd;
    UIItemCollectionWnd* m_itemCollectionWnd;
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
    std::vector<std::string> m_chatInputHistory;
    std::string m_chatWhisperTargetText;
    std::string m_chatInputText;
    int m_chatActiveInputField;
    int m_chatScrollLineOffset;

private:
    UIWindow* HitTestWindow(int x, int y) const;
    bool HasFrontMenuUiVisible() const;
    bool HandleHotkeyBeforeFocusedUi(int virtualKey, bool isAltDown, bool isCtrlDown, bool hasFrontMenuUi);
    bool HandleHotkeyAfterFocusedUi(int virtualKey, bool hasFrontMenuUi);
    bool HasBlockingUiForGameplayHotkeys() const;
    void ReleaseComposeSurface();
    bool EnsureComposeSurface(int width, int height);
};

extern UIWindowMgr g_windowMgr;
