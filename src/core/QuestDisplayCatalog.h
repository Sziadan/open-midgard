#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

namespace ro {

// Text entries from questid2display.txt. The resource stores one ID and five
// display strings per record: title, icon, illustration, summary and objective.
struct QuestDisplayEntry {
    std::string title;
    std::string summary;
    std::string objective;
};

class QuestDisplayCatalog {
public:
    // Loads the installed resource through g_fileMgr, replacing any prior data.
    bool Load();
    // Parses six hash-delimited fields per record (ID plus five strings).
    bool Parse(std::string_view text);
    const QuestDisplayEntry* Find(std::uint32_t questId) const;
    void Clear() { m_entries.clear(); }

private:
    std::unordered_map<std::uint32_t, QuestDisplayEntry> m_entries;
};

} // namespace ro
