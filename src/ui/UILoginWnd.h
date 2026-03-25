#pragma once

#include "UIFrameWnd.h"
#include <string>
#include <windows.h>
#include <array>

class UILoginWnd : public UIFrameWnd {
public:
    UILoginWnd();
    ~UILoginWnd() override;

    void OnCreate(int cx, int cy) override;
    void OnDraw() override;
    int SendMsg(UIWindow* sender, int msg, int wparam, int lparam, int extra) override;
    void OnKeyDown(int virtualKey);
    void SetWallpaperName(const std::string& wallpaperName);

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
    bool EnsureComposeSurface(HDC referenceDC, int width, int height);

    bool m_controlsCreated;
    bool m_assetsProbed;
    HBITMAP m_wallpaperBmp;
    std::array<HBITMAP, UiAssetCount> m_uiAssets;
    std::array<std::string, UiAssetCount> m_uiAssetPaths;
    HDC m_composeDC;
    HBITMAP m_composeBitmap;
    int m_composeWidth;
    int m_composeHeight;
    UIEditCtrl* m_login;
    UIEditCtrl* m_password;
    UIBitmapButton* m_cancelButton;
    UICheckBox* m_saveAccountCheck;
    std::string m_requestedWallpaper;
    std::string m_wallpaperPath;
};
