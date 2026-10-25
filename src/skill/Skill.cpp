// Skill.cpp - Skill data and usage logic
#include "Skill.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>

namespace {

#include "SkillEnumIdTable.inc"

constexpr const char* kUiKorPrefix =
    "texture\\"
    "\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA"
    "\\";

std::string TrimAscii(const std::string& value)
{
    const auto isSpace = [](unsigned char ch) {
        return std::isspace(ch) != 0;
    };

    size_t begin = 0;
    while (begin < value.size() && isSpace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }

    size_t end = value.size();
    while (end > begin && isSpace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    return value.substr(begin, end - begin);
}

std::string ToLowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        if (ch >= 'A' && ch <= 'Z') {
            return static_cast<char>(ch - 'A' + 'a');
        }
        return static_cast<char>(ch);
    });
    return value;
}

bool ReadLines(const std::filesystem::path& path, std::vector<std::string>& outLines)
{
    outLines.clear();
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return false;
    }

    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        outLines.push_back(line);
    }

    return true;
}

std::filesystem::path GetSkillDataRoot()
{
#ifdef RO_SOURCE_ROOT
    return std::filesystem::path(RO_SOURCE_ROOT) / "Ref" / "GRF-Content" / "data";
#else
    return std::filesystem::current_path() / "Ref" / "GRF-Content" / "data";
#endif
}

std::vector<std::string> SplitHashTokens(const std::string& line)
{
    std::vector<std::string> out;
    std::string current;
    for (char ch : line) {
        if (ch == '#') {
            out.push_back(current);
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    if (!current.empty()) {
        out.push_back(current);
    }
    return out;
}

} // namespace

CSkillMgr::CSkillMgr()
    : m_loaded(false)
{
}

CSkillMgr::~CSkillMgr() = default;

bool CSkillMgr::EnsureLoaded()
{
    if (m_loaded) {
        return !m_byId.empty();
    }
    m_loaded = true;
    return LoadSkillMetadata();
}

const SkillMetadata* CSkillMgr::GetSkillMetadata(int skillId) const
{
    const auto it = m_byId.find(skillId);
    if (it == m_byId.end()) {
        return nullptr;
    }
    return &it->second;
}

const SkillMetadata* CSkillMgr::GetSkillMetadataByName(const std::string& skillIdName) const
{
    const auto it = m_idBySkillName.find(ToLowerAscii(skillIdName));
    if (it == m_idBySkillName.end()) {
        return nullptr;
    }
    return GetSkillMetadata(it->second);
}

std::string CSkillMgr::GetSkillIconPath(int skillId) const
{
    const SkillMetadata* metadata = GetSkillMetadata(skillId);
    if (!metadata || metadata->skillIdName.empty()) {
        return std::string();
    }

    return std::string(kUiKorPrefix) + "item\\" + ToLowerAscii(metadata->skillIdName) + ".bmp";
}

bool CSkillMgr::LoadSkillMetadata()
{
    m_byId.clear();
    m_idBySkillName.clear();

    if (!BuildSkillIdMapFromNameTable()) {
        return false;
    }

    LoadSkillDisplayNames();
    LoadSkillDescriptions();
    LoadSkillLevelSpCosts();
    return !m_byId.empty();
}

bool CSkillMgr::BuildSkillIdMapFromNameTable()
{
    // Wire-format skill IDs match the server (eAthena `e_skill` / packet SKID), not
    // skillnametable.txt line order. Ref resolves names by ID; we embed the same enum IDs.
    m_byId.clear();
    m_idBySkillName.clear();
    for (size_t i = 0; i < kSkillEnumEntriesCount; ++i) {
        const SkillEnumEntry& e = kSkillEnumEntries[i];
        SkillMetadata metadata;
        metadata.skillId = e.id;
        metadata.skillIdName = e.name;
        m_byId[e.id] = std::move(metadata);
        m_idBySkillName[ToLowerAscii(std::string(e.name))] = e.id;
    }
    return !m_byId.empty();
}

void CSkillMgr::LoadSkillDisplayNames()
{
    std::vector<std::string> lines;
    if (!ReadLines(GetSkillDataRoot() / "skillnametable.txt", lines)) {
        return;
    }

    for (const std::string& rawLine : lines) {
        const std::string line = TrimAscii(rawLine);
        if (line.empty()) {
            continue;
        }

        const std::vector<std::string> tokens = SplitHashTokens(line);
        if (tokens.size() < 2) {
            continue;
        }

        const auto it = m_idBySkillName.find(ToLowerAscii(TrimAscii(tokens[0])));
        if (it == m_idBySkillName.end()) {
            continue;
        }

        SkillMetadata& metadata = m_byId[it->second];
        metadata.displayName = tokens[1];
        std::replace(metadata.displayName.begin(), metadata.displayName.end(), '_', ' ');
    }
}

void CSkillMgr::LoadSkillDescriptions()
{
    std::vector<std::string> lines;
    if (!ReadLines(GetSkillDataRoot() / "skilldesctable.txt", lines)) {
        return;
    }

    SkillMetadata* currentMetadata = nullptr;
    bool expectTitle = false;
    for (const std::string& rawLine : lines) {
        const std::string trimmed = TrimAscii(rawLine);
        if (trimmed.empty()) {
            continue;
        }

        if (trimmed == "#") {
            currentMetadata = nullptr;
            expectTitle = false;
            continue;
        }

        if (trimmed.size() > 1 && trimmed.back() == '#') {
            const std::string skillIdName = trimmed.substr(0, trimmed.size() - 1);
            const auto it = m_idBySkillName.find(ToLowerAscii(skillIdName));
            currentMetadata = (it == m_idBySkillName.end()) ? nullptr : &m_byId[it->second];
            expectTitle = true;
            continue;
        }

        if (!currentMetadata) {
            continue;
        }

        if (expectTitle) {
            if (currentMetadata->displayName.empty()) {
                currentMetadata->displayName = trimmed;
            }
            expectTitle = false;
            continue;
        }

        currentMetadata->descriptionLines.push_back(rawLine);
    }
}

void CSkillMgr::LoadSkillLevelSpCosts()
{
    std::vector<std::string> lines;
    if (!ReadLines(GetSkillDataRoot() / "leveluseskillspamount.txt", lines)) {
        return;
    }

    SkillMetadata* currentMetadata = nullptr;
    for (const std::string& rawLine : lines) {
        const std::string trimmed = TrimAscii(rawLine);
        if (trimmed.empty()) {
            continue;
        }

        if (trimmed == "@") {
            currentMetadata = nullptr;
            continue;
        }

        if (trimmed.size() > 1 && trimmed.back() == '#') {
            const std::string skillIdName = trimmed.substr(0, trimmed.size() - 1);
            const auto it = m_idBySkillName.find(ToLowerAscii(skillIdName));
            currentMetadata = (it == m_idBySkillName.end()) ? nullptr : &m_byId[it->second];
            continue;
        }

        if (!currentMetadata) {
            continue;
        }

        currentMetadata->levelSpCosts.push_back(std::atoi(trimmed.c_str()));
    }
}

CSkillMgr g_skillMgr;

namespace SKILL_INFO {

bool IsSkillNotDelayed(int skillId)
{
    if (skillId > 77) {
        if (skillId != 86 && skillId != 340 && skillId != 690) {
            return true;
        }
    } else if (skillId != 77) {
        switch (skillId) {
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
        case 17:
        case 18:
        case 19:
        case 20:
        case 21:
        case 27:
        case 29:
        case 30:
        case 32:
        case 33:
            return false;
        default:
            return true;
        }
    }
    return false;
}

bool IsGoodSkillForActor(int skillId)
{
    if (skillId <= 465) {
        if (skillId < 460) {
            if (skillId > 139) {
                switch (skillId) {
                case 231:
                case 234:
                case 235:
                case 236:
                case 237:
                case 255:
                case 256:
                case 262:
                case 280:
                case 281:
                case 282:
                case 283:
                case 361:
                case 445:
                case 446:
                case 447:
                case 448:
                case 449:
                case 450:
                case 451:
                case 452:
                case 453:
                case 454:
                case 455:
                case 456:
                case 457:
                case 458:
                    return true;
                default:
                    return false;
                }
            }
            if (skillId < 138) {
                switch (skillId) {
                case 4:
                case 28:
                case 29:
                case 34:
                case 35:
                case 53:
                case 54:
                case 66:
                case 67:
                case 68:
                case 71:
                case 72:
                case 73:
                case 108:
                    return true;
                default:
                    return false;
                }
            }
        }
        return true;
    }
    if (skillId <= 626) {
        if (skillId != 626) {
            switch (skillId) {
            case 479:
            case 494:
            case 564:
            case 572:
            case 573:
            case 574:
            case 575:
            case 577:
            case 593:
            case 621:
            case 622:
                return true;
            default:
                return false;
            }
        }
        return true;
    }
    if (skillId <= 691) {
        if (skillId < 688) {
            switch (skillId) {
            case 636:
            case 637:
            case 641:
            case 642:
            case 643:
            case 645:
            case 646:
                return true;
            default:
                return false;
            }
        }
        return true;
    }
    if (skillId > 1015) {
        if (skillId < 8226 || skillId > 8231) {
            return false;
        }
    } else if (skillId != 1015 && skillId != 694) {
        return false;
    }
    return true;
}

}
