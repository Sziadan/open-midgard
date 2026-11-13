#include "QuestDisplayCatalog.h"

#include "File.h"

#include <cctype>
#include <array>
#include <memory>
#include <string>

namespace {

std::string_view Trim(std::string_view value)
{
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.remove_prefix(1);
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.remove_suffix(1);
    }
    return value;
}

bool ParseId(std::string_view value, std::uint32_t& id)
{
    value = Trim(value);
    if (value.empty()) return false;

    std::uint32_t parsed = 0;
    for (char ch : value) {
        if (ch < '0' || ch > '9') return false;
        const std::uint32_t digit = static_cast<std::uint32_t>(ch - '0');
        if (parsed > (UINT32_MAX - digit) / 10) return false;
        parsed = parsed * 10 + digit;
    }
    id = parsed;
    return id != 0;
}

} // namespace

bool ro::QuestDisplayCatalog::Load()
{
    Clear();
    for (const char* path : {"questid2display.txt", "data\\questid2display.txt", "data/questid2display.txt"}) {
        int size = 0;
        std::unique_ptr<unsigned char[]> bytes(g_fileMgr.GetData(path, &size));
        if (!bytes || size <= 0) continue;
        return Parse(std::string_view(reinterpret_cast<const char*>(bytes.get()), static_cast<std::size_t>(size)));
    }
    return false;
}

bool ro::QuestDisplayCatalog::Parse(std::string_view text)
{
    Clear();
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF) {
        text.remove_prefix(3);
    }

    while (!(text = Trim(text)).empty()) {
        if (text.substr(0, 2) == "//") {
            const auto newline = text.find('\n');
            if (newline == text.npos) break;
            text.remove_prefix(newline + 1);
            continue;
        }
        std::array<std::string_view, 6> fields;
        for (auto& field : fields) {
            const auto end = text.find('#');
            if (end == text.npos) return !m_entries.empty();
            field = Trim(text.substr(0, end));
            text.remove_prefix(end + 1);
        }
        std::uint32_t id = 0;
        if (ParseId(fields[0], id)) {
            m_entries[id] = {std::string(fields[1]), std::string(fields[4]), std::string(fields[5])};
        }
    }
    return !m_entries.empty();
}

const ro::QuestDisplayEntry* ro::QuestDisplayCatalog::Find(std::uint32_t questId) const
{
    const auto it = m_entries.find(questId);
    return it == m_entries.end() ? nullptr : &it->second;
}
