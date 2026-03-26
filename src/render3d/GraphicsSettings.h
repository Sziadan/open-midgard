#pragma once

#include "RenderBackend.h"

enum class WindowMode {
    Windowed = 0,
    Fullscreen,
    BorderlessFullscreen,
};

enum class AntiAliasingMode {
    None = 0,
    MSAA2X,
    MSAA4X,
    MSAA8X,
};

struct GraphicsSettings {
    int width;
    int height;
    WindowMode windowMode;
    int textureUpscaleFactor;
    int anisotropicLevel;
    AntiAliasingMode antiAliasing;
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
WindowMode GetEffectiveWindowModeForBackend(RenderBackendType backend, WindowMode requestedMode);
bool GraphicsSettingsRequireRestart(const GraphicsSettings& current, const GraphicsSettings& pending);
