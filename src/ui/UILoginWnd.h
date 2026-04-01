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
    msgresult_t SendMsg(UIWindow* sender, int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t extra) override;
    void OnKeyDown(int virtualKey);
    void SetWallpaperName(const std::string& wallpaperName);
    bool HandleQtMouseDown(int x, int y);
    const char* GetLoginText() const;
    int GetPasswordLength() const;
    bool IsSaveAccountChecked() const;
    bool IsPasswordFocused() const;

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
    HBITMAP m_wallpaperBmp;
    std::array<HBITMAP, UiAssetCount> m_uiAssets;
    std::array<std::string, UiAssetCount> m_uiAssetPaths;
    HDC m_composeDC;
    HBITMAP m_composeBitmap;
    void* m_composeBits;
    int m_composeWidth;
    int m_composeHeight;
    bool m_saveAccountChecked;
    UIEditCtrl* m_login;
    UIEditCtrl* m_password;
    UIBitmapButton* m_cancelButton;
    UICheckBox* m_saveAccountCheck;
    std::string m_requestedWallpaper;
    std::string m_wallpaperPath;
};
