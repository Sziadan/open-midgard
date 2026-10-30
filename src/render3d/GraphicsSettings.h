#pragma once

#include "RenderBackend.h"

#include <vector>

enum class WindowMode {
    Windowed = 0,
    Fullscreen,
    BorderlessFullscreen,
};

enum class AntiAliasingMode {
    None = 0,
    FXAA,
    SMAA,
};

enum class SmaaPreset {
    High = 0,
};

enum class SpriteFilterMode {
    Smooth = 0,
    Sharp,
};

struct GraphicsSettings {
    int width;
    int height;
    WindowMode windowMode;
    int textureUpscaleFactor;
    int anisotropicLevel;
    AntiAliasingMode antiAliasing;
    bool verticalSync;
    SpriteFilterMode spriteFilter;
    int frameLimit;
    int renderScalePercent;
};

GraphicsSettings LoadGraphicsSettings();
bool SaveGraphicsSettings(const GraphicsSettings& settings);
GraphicsSettings GetDefaultGraphicsSettings();
const GraphicsSettings& GetCachedGraphicsSettings();
void RefreshGraphicsSettingsCache();
void SanitizeGraphicsSettings(GraphicsSettings* settings);

bool DoesBackendSupportWindowMode(RenderBackendType backend, WindowMode mode);
bool DoesBackendSupportResolutionSelection(RenderBackendType backend);
bool DoesBackendSupportTextureUpscaling(RenderBackendType backend);
bool DoesBackendSupportAnisotropicFiltering(RenderBackendType backend);
bool DoesBackendSupportAntiAliasing(RenderBackendType backend);
bool DoesBackendSupportRenderScale(RenderBackendType backend);
void GetSceneRenderSize(int windowWidth, int windowHeight, int* outWidth, int* outHeight);
bool GraphicsSettingsUseOffscreenScene(RenderBackendType backend);
std::vector<AntiAliasingMode> GetSupportedAntiAliasingModesForBackend(RenderBackendType backend);
bool DoesBackendSupportAntiAliasingMode(RenderBackendType backend, AntiAliasingMode mode);
AntiAliasingMode GetEffectiveAntiAliasingModeForBackend(RenderBackendType backend, AntiAliasingMode requestedMode);
SmaaPreset GetDefaultSmaaPreset();
const char* GetSmaaPresetName(SmaaPreset preset);
void ClampGraphicsSettingsToBackend(RenderBackendType backend, GraphicsSettings* settings);
WindowMode GetEffectiveWindowModeForBackend(RenderBackendType backend, WindowMode requestedMode);
bool GraphicsSettingsRequireRestart(const GraphicsSettings& current, const GraphicsSettings& pending);
