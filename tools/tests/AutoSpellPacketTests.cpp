#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using msgparam_t = std::intptr_t;

struct PacketView {
    u16 packetId;
    u16 packetLength;
    const u8* data;
};

class CGameMode {};

struct UIWindow {
    struct Message { int id; msgparam_t value; };
    std::vector<Message> messages;
    int SendMsg(UIWindow*, int message, msgparam_t wparam, msgparam_t, msgparam_t)
    {
        messages.push_back({message, wparam});
        return 0;
    }
};

struct UIWindowMgr {
    enum { WID_SPELLLISTWND = 99 };
    int makeWindowCalls = 0;
    UIWindow* window = nullptr;
    UIWindow* MakeWindow(int windowId)
    {
        assert(windowId == WID_SPELLLISTWND);
        ++makeWindowCalls;
        return window;
    }
};

UIWindowMgr g_windowMgr;

u32 ReadLE32(const u8* data)
{
    return static_cast<u32>(data[0])
        | (static_cast<u32>(data[1]) << 8)
        | (static_cast<u32>(data[2]) << 16)
        | (static_cast<u32>(data[3]) << 24);
}

#include "AutoSpellPacketBody.inc"

void WriteLE32(u8* data, u32 value)
{
    data[0] = static_cast<u8>(value);
    data[1] = static_cast<u8>(value >> 8);
    data[2] = static_cast<u8>(value >> 16);
    data[3] = static_cast<u8>(value >> 24);
}

int main()
{
    CGameMode mode;
    std::array<u8, 30> bytes{};
    g_windowMgr = {};
    HandleAutoSpellList(mode, PacketView{0x01CD, 30, bytes.data()});
    assert(g_windowMgr.makeWindowCalls == 0); // all seven IDs are zero

    UIWindow window;
    g_windowMgr.window = &window;
    WriteLE32(bytes.data() + 2, 5101);
    WriteLE32(bytes.data() + 6, 0);
    WriteLE32(bytes.data() + 10, 5102);
    WriteLE32(bytes.data() + 26, 5107);
    HandleAutoSpellList(mode, PacketView{0x01CD, 30, bytes.data()});
    assert(g_windowMgr.makeWindowCalls == 1);
    assert(window.messages.size() == 3);
    assert(window.messages[0].id == 31 && window.messages[0].value == 5101);
    assert(window.messages[1].id == 31 && window.messages[1].value == 5102);
    assert(window.messages[2].id == 31 && window.messages[2].value == 5107);

    g_windowMgr = {};
    WriteLE32(bytes.data() + 2, 0);
    HandleAutoSpellList(mode, PacketView{0x01CD, 30, bytes.data()});
    assert(g_windowMgr.makeWindowCalls == 0); // no window for an empty list

    g_windowMgr = {};
    g_windowMgr.window = nullptr;
    WriteLE32(bytes.data() + 2, 5200);
    HandleAutoSpellList(mode, PacketView{0x01CD, 30, bytes.data()});
    assert(g_windowMgr.makeWindowCalls == 1); // failed creation suppresses messages

    g_windowMgr = {};
    HandleAutoSpellList(mode, PacketView{0x01CD, 29, bytes.data()});
    HandleAutoSpellList(mode, PacketView{0x01CD, 30, nullptr});
    assert(g_windowMgr.makeWindowCalls == 0);
    return 0;
}
