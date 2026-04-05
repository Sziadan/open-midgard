#pragma once

#include "UIFrameWnd.h"
#include "UIShopCommon.h"
#include "render3d/GraphicsSettings.h"
#include "render3d/RenderBackend.h"

#include <string>
#include <vector>
#include "platform/WindowsCompat.h"

class UICheckBox;

void ApplySavedAudioSettings();

class UIOptionWnd : public UIFrameWnd {
public:
    struct DisplayButton {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        bool visible = true;
        std::string label;
    };

    struct DisplayToggle {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        bool checked = false;
        std::string label;
    };

    struct DisplaySlider {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        int value = 0;
        std::string label;
    };

    struct DisplayGraphicsRow {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        int prevX = 0;
        int prevY = 0;
        int prevWidth = 0;
        int prevHeight = 0;
        int nextX = 0;
        int nextY = 0;
        int nextWidth = 0;
        int nextHeight = 0;
        std::string prevLabel;
        std::string nextLabel;
        std::string label;
        std::string value;
    };

    struct DisplayTab {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        bool active = false;
        std::string label;
    };

    struct DisplayData {
        bool collapsed = false;
        int activeTab = 0;
        int contentX = 0;
        int contentY = 0;
        int contentWidth = 0;
        int contentHeight = 0;
        std::vector<DisplayButton> systemButtons;
        DisplayButton restartButton;
        std::vector<DisplayTab> tabs;
        std::vector<DisplayToggle> toggles;
        std::vector<DisplaySlider> sliders;
        std::vector<DisplayGraphicsRow> graphicsRows;
    };

    UIOptionWnd();
    ~UIOptionWnd() override;

    void OnCreate(int cx, int cy) override;
    void OnDraw() override;
    void OnLBtnDown(int x, int y) override;
    void OnMouseMove(int x, int y) override;
    void OnLBtnUp(int x, int y) override;
    void OnLBtnDblClk(int x, int y) override;
    msgresult_t SendMsg(UIWindow* sender, int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t extra) override;
    void OnKeyDown(int virtualKey) override;
    bool GetDisplayDataForQt(DisplayData* outData) const;

private:
    enum TabId {
        TabId_Game = 0,
        TabId_Graphics,
        TabId_Audio,
        TabId_Count,
    };

    enum DragMode {
        DragMode_None = 0,
        DragMode_Window,
        DragMode_BgmSlider,
        DragMode_SoundSlider,
    };

    enum GraphicsRowId {
        GraphicsRow_Resolution = 0,
        GraphicsRow_Renderer,
        GraphicsRow_WindowMode,
        GraphicsRow_AntiAliasing,
        GraphicsRow_TextureUpscale,
        GraphicsRow_AnisotropicFiltering,
    };

    struct ResolutionEntry {
        int width;
        int height;
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
    RECT GetBodyRect() const;
    RECT GetBaseButtonRect() const;
    RECT GetMiniButtonRect() const;
    RECT GetCloseButtonRect() const;
    RECT GetTabRect(int tabIndex) const;
    RECT GetContentRect() const;
    RECT GetRowRect(int rowIndex) const;
    RECT GetRowPrevButtonRect(int rowIndex) const;
    RECT GetRowNextButtonRect(int rowIndex) const;
    RECT GetRestartButtonRect() const;
    RECT GetBgmSliderRect() const;
    RECT GetSoundSliderRect() const;
    RECT GetAudioToggleRect(int toggleIndex) const;
    RECT GetGameToggleRect(int toggleIndex) const;
    RECT GetSliderKnobRect(const RECT& sliderRect, int value) const;
    void DrawSlider(HDC hdc, const RECT& sliderRect, int value, const char* label) const;
    void DrawHeaderButton(HDC hdc, const RECT& rect, const char* text) const;
    void DrawTabButton(HDC hdc, const RECT& rect, const char* text, bool active) const;
    void DrawSettingRow(HDC hdc, int rowIndex, const char* label, const std::string& value) const;
    void RefreshResolutionEntries();
    int FindResolutionIndex(int width, int height) const;
    std::vector<GraphicsRowId> GetVisibleGraphicsRows() const;
    std::vector<RenderBackendType> GetSupportedRenderBackends() const;
    std::vector<WindowMode> GetSupportedWindowModes() const;
    std::vector<int> GetSupportedAnisotropicLevels() const;
    std::vector<AntiAliasingMode> GetSupportedAntiAliasingModes() const;
    RenderBackendType NormalizeSelectedBackend(RenderBackendType backend) const;
    bool HasPendingGraphicsRestart() const;
    void PromptForGraphicsRestart();
    void CycleGraphicsSetting(GraphicsRowId rowId, int direction);
    std::string GetGraphicsRowValue(GraphicsRowId rowId) const;
    void SaveGraphicsPreferences() const;
    bool HandleQtToggleClick(int x, int y);

    bool m_controlsCreated;
    bool m_assetsProbed;
    shopui::BitmapPixels m_frameBitmap;
    std::string m_frameBitmapPath;
    shopui::BitmapPixels m_bodyBitmap;
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
    int m_activeTab;
    int m_dragMode;
    int m_dragAnchorX;
    int m_dragAnchorY;
    int m_dragWindowStartX;
    int m_dragWindowStartY;
    GraphicsSettings m_graphicsSettings;
    GraphicsSettings m_appliedGraphicsSettings;
    RenderBackendType m_selectedRenderBackend;
    RenderBackendType m_appliedRenderBackend;
    std::vector<ResolutionEntry> m_resolutionEntries;
};