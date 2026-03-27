#pragma once
#include "UIWindow.h"
#include <map>

// Forward declarations for all windows managed by UIWindowMgr
class UILoadingWnd;
class UIMinimapZoomWnd;
class UIStatusWnd;
class UINewChatWnd;
class UILoginWnd;
class UISelectServerWnd;
class UISelectCharWnd;
class UIMakeCharWnd;
class UIItemWnd;
class UIQuestWnd;
class UIBasicInfoWnd;
class UINotifyLevelUpWnd;
class UINotifyJobLevelUpWnd;
class UIEquipWnd;
class UIOptionWnd;
class UIShortCutWnd;
class UIItemDropCntWnd;
class UISayDialogWnd;
class UIChooseWnd;
class CBitmapRes;
class CSurface;

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
        WID_SAYDIALOGWND      = 9,
        WID_LOADINGWND        = 10,
        WID_OPTIONWND         = 13,
        WID_EQUIPWND          = 14,
        WID_NOTIFYLEVELUPWND  = 21,
        WID_CHOOSEWND         = 17,
        WID_NOTIFYJOBLEVELUPWND = 49,
    };

    UIWindowMgr();
    ~UIWindowMgr();

    bool Init();
    void Reset();
    void OnProcess();
    void OnDraw();
    bool HasDirtyVisualState() const;
    void RenderWallPaper();
    void DrawWallpaperToDC(HDC targetDC, int width, int height);
    void SetWallpaper(CBitmapRes* bitmap);
    bool SetWallpaperFromGameData(const std::string& wallpaperName);
    void ShowLoadingScreen(const std::string& wallpaperName, const std::string& message, float progress);
    void UpdateLoadingScreen(const std::string& message, float progress);
    void HideLoadingScreen();
    void SetComposeCursorState(int cursorActNum, u32 mouseAnimStartTick, bool enabled);
    void SendMsg(int msg, int wparam, int lparam);
    void SetLoginStatus(const std::string& status);
    void SetLoginWallpaper(const std::string& wallpaperName);
    const std::string& GetLoginStatus() const;
    const std::vector<UIChatEvent>& GetChatEvents() const;
    std::vector<UIChatEvent> GetChatPreviewEvents(size_t maxCount) const;
    void ClearChatEvents();
    UIWindow* MakeWindow(int windowId);
    void RemoveAllWindows();
    void DeleteWindow(UIWindow* window);

    // Input routing
    void OnLBtnDown(int x, int y);
    void OnLBtnDblClk(int x, int y);
    void OnLBtnUp(int x, int y);
    void OnMouseMove(int x, int y);
    void OnChar(char c);
    void OnKeyDown(int virtualKey);
    bool HasWindowAtPoint(int x, int y) const;

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
    UIMinimapZoomWnd* m_minimapZoomWnd;
    UIStatusWnd* m_statusWnd;
    UINewChatWnd* m_chatWnd;
    UILoginWnd* m_loginWnd;
    UISelectServerWnd* m_selectServerWnd;
    UISelectCharWnd* m_selectCharWnd;
    UIMakeCharWnd* m_makeCharWnd;
    UIChooseWnd* m_chooseWnd;
    UIOptionWnd* m_optionWnd;
    UIItemWnd* m_itemWnd;
    UIQuestWnd* m_questWnd;
    UIBasicInfoWnd* m_basicInfoWnd;
    UINotifyLevelUpWnd* m_notifyLevelUpWnd;
    UINotifyJobLevelUpWnd* m_notifyJobLevelUpWnd;
    UIEquipWnd* m_equipWnd;
    std::string m_loginStatus;
    std::string m_loginWallpaper;
    std::string m_loadedWallpaperPath;
    CSurface* m_wallpaperSurface;
    HDC m_uiComposeDC;
    HBITMAP m_uiComposeBitmap;
    void* m_uiComposeBits;
    int m_uiComposeWidth;
    int m_uiComposeHeight;
    int m_composeCursorActNum;
    u32 m_composeCursorStartTick;
    bool m_composeCursorEnabled;

    std::vector<UIChatEvent> m_chatEvents;

private:
    UIWindow* HitTestWindow(int x, int y) const;
    void ReleaseComposeSurface();
    bool EnsureComposeSurface(HDC referenceDC, int width, int height);
};

extern UIWindowMgr g_windowMgr;
