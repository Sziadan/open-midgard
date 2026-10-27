#include <array>
#include <cassert>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;

struct PacketView {
    u16 packetId;
    u16 packetLength;
    const u8* data;
};

class CGameMode {};

struct ChatEvent {
    std::string text;
    u32 color;
    u8 channel;
};

class UIWindowMgr {
public:
    void PushChatEvent(const char* text, u32 color, u8 channel)
    {
        events.push_back({text ? text : "", color, channel});
    }

    std::vector<ChatEvent> events;
};

UIWindowMgr g_windowMgr;

u32 ReadLE32(const u8* data)
{
    return static_cast<u32>(data[0])
        | (static_cast<u32>(data[1]) << 8)
        | (static_cast<u32>(data[2]) << 16)
        | (static_cast<u32>(data[3]) << 24);
}

#include "SkillMessageBody.inc"

int main()
{
    const std::array<std::pair<u32, const char*>, 11> cases = {{
        {0x15, "All abnormal status effects have been removed."},
        {0x16, "You will be immune to abnormal status effects for the next minute."},
        {0x17, "Your Max HP will stay increased for the next minute."},
        {0x18, "Your Max SP will stay increased for the next minute."},
        {0x19, "All of your Stats will stay increased for the next minute."},
        {0x1C, "Your weapon will remain blessed with Holy power for the next minute."},
        {0x1D, "Your armor will remain blessed with Holy power for the next minute."},
        {0x1E, "Your Defense will stay increased for the next 10 seconds."},
        {0x1F, "Your Attack strength will be increased for the next minute."},
        {0x20, "Your Accuracy and Flee Rate will be increased for the next minute."},
        {0x28, "This target is fully coated, so Full Strip will not work."},
    }};

    CGameMode mode;
    for (const auto& [id, expectedText] : cases) {
        std::array<u8, 6> bytes{0x15, 0x02, 0, 0, 0, 0};
        bytes[2] = static_cast<u8>(id & 0xFF);
        bytes[3] = static_cast<u8>((id >> 8) & 0xFF);
        bytes[4] = static_cast<u8>((id >> 16) & 0xFF);
        bytes[5] = static_cast<u8>((id >> 24) & 0xFF);
        const PacketView packet{0x0215, 6, bytes.data()};
        HandleSkillMessage(mode, packet);
        assert(g_windowMgr.events.size() == 1);
        assert(g_windowMgr.events.back().text == expectedText);
        assert(g_windowMgr.events.back().color == 10198015u);
        assert(g_windowMgr.events.back().channel == 7);
        g_windowMgr.events.clear();
    }

    std::array<u8, 6> bytes{0x15, 0x02, 0x21, 0, 0, 0};
    HandleSkillMessage(mode, PacketView{0x0215, 6, bytes.data()});
    assert(g_windowMgr.events.empty());
    HandleSkillMessage(mode, PacketView{0x0215, 5, bytes.data()});
    assert(g_windowMgr.events.empty());
    HandleSkillMessage(mode, PacketView{0x0215, 6, nullptr});
    assert(g_windowMgr.events.empty());
    return 0;
}
