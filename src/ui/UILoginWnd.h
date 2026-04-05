#pragma once

#include "UIFrameWnd.h"
#include "UIShopCommon.h"
#include "render/DC.h"
#include "platform/WindowsCompat.h"
#include <string>
#include <array>

class UILoginWnd : public UIFrameWnd {
public:
    struct QtButtonDisplay {
        int id = 0;
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        const char* label = "";
    };

    UILoginWnd();
    ~UILoginWnd() override;

    void OnCreate(int cx, int cy) override;
    void OnDraw() override;
    msgresult_t SendMsg(UIWindow* sender, int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t extra) override;
    void OnKeyDown(int virtualKey) override;
    void SetWallpaperName(const std::string& wallpaperName);
    bool HandleQtMouseDown(int x, int y);
    void EnsureQtLayout();
    bool GetQtPanelBitmap(const unsigned int** pixels, int* width, int* height);
    void RefreshRememberedUserIdStorage();
    const char* GetLoginText() const;
    int GetPasswordLength() const;
    bool IsSaveAccountChecked() const;
    bool IsPasswordFocused() const;
    int GetQtButtonCount() const;
    bool GetQtButtonDisplayForQt(int index, QtButtonDisplay* outButton) const;

private:
    enum UiAssetSlot {
        UiPanel = 0,
        UiLogo,
        UiButton,
        UiField,
        UiAssetCount
    };

    void EnsureResourceCache();
    void ClearUiAssets();
    void ReleaseComposeSurface();
    bool EnsureComposeSurface(int width, int height);

    bool m_controlsCreated;
    bool m_assetsProbed;
    shopui::BitmapPixels m_wallpaperBmp;
    std::array<shopui::BitmapPixels, UiAssetCount> m_uiAssets;
    std::array<std::string, UiAssetCount> m_uiAssetPaths;
    ArgbDibSurface m_composeSurface;
    bool m_saveAccountChecked;
    UIEditCtrl* m_login;
    UIEditCtrl* m_password;
    UIBitmapButton* m_cancelButton;
    UICheckBox* m_saveAccountCheck;
    std::string m_requestedWallpaper;
    std::string m_wallpaperPath;
    bool m_hasRememberedUserIdSnapshot;
    bool m_lastRememberedUserIdEnabled;
    std::string m_lastRememberedUserId;
};
