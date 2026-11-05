#pragma once

#include <filesystem>
#include <string>

std::filesystem::path GetOpenMidgardIniPath();
void EnsureOpenMidgardIniDefaults();
bool HasSettingsIniSection(const char* section);
bool TryLoadSettingsIniInt(const char* section, const char* key, int* value);
bool TryLoadSettingsIniString(const char* section, const char* key, std::string* value);
int LoadSettingsIniInt(const char* section, const char* key, int defaultValue);
std::string LoadSettingsIniString(const char* section, const char* key, const char* defaultValue);
bool SaveSettingsIniInt(const char* section, const char* key, int value);
bool SaveSettingsIniString(const char* section, const char* key, const std::string& value);
