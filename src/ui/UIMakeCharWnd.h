#pragma once

#include "UIFrameWnd.h"
#include "UIShopCommon.h"

#include "res/ActRes.h"
#include "res/ImfRes.h"
#include "res/Sprite.h"

#include <array>
#include <string>
#include <windows.h>

class UIEditCtrl;
class UIBitmapButton;

class UIMakeCharWnd : public UIFrameWnd {
public:
    struct MakeCharDisplay {
        std::string name;
        bool nameFocused = false;
        int stats[6]{};
        int hairIndex = 1;
        int hairColor = 0;
    };

    struct PreviewState {
        int x = 0;
        int y = 0;
        int baseAction = 0;
        int curAction = 0;
        int curMotion = 0;
        int bodyPalette = 0;
        int headPalette = 0;
        std::string actName[2];
        std::string sprName[2];
        std::string imfName;
        std::string bodyPaletteName;
        std::string headPaletteName;
    };

    UIMakeCharWnd();
    ~UIMakeCharWnd() override;

    void OnCreate(int cx, int cy) override;
    void OnDraw() override;
    void OnProcess() override;
    msgresult_t SendMsg(UIWindow* sender, int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t extra) override;
    void OnKeyDown(int virtualKey);
    bool HandleQtMouseDown(int x, int y);
    bool HandleQtMouseUp(int x, int y);
    bool GetMakeCharDisplay(MakeCharDisplay* out) const;

private:
    void EnsureResourceCache();
    void EnsureButtons();
    void ClearAssets();
    void ReleaseComposeSurface();
    bool EnsureComposeSurface(int width, int height);
    void DrawHexagon(HDC hdc) const;
    void RebuildPreview();
    void DrawPreview(HDC hdc, const PreviewState& preview);

    // Stats in order: [0]=Str [1]=Agi [2]=Vit [3]=Int [4]=Dex [5]=Luk
    int m_stats[6];
    int m_hairIdx;
    int m_hairColor;

    bool m_controlsCreated;
    bool m_assetsProbed;
    shopui::BitmapPixels m_backgroundBmp;
    std::string m_backgroundPath;
    HDC m_composeDC;
    HBITMAP m_composeBitmap;
    void* m_composeBits;
    int m_composeWidth;
    int m_composeHeight;
    UIEditCtrl* m_nameEditCtrl;
    UIBitmapButton* m_okButton;
    UIBitmapButton* m_cancelButton;
    // Stat swap buttons indexed [0..5] = IDs 139,142,140,144,141,143
    UIBitmapButton* m_statBtns[6];
    // Char1 hair buttons: [0]=161(prev) [1]=160(next) [2]=213(color)
    UIBitmapButton* m_hairBtns[3];
    PreviewState m_preview;
    DWORD m_lastPreviewAdvanceTick;
};