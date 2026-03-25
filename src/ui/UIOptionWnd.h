#pragma once

#include "UIFrameWnd.h"

#include <string>
#include <windows.h>

class UICheckBox;

class UIOptionWnd : public UIFrameWnd {
public:
    UIOptionWnd();
    ~UIOptionWnd() override;

    void OnCreate(int cx, int cy) override;
    void OnDraw() override;
    void OnLBtnDown(int x, int y) override;
    void OnMouseMove(int x, int y) override;
    void OnLBtnUp(int x, int y) override;
    void OnLBtnDblClk(int x, int y) override;
    int SendMsg(UIWindow* sender, int msg, int wparam, int lparam, int extra) override;
    void OnKeyDown(int virtualKey);

private:
    enum DragMode {
        DragMode_None = 0,
        DragMode_Window,
        DragMode_BgmSlider,
        DragMode_SoundSlider,
    };

    void EnsureResources();
    void ClearResources();
    void LoadSettings();
    void SaveSettings() const;
    void ApplyAudioSettings() const;
    void LayoutControls();
    void SetCollapsed(bool collapsed);
    void ResetToDefaultPlacement();
    int SliderValueFromMouseX(int mouseX) const;
    RECT GetTitleBarRect() const;
    RECT GetBaseButtonRect() const;
    RECT GetMiniButtonRect() const;
    RECT GetCloseButtonRect() const;
    RECT GetSkinRect() const;
    RECT GetBgmSliderRect() const;
    RECT GetSoundSliderRect() const;
    RECT GetSliderKnobRect(const RECT& sliderRect, int value) const;
    void DrawSlider(HDC hdc, const RECT& sliderRect, int value, const char* label) const;
    void DrawHeaderButton(HDC hdc, const RECT& rect, const char* text) const;

    bool m_controlsCreated;
    bool m_assetsProbed;
    HBITMAP m_frameBitmap;
    std::string m_frameBitmapPath;
    HBITMAP m_bodyBitmap;
    std::string m_bodyBitmapPath;
    UICheckBox* m_bgmOnCheckBox;
    UICheckBox* m_soundOnCheckBox;
    UICheckBox* m_noCtrlCheckBox;
    UICheckBox* m_attackSnapCheckBox;
    UICheckBox* m_skillSnapCheckBox;
    UICheckBox* m_itemSnapCheckBox;
    int m_orgHeight;
    int m_bgmVolume;
    int m_soundVolume;
    int m_bgmEnabled;
    int m_soundEnabled;
    int m_noCtrl;
    int m_attackSnap;
    int m_skillSnap;
    int m_itemSnap;
    int m_collapsed;
    int m_dragMode;
    int m_dragAnchorX;
    int m_dragAnchorY;
    int m_dragWindowStartX;
    int m_dragWindowStartY;
};