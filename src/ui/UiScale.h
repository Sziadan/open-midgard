#pragma once

constexpr int kUiScaleMinPercent = 50;
constexpr int kUiScaleMaxPercent = 200;
constexpr int kUiScaleDefaultPercent = 100;

int ClampUiScalePercent(int percent);
int GetConfiguredUiScalePercent();
float GetConfiguredUiScaleFactor();
void SetRuntimeUiScalePercent(int percent);
void SaveConfiguredUiScalePercent(int percent);
int UiScaleRawToLogicalCoordinate(int rawValue);