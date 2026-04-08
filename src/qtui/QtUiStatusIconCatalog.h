#pragma once

namespace qtui {

struct StatusIconCatalogEntry {
    int statusType;
    int skillId;
    const char* fallbackLabel;
    const char* legacyIconFileName;
};

constexpr char kStatusIconAngelus[] = "\xBE\xC8\xC1\xA9\xB7\xE7\xBD\xBA\x2E\x74\x67\x61";
constexpr char kStatusIconBlessing[] = "\xBA\xED\xB7\xB9\xBD\xCC\x2E\x74\x67\x61";
constexpr char kStatusIconIncreaseAgi[] = "\xB9\xCE\xC3\xB8\xBC\xBA\xC1\xF5\xB0\xA1\x2E\x74\x67\x61";
constexpr char kStatusIconImproveConcentration[] = "\xC1\xFD\xC1\xDF\xB7\xC2\xC7\xE2\xBB\xF3\x2E\x74\x67\x61";
constexpr char kStatusIconImpositioManus[] = "\xC0\xD3\xC6\xF7\xBD\xC3\xC6\xBC\xBF\xC0\xB8\xB6\xB4\xA9\xBD\xBA\x2E\x74\x67\x61";
constexpr char kStatusIconSuffragium[] = "\xBC\xF6\xC7\xC1\xB6\xF3\xB1\xE2\xBF\xF2\x2E\x74\x67\x61";
constexpr char kStatusIconAspersio[] = "\xBE\xC6\xBD\xBA\xC6\xE4\xB8\xA3\xBD\xC3\xBF\xC0\x2E\x74\x67\x61";
constexpr char kStatusIconKyrieEleison[] = "\xB1\xE2\xB8\xAE\xBF\xA1\xBF\xA4\xB7\xB9\xC0\xCC\xBC\xD5\x2E\x74\x67\x61";
constexpr char kStatusIconMagnificat[] = "\xB8\xB6\xB4\xCF\xC7\xC7\xC4\xB1\x2E\x74\x67\x61";
constexpr char kStatusIconGloria[] = "\xB1\xDB\xB7\xCE\xB8\xAE\xBE\xC6\x2E\x74\x67\x61";
constexpr char kStatusIconAdrenalineRush[] = "\xBE\xC6\xB5\xE5\xB7\xB9\xB3\xAF\xB8\xB0\xB7\xAF\xBD\xAC\x2E\x74\x67\x61";
constexpr char kStatusIconWeaponPerfection[] = "\xBF\xFE\xC6\xF9\xC6\xDB\xC6\xE5\xBC\xC7\x2E\x74\x67\x61";
constexpr char kStatusIconOverThrust[] = "\xBF\xC0\xB9\xF6\xC6\xAE\xB7\xAF\xBD\xBA\xC6\xAE\x2E\x74\x67\x61";
constexpr char kStatusIconEnergyCoat[] = "\xBF\xA1\xB3\xCA\xC1\xF6\xC4\xDA\xC6\xAE\x2E\x74\x67\x61";
constexpr char kStatusIconChemicalProtectionWeapon[] = "\xC4\xC9\xB9\xCC\xC4\xC3\xC7\xC1\xB7\xCE\xC5\xD8\xBC\xC7\x5B\xBF\xFE\xC6\xF9\x5D\x2E\x74\x67\x61";
constexpr char kStatusIconChemicalProtectionShield[] = "\xC4\xC9\xB9\xCC\xC4\xC3\xC7\xC1\xB7\xCE\xC5\xD8\xBC\xC7\x5B\xBD\xAF\xB5\xE5\x5D\x2E\x74\x67\x61";
constexpr char kStatusIconChemicalProtectionArmor[] = "\xC4\xC9\xB9\xCC\xC4\xC3\xC7\xC1\xB7\xCE\xC5\xD8\xBC\xC7\x5B\xBE\xC6\xB8\xD3\x5D\x2E\x74\x67\x61";
constexpr char kStatusIconChemicalProtectionHelm[] = "\xC4\xC9\xB9\xCC\xC4\xC3\xC7\xC1\xB7\xCE\xC5\xD8\xBC\xC7\x5B\xC7\xEF\xB8\xA7\x5D\x2E\x74\x67\x61";
constexpr char kStatusIconAutoGuard[] = "\xBF\xC0\xC5\xE4\xB0\xA1\xB5\xE5\x2E\x74\x67\x61";
constexpr char kStatusIconReflectShield[] = "\xB8\xAE\xC7\xC3\xB7\xBA\xC6\xAE\xBD\xAF\xB5\xE5\x2E\x74\x67\x61";
constexpr char kStatusIconSpearQuicken[] = "\xBD\xBA\xC7\xC7\xBE\xEE\xC4\xFB\xC5\xAB\x2E\x74\x67\x61";
constexpr char kStatusIconTwoHandQuicken[] = "\xC5\xF5\xC7\xDA\xB5\xE5\xC4\xFB\xC5\xAB\x2E\x74\x67\x61";

constexpr StatusIconCatalogEntry kStatusIconCatalog[] = {
    {1, 8, "Endure", "endure.tga"},
    {2, 60, "Two-Hand Quicken", kStatusIconTwoHandQuicken},
    {3, 45, "Improve Concentration", kStatusIconImproveConcentration},
    {9, 33, "Angelus", kStatusIconAngelus},
    {10, 34, "Blessing", kStatusIconBlessing},
    {12, 29, "Increase Agi", kStatusIconIncreaseAgi},
    {15, 66, "Impositio Manus", kStatusIconImpositioManus},
    {16, 67, "Suffragium", kStatusIconSuffragium},
    {17, 68, "Aspersio", kStatusIconAspersio},
    {19, 73, "Kyrie Eleison", kStatusIconKyrieEleison},
    {20, 74, "Magnificat", kStatusIconMagnificat},
    {21, 75, "Gloria", kStatusIconGloria},
    {23, 111, "Adrenaline Rush", kStatusIconAdrenalineRush},
    {24, 112, "Weapon Perfection", kStatusIconWeaponPerfection},
    {25, 113, "Over Thrust", kStatusIconOverThrust},
    {31, 157, "Energy Coat", kStatusIconEnergyCoat},
    {54, 234, "Chemical Protection Weapon", kStatusIconChemicalProtectionWeapon},
    {55, 235, "Chemical Protection Shield", kStatusIconChemicalProtectionShield},
    {56, 236, "Chemical Protection Armor", kStatusIconChemicalProtectionArmor},
    {57, 237, "Chemical Protection Helm", kStatusIconChemicalProtectionHelm},
    {58, 249, "Auto Guard", kStatusIconAutoGuard},
    {59, 252, "Reflect Shield", kStatusIconReflectShield},
    {68, 258, "Spear Quicken", kStatusIconSpearQuicken},
    {110, 361, "Assumptio", nullptr},
};

inline const StatusIconCatalogEntry* FindStatusIconCatalogEntry(int statusType)
{
    for (const StatusIconCatalogEntry& entry : kStatusIconCatalog) {
        if (entry.statusType == statusType) {
            return &entry;
        }
    }
    return nullptr;
}

} // namespace qtui