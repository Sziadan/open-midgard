#include "ProhibitionHistory.h"

#include <ctime>
#include <fstream>

namespace {
constexpr u32 maxRows = 100000;

bool ReadWord(std::istream& in, u32& value)
{
    unsigned char bytes[4];
    if (!in.read(reinterpret_cast<char*>(bytes), sizeof(bytes))) return false;
    value = u32(bytes[0]) | u32(bytes[1]) << 8 | u32(bytes[2]) << 16 | u32(bytes[3]) << 24;
    return true;
}

void WriteWord(std::ostream& out, u32 value)
{
    for (int shift = 0; shift < 32; shift += 8) out.put(static_cast<char>(value >> shift));
}

bool ReadField(std::istream& in, std::string& value)
{
    u32 size = 0;
    char field[32];
    if (!ReadWord(in, size) || size == 0 || size > sizeof(field) || !in.read(field, size)
        || field[size - 1] != '\0') return false;
    value.assign(field, size - 1);
    return value.find('\0') == std::string::npos;
}

void WriteField(std::ostream& out, const std::string& value)
{
    WriteWord(out, static_cast<u32>(value.size() + 1));
    out.write(value.c_str(), value.size() + 1);
}
}

bool ProhibitionHistory::Load(const std::filesystem::path& path)
{
    if (m_path == path) return true;
    std::error_code error;
    if (!std::filesystem::exists(path, error) && !error) {
        m_path = path;
        m_rows.clear();
        return true;
    }
    std::ifstream in(path, std::ios::binary);
    u32 count = 0;
    if (!ReadWord(in, count) || count > maxRows) return false;
    std::vector<Row> rows(count);
    for (auto& row : rows) {
        if (!ReadField(in, row.account) || !ReadField(in, row.character) || !ReadField(in, row.server)
            || !ReadWord(in, row.aid) || !ReadWord(in, row.gid) || !ReadWord(in, row.duration)
            || !ReadWord(in, row.date) || !ReadWord(in, row.time)) return false;
    }
    if (in.peek() != std::char_traits<char>::eof()) return false;
    m_path = path;
    m_rows = std::move(rows);
    return true;
}

bool ProhibitionHistory::Save() const
{
    if (m_path.empty()) return false;
    std::error_code error;
    if (m_path.has_parent_path()) std::filesystem::create_directories(m_path.parent_path(), error);
    if (error) return false;
    auto temporary = m_path;
    temporary += ".tmp";
    std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
    WriteWord(out, static_cast<u32>(m_rows.size()));
    for (const auto& row : m_rows) {
        WriteField(out, row.account);
        WriteField(out, row.character);
        WriteField(out, row.server);
        for (u32 value : {row.aid, row.gid, row.duration, row.date, row.time}) WriteWord(out, value);
    }
    out.close();
    if (out) {
        std::filesystem::rename(temporary, m_path, error);
        if (!error) return true;
    }
    std::filesystem::remove(temporary, error);
    return false;
}

void ProhibitionHistory::Append(u32 aid, u32 gid, u32 duration, std::string server,
    std::string character, const std::string* account)
{
    if (m_rows.size() >= maxRows) return;
    const auto now = std::time(nullptr);
    std::tm timestamp{};
    if (const auto* local = std::localtime(&now)) timestamp = *local;
    const u32 date = u32(timestamp.tm_year + 1900) << 16 | u32(timestamp.tm_mon + 1) << 8 | u32(timestamp.tm_mday);
    const u32 time = u32(timestamp.tm_hour) << 16 | u32(timestamp.tm_min) << 8 | u32(timestamp.tm_sec);
    m_rows.push_back({account ? account->substr(0, 31) : Unassigned, character.substr(0, 31),
        server.substr(0, 31), aid, gid, duration, date, time});
}

bool ProhibitionHistory::RemoveLatest(u32 aid, const std::string& server, u32 duration)
{
    for (auto row = m_rows.end(); row != m_rows.begin();) {
        --row;
        if (row->aid != aid || row->server != server) continue;
        if (row->duration > duration) return false;
        m_rows.erase(row);
        return true;
    }
    return false;
}

bool ProhibitionHistory::Assign(u32 aid, const std::string& server, const std::string& account)
{
    bool changed = false;
    for (auto& row : m_rows) {
        if (row.aid != aid || row.server != server || row.account == account) continue;
        row.account = account.substr(0, 31);
        changed = true;
    }
    return changed;
}
