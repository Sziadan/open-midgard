#include "UiScale.h"

#include "core/SettingsIni.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr char kUiScaleSection[] = "OptionWnd";
constexpr char kUiScalePercentValue[] = "GuiScalePercent";

int g_uiScalePercent = -1;

int LoadUiScalePercentFromSettings()
{
    return ClampUiScalePercent(LoadSettingsIniInt(kUiScaleSection, kUiScalePercentValue, kUiScaleDefaultPercent));
}

}

int ClampUiScalePercent(int percent)
{
    return (std::max)(kUiScaleMinPercent, (std::min)(kUiScaleMaxPercent, percent));
}

int GetConfiguredUiScalePercent()
{
    if (g_uiScalePercent < 0) {
        g_uiScalePercent = LoadUiScalePercentFromSettings();
    }
    return g_uiScalePercent;
}

float GetConfiguredUiScaleFactor()
{
    return static_cast<float>(GetConfiguredUiScalePercent()) / 100.0f;
}

void SetRuntimeUiScalePercent(int percent)
{
    g_uiScalePercent = ClampUiScalePercent(percent);
}

void SaveConfiguredUiScalePercent(int percent)
{
    const int clampedPercent = ClampUiScalePercent(percent);
    g_uiScalePercent = clampedPercent;
    SaveSettingsIniInt(kUiScaleSection, kUiScalePercentValue, clampedPercent);
}

int UiScaleRawToLogicalCoordinate(int rawValue)
{
    const float factor = GetConfiguredUiScaleFactor();
    if (factor <= 0.0f || std::abs(factor - 1.0f) < 0.0001f) {
        return rawValue;
    }

    return static_cast<int>(std::floor(static_cast<double>(rawValue) / static_cast<double>(factor)));
}