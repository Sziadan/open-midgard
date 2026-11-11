#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace ro::chat {
enum class NoticeRoute { Unspecified = 0, Normal = 1, Command = 20, NormalAndCommand };
enum class NoticeType {
    None = 0, Chat = 1, Whisper = 2, Party = 3, Guild = 4, ObtainItem = 5,
    Equip = 6, Status = 7, PartyItem = 8, PartyStatus = 9, FailedSkill = 10,
    PartySetup = 11, DamagedEquipment = 12, Siege = 13
};
struct NoticeSource {
    NoticeRoute route = NoticeRoute::Unspecified;
    NoticeType type = NoticeType::None;
    std::string talker;
    unsigned long long mainGeneration = 0;
};

// CutString(94), with the port's UTF-8 character boundaries replacing CharNextExA.
// The original byte-distance threshold is not a hard maximum: a space run or
// multibyte boundary can cross it. Preserve every byte, including trailing spaces.
inline std::vector<std::string> SplitNotice(std::string_view text, std::size_t threshold = 94)
{
    if (threshold == 0) threshold = 1;
    std::vector<std::string> result;
    std::size_t start = 0, cursor = 0, lastSpace = 0;
    while (cursor < text.size()) {
        ++cursor;
        while (cursor < text.size() && (static_cast<unsigned char>(text[cursor]) & 0xc0) == 0x80) ++cursor;
        if (cursor < text.size() && text[cursor] == ' ') {
            do { lastSpace = ++cursor; } while (cursor < text.size() && text[cursor] == ' ');
        }
        if (cursor - start >= threshold) {
            const auto end = lastSpace > start ? lastSpace : cursor;
            result.emplace_back(text.substr(start, end - start));
            start = end;
        }
    }
    if (start < cursor) result.emplace_back(text.substr(start, cursor - start));
    return result;
}

inline unsigned int ColorRefToUiRgb(unsigned int value)
{
    return ((value & 0xff) << 16) | (value & 0xff00) | ((value >> 16) & 0xff);
}
inline bool IsNoticeSentinel(std::string_view text)
{
    return text == "NO MSG" || text == "No Msg";
}
} // namespace ro::chat
