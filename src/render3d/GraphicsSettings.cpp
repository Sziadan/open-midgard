#include "GraphicsSettings.h"

#include <windows.h>

#include <algorithm>

namespace {

constexpr char kRegPath[] = "Software\\Gravity Soft\\Ragnarok Online";
constexpr char kGraphicsWidthValue[] = "GraphicsWidth";
constexpr char kGraphicsHeightValue[] = "GraphicsHeight";
constexpr char kGraphicsWindowModeValue[] = "GraphicsWindowMode";
constexpr char kGraphicsTextureUpscaleValue[] = "GraphicsTextureUpscale";
constexpr char kGraphicsAnisotropicValue[] = "GraphicsAnisotropicLevel";
constexpr char kGraphicsAntiAliasingValue[] = "GraphicsAntiAliasing";
constexpr int kMinWidth = 640;
constexpr int kMinHeight = 480;
constexpr int kMaxWidth = 7680;
constexpr int kMaxHeight = 4320;
GraphicsSettings g_cachedSettings = GetDefaultGraphicsSettings();
bool g_cachedSettingsValid = false;

void LoadDwordSetting(HKEY key, const char* valueName, int* target)
{
    if (!key || !valueName || !target) {
        return;
    }

    DWORD value = static_cast<DWORD>(*target);
    DWORD size = sizeof(value);
    if (RegQueryValueExA(key, valueName, nullptr, nullptr, reinterpret_cast<BYTE*>(&value), &size) == ERROR_SUCCESS) {
        *target = static_cast<int>(value);
    }
}

void SaveDwordSetting(HKEY key, const char* valueName, int value)
{
    if (!key || !valueName) {
        return;
    }

    const DWORD rawValue = static_cast<DWORD>(value);
    RegSetValueExA(key, valueName, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&rawValue), sizeof(rawValue));
}

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

} // namespace

GraphicsSettings GetDefaultGraphicsSettings()
{
    GraphicsSettings settings{};
    settings.width = 1920;
    settings.height = 1080;
    settings.windowMode = WindowMode::Windowed;
    settings.textureUpscaleFactor = 1;
    settings.anisotropicLevel = 1;
    settings.antiAliasing = AntiAliasingMode::None;
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
        && settings->antiAliasing != AntiAliasingMode::MSAA2X
        && settings->antiAliasing != AntiAliasingMode::MSAA4X
        && settings->antiAliasing != AntiAliasingMode::MSAA8X) {
        settings->antiAliasing = AntiAliasingMode::None;
    }
}

GraphicsSettings LoadGraphicsSettings()
{
    GraphicsSettings settings = GetDefaultGraphicsSettings();

    HKEY key = nullptr;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, kRegPath, 0, KEY_READ, &key) == ERROR_SUCCESS) {
        LoadDwordSetting(key, kGraphicsWidthValue, &settings.width);
        LoadDwordSetting(key, kGraphicsHeightValue, &settings.height);
        LoadDwordSetting(key, kGraphicsWindowModeValue, reinterpret_cast<int*>(&settings.windowMode));
        LoadDwordSetting(key, kGraphicsTextureUpscaleValue, &settings.textureUpscaleFactor);
        LoadDwordSetting(key, kGraphicsAnisotropicValue, &settings.anisotropicLevel);
        LoadDwordSetting(key, kGraphicsAntiAliasingValue, reinterpret_cast<int*>(&settings.antiAliasing));
        RegCloseKey(key);
    }

    SanitizeGraphicsSettings(&settings);
    return settings;
}

bool SaveGraphicsSettings(const GraphicsSettings& rawSettings)
{
    GraphicsSettings settings = rawSettings;
    SanitizeGraphicsSettings(&settings);

    HKEY key = nullptr;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, kRegPath, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
        return false;
    }

    SaveDwordSetting(key, kGraphicsWidthValue, settings.width);
    SaveDwordSetting(key, kGraphicsHeightValue, settings.height);
    SaveDwordSetting(key, kGraphicsWindowModeValue, static_cast<int>(settings.windowMode));
    SaveDwordSetting(key, kGraphicsTextureUpscaleValue, settings.textureUpscaleFactor);
    SaveDwordSetting(key, kGraphicsAnisotropicValue, settings.anisotropicLevel);
    SaveDwordSetting(key, kGraphicsAntiAliasingValue, static_cast<int>(settings.antiAliasing));
    RegCloseKey(key);
    return true;
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

bool DoesBackendSupportAntiAliasing(RenderBackendType backend)
{
    (void)backend;
    return false;
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
