#include "GraphicsSettings.h"

#include "core/SettingsIni.h"

#include <windows.h>

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace {

constexpr char kGraphicsSection[] = "Graphics";
constexpr char kGraphicsWidthValue[] = "Width";
constexpr char kGraphicsHeightValue[] = "Height";
constexpr char kGraphicsWindowModeValue[] = "WindowMode";
constexpr char kGraphicsTextureUpscaleValue[] = "TextureUpscale";
constexpr char kGraphicsAnisotropicValue[] = "AnisotropicLevel";
constexpr char kGraphicsAntiAliasingValue[] = "AntiAliasing";
constexpr char kGraphicsVerticalSyncValue[] = "VSync";
constexpr char kGraphicsSpriteFilterValue[] = "SpriteFilter";
constexpr char kGraphicsFrameLimitValue[] = "FrameLimit";
constexpr char kGraphicsRenderScaleValue[] = "RenderScale";
constexpr int kMinWidth = 640;
constexpr int kMinHeight = 480;
constexpr int kMaxWidth = 7680;
constexpr int kMaxHeight = 4320;
GraphicsSettings g_cachedSettings = GetDefaultGraphicsSettings();
bool g_cachedSettingsValid = false;

int ClampToAllowedAnisotropy(int level)
{
    static constexpr int kAllowedLevels[] = { 1, 2, 4, 8, 16 };
    int best = kAllowedLevels[0];
    for (int candidate : kAllowedLevels) {
        if (level >= candidate) {
            best = candidate;
        }
    }
    return best;
}

int SnapToNearest(int value, const int* candidates, int candidateCount)
{
    int best = candidates[0];
    int bestDistance = std::abs(value - best);
    for (int index = 1; index < candidateCount; ++index) {
        const int distance = std::abs(value - candidates[index]);
        if (distance < bestDistance) {
            best = candidates[index];
            bestDistance = distance;
        }
    }
    return best;
}

int SnapFrameLimit(int limit)
{
    static constexpr int kLimits[] = { 0, 60, 120 };
    return SnapToNearest(limit, kLimits, static_cast<int>(std::size(kLimits)));
}

int SnapRenderScale(int percent)
{
    static constexpr int kPercents[] = { 75, 100, 125, 150 };
    return SnapToNearest(percent, kPercents, static_cast<int>(std::size(kPercents)));
}

void PublishLiveGraphicsFields(const GraphicsSettings& settings)
{
    if (!g_cachedSettingsValid) {
        return;
    }

    g_cachedSettings.verticalSync = settings.verticalSync;
    g_cachedSettings.spriteFilter = settings.spriteFilter;
    g_cachedSettings.frameLimit = settings.frameLimit;
    g_cachedSettings.renderScalePercent = settings.renderScalePercent;
}

} // namespace

GraphicsSettings GetDefaultGraphicsSettings()
{
    GraphicsSettings settings{};
    settings.width = kMinWidth;
    settings.height = kMinHeight;
    settings.windowMode = WindowMode::Windowed;
    settings.textureUpscaleFactor = 1;
    settings.anisotropicLevel = 1;
    settings.antiAliasing = AntiAliasingMode::None;
    settings.verticalSync = false;
    settings.spriteFilter = SpriteFilterMode::Smooth;
    settings.frameLimit = 0;
    settings.renderScalePercent = 100;
    return settings;
}

void SanitizeGraphicsSettings(GraphicsSettings* settings)
{
    if (!settings) {
        return;
    }

    settings->width = (std::max)(kMinWidth, (std::min)(kMaxWidth, settings->width));
    settings->height = (std::max)(kMinHeight, (std::min)(kMaxHeight, settings->height));

    if (settings->windowMode != WindowMode::Windowed
        && settings->windowMode != WindowMode::Fullscreen
        && settings->windowMode != WindowMode::BorderlessFullscreen) {
        settings->windowMode = WindowMode::Windowed;
    }

    settings->textureUpscaleFactor = (std::max)(1, (std::min)(4, settings->textureUpscaleFactor));
    settings->anisotropicLevel = ClampToAllowedAnisotropy(settings->anisotropicLevel);
    if (settings->antiAliasing != AntiAliasingMode::None
        && settings->antiAliasing != AntiAliasingMode::FXAA
        && settings->antiAliasing != AntiAliasingMode::SMAA) {
        settings->antiAliasing = AntiAliasingMode::None;
    }

    if (settings->spriteFilter != SpriteFilterMode::Smooth
        && settings->spriteFilter != SpriteFilterMode::Sharp) {
        settings->spriteFilter = SpriteFilterMode::Smooth;
    }
    settings->frameLimit = SnapFrameLimit(settings->frameLimit);
    settings->renderScalePercent = SnapRenderScale(settings->renderScalePercent);
}

GraphicsSettings LoadGraphicsSettings()
{
    GraphicsSettings settings = GetDefaultGraphicsSettings();

    settings.width = LoadSettingsIniInt(kGraphicsSection, kGraphicsWidthValue, settings.width);
    settings.height = LoadSettingsIniInt(kGraphicsSection, kGraphicsHeightValue, settings.height);
    settings.windowMode = static_cast<WindowMode>(LoadSettingsIniInt(kGraphicsSection, kGraphicsWindowModeValue, static_cast<int>(settings.windowMode)));
    settings.textureUpscaleFactor = LoadSettingsIniInt(kGraphicsSection, kGraphicsTextureUpscaleValue, settings.textureUpscaleFactor);
    settings.anisotropicLevel = LoadSettingsIniInt(kGraphicsSection, kGraphicsAnisotropicValue, settings.anisotropicLevel);
    settings.antiAliasing = static_cast<AntiAliasingMode>(LoadSettingsIniInt(kGraphicsSection, kGraphicsAntiAliasingValue, static_cast<int>(settings.antiAliasing)));
    settings.verticalSync = LoadSettingsIniInt(kGraphicsSection, kGraphicsVerticalSyncValue, settings.verticalSync ? 1 : 0) != 0;
    settings.spriteFilter = static_cast<SpriteFilterMode>(LoadSettingsIniInt(kGraphicsSection, kGraphicsSpriteFilterValue, static_cast<int>(settings.spriteFilter)));
    settings.frameLimit = LoadSettingsIniInt(kGraphicsSection, kGraphicsFrameLimitValue, settings.frameLimit);
    settings.renderScalePercent = LoadSettingsIniInt(kGraphicsSection, kGraphicsRenderScaleValue, settings.renderScalePercent);

    SanitizeGraphicsSettings(&settings);
    return settings;
}

bool SaveGraphicsSettings(const GraphicsSettings& rawSettings)
{
    GraphicsSettings settings = rawSettings;
    SanitizeGraphicsSettings(&settings);

    const bool saved = SaveSettingsIniInt(kGraphicsSection, kGraphicsWidthValue, settings.width)
        && SaveSettingsIniInt(kGraphicsSection, kGraphicsHeightValue, settings.height)
        && SaveSettingsIniInt(kGraphicsSection, kGraphicsWindowModeValue, static_cast<int>(settings.windowMode))
        && SaveSettingsIniInt(kGraphicsSection, kGraphicsTextureUpscaleValue, settings.textureUpscaleFactor)
        && SaveSettingsIniInt(kGraphicsSection, kGraphicsAnisotropicValue, settings.anisotropicLevel)
        && SaveSettingsIniInt(kGraphicsSection, kGraphicsAntiAliasingValue, static_cast<int>(settings.antiAliasing))
        && SaveSettingsIniInt(kGraphicsSection, kGraphicsVerticalSyncValue, settings.verticalSync ? 1 : 0)
        && SaveSettingsIniInt(kGraphicsSection, kGraphicsSpriteFilterValue, static_cast<int>(settings.spriteFilter))
        && SaveSettingsIniInt(kGraphicsSection, kGraphicsFrameLimitValue, settings.frameLimit)
        && SaveSettingsIniInt(kGraphicsSection, kGraphicsRenderScaleValue, settings.renderScalePercent);
    if (saved) {
        PublishLiveGraphicsFields(settings);
    }
    return saved;
}

const GraphicsSettings& GetCachedGraphicsSettings()
{
    if (!g_cachedSettingsValid) {
        g_cachedSettings = LoadGraphicsSettings();
        g_cachedSettingsValid = true;
    }
    return g_cachedSettings;
}

void RefreshGraphicsSettingsCache()
{
    g_cachedSettings = LoadGraphicsSettings();
    g_cachedSettingsValid = true;
}

bool DoesBackendSupportWindowMode(RenderBackendType backend, WindowMode mode)
{
    switch (backend) {
    case RenderBackendType::LegacyDirect3D7:
        return mode == WindowMode::Windowed
            || mode == WindowMode::Fullscreen
            || mode == WindowMode::BorderlessFullscreen;

    case RenderBackendType::Direct3D11:
    case RenderBackendType::Direct3D12:
    case RenderBackendType::Vulkan:
        return mode == WindowMode::Windowed || mode == WindowMode::BorderlessFullscreen;

    default:
        return mode == WindowMode::Windowed;
    }
}

bool DoesBackendSupportResolutionSelection(RenderBackendType backend)
{
    return backend == RenderBackendType::LegacyDirect3D7
        || backend == RenderBackendType::Direct3D11
        || backend == RenderBackendType::Direct3D12
        || backend == RenderBackendType::Vulkan;
}

bool DoesBackendSupportTextureUpscaling(RenderBackendType backend)
{
    return backend == RenderBackendType::LegacyDirect3D7
        || backend == RenderBackendType::Direct3D11
        || backend == RenderBackendType::Direct3D12
        || backend == RenderBackendType::Vulkan;
}

bool DoesBackendSupportAnisotropicFiltering(RenderBackendType backend)
{
    return backend == RenderBackendType::Direct3D11
        || backend == RenderBackendType::Direct3D12
        || backend == RenderBackendType::Vulkan;
}

std::vector<AntiAliasingMode> GetSupportedAntiAliasingModesForBackend(RenderBackendType backend)
{
    switch (backend) {
    case RenderBackendType::Direct3D11:
    case RenderBackendType::Direct3D12:
    case RenderBackendType::Vulkan:
        return { AntiAliasingMode::None, AntiAliasingMode::FXAA, AntiAliasingMode::SMAA };

    default:
        return {};
    }
}

bool DoesBackendSupportAntiAliasing(RenderBackendType backend)
{
    return !GetSupportedAntiAliasingModesForBackend(backend).empty();
}

bool DoesBackendSupportRenderScale(RenderBackendType backend)
{
    return backend == RenderBackendType::Direct3D11
        || backend == RenderBackendType::Direct3D12
        || backend == RenderBackendType::Vulkan;
}

void GetSceneRenderSize(int windowWidth, int windowHeight, int* outWidth, int* outHeight)
{
    const int percent = GetCachedGraphicsSettings().renderScalePercent;
    const int width = windowWidth > 0 ? (windowWidth * percent + 50) / 100 : 1;
    const int height = windowHeight > 0 ? (windowHeight * percent + 50) / 100 : 1;
    if (outWidth) {
        *outWidth = (std::max)(1, width);
    }
    if (outHeight) {
        *outHeight = (std::max)(1, height);
    }
}

bool GraphicsSettingsUseOffscreenScene(RenderBackendType backend)
{
    const GraphicsSettings& settings = GetCachedGraphicsSettings();
    if (DoesBackendSupportRenderScale(backend) && settings.renderScalePercent != 100) {
        return true;
    }

    const AntiAliasingMode antiAliasing = GetEffectiveAntiAliasingModeForBackend(backend, settings.antiAliasing);
    return antiAliasing == AntiAliasingMode::FXAA || antiAliasing == AntiAliasingMode::SMAA;
}

bool DoesBackendSupportAntiAliasingMode(RenderBackendType backend, AntiAliasingMode mode)
{
    if (mode == AntiAliasingMode::None) {
        return true;
    }

    const std::vector<AntiAliasingMode> supportedModes = GetSupportedAntiAliasingModesForBackend(backend);
    return std::find(supportedModes.begin(), supportedModes.end(), mode) != supportedModes.end();
}

AntiAliasingMode GetEffectiveAntiAliasingModeForBackend(RenderBackendType backend, AntiAliasingMode requestedMode)
{
    return DoesBackendSupportAntiAliasingMode(backend, requestedMode)
        ? requestedMode
        : AntiAliasingMode::None;
}

SmaaPreset GetDefaultSmaaPreset()
{
    return SmaaPreset::High;
}

const char* GetSmaaPresetName(SmaaPreset preset)
{
    switch (preset) {
    case SmaaPreset::High:
        return "High";

    default:
        return "High";
    }
}

void ClampGraphicsSettingsToBackend(RenderBackendType backend, GraphicsSettings* settings)
{
    if (!settings) {
        return;
    }

    settings->windowMode = GetEffectiveWindowModeForBackend(backend, settings->windowMode);
    if (!DoesBackendSupportAnisotropicFiltering(backend)) {
        settings->anisotropicLevel = 1;
    }
    settings->antiAliasing = GetEffectiveAntiAliasingModeForBackend(backend, settings->antiAliasing);
    if (!DoesBackendSupportRenderScale(backend)) {
        settings->renderScalePercent = 100;
    }
}

WindowMode GetEffectiveWindowModeForBackend(RenderBackendType backend, WindowMode requestedMode)
{
    if (DoesBackendSupportWindowMode(backend, requestedMode)) {
        return requestedMode;
    }

    if (DoesBackendSupportWindowMode(backend, WindowMode::BorderlessFullscreen)) {
        return WindowMode::BorderlessFullscreen;
    }

    return WindowMode::Windowed;
}

bool GraphicsSettingsRequireRestart(const GraphicsSettings& current, const GraphicsSettings& pending)
{
    return current.width != pending.width
        || current.height != pending.height
        || current.windowMode != pending.windowMode
        || current.textureUpscaleFactor != pending.textureUpscaleFactor
        || current.anisotropicLevel != pending.anisotropicLevel
        || current.antiAliasing != pending.antiAliasing;
}
