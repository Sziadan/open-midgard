#pragma once

#include "Types.h"
#include <filesystem>
#include <string>
#include <vector>

class ProhibitionHistory {
public:
    struct Row {
        std::string account, character, server;
        u32 aid = 0, gid = 0, duration = 0, date = 0, time = 0;
    };
    static constexpr const char* Unassigned = "not Assigned AccountName";

    bool Load(const std::filesystem::path& path);
    bool Save() const;
    void Append(u32 aid, u32 gid, u32 duration, std::string server, std::string character,
        const std::string* account);
    bool RemoveLatest(u32 aid, const std::string& server, u32 duration);
    bool Assign(u32 aid, const std::string& server, const std::string& account);
    const std::vector<Row>& Rows() const { return m_rows; }

private:
    std::filesystem::path m_path;
    std::vector<Row> m_rows;
};
