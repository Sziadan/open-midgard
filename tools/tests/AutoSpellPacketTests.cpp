#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
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

class CGameMode {
public:
    u16 m_warpSkillId = 0;
    std::vector<std::string> m_warpMapDestinations;
};

struct UIWindow {
    virtual ~UIWindow() = default;
};

struct UISkillSelectWnd : UIWindow {
    std::vector<unsigned int> skillIds;
    bool autoSpell = false;
    void SetAutoSpellSkills(const std::vector<unsigned int>& values) { skillIds = values; autoSpell = true; }
    bool IsAutoSpellSelection() const { return autoSpell; }
};

struct UIWindowMgr {
    enum { WID_SKILLSELECTWND = 100 };
    int makeWindowCalls = 0;
    UIWindow* window = nullptr;
    UISkillSelectWnd* m_skillSelectWnd = nullptr;
    UIWindow* MakeWindow(int windowId)
    {
        assert(windowId == WID_SKILLSELECTWND);
        ++makeWindowCalls;
        return window;
    }
    void DeleteWindow(UIWindow* value) { if (value == m_skillSelectWnd) m_skillSelectWnd = nullptr; }
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

    UISkillSelectWnd window;
    g_windowMgr.window = &window;
    g_windowMgr.m_skillSelectWnd = &window;
    WriteLE32(bytes.data() + 2, 5101);
    WriteLE32(bytes.data() + 6, 0);
    WriteLE32(bytes.data() + 10, 5102);
    WriteLE32(bytes.data() + 26, 5107);
    HandleAutoSpellList(mode, PacketView{0x01CD, 30, bytes.data()});
    assert(g_windowMgr.makeWindowCalls == 1);
    assert(window.skillIds == std::vector<unsigned int>({5101, 5102, 5107}));

    // A replacement snapshot replaces earlier entries and retains packet order.
    bytes.fill(0);
    WriteLE32(bytes.data() + 14, 5203);
    WriteLE32(bytes.data() + 26, 5207);
    HandleAutoSpellList(mode, PacketView{0x01CD, 30, bytes.data()});
    assert(window.skillIds == std::vector<unsigned int>({5203, 5207}));

    // An all-zero snapshot closes an existing auto-spell selector.
    bytes.fill(0);
    HandleAutoSpellList(mode, PacketView{0x01CD, 30, bytes.data()});
    assert(g_windowMgr.m_skillSelectWnd == nullptr);

    // IDs outside the server's u16 skill selection domain are not offered.
    g_windowMgr = {};
    g_windowMgr.window = &window;
    WriteLE32(bytes.data() + 2, 0x10000);
    HandleAutoSpellList(mode, PacketView{0x01CD, 30, bytes.data()});
    assert(g_windowMgr.makeWindowCalls == 0);

    g_windowMgr = {};
    g_windowMgr.window = nullptr;
    bytes.fill(0);
    WriteLE32(bytes.data() + 2, 5200);
    HandleAutoSpellList(mode, PacketView{0x01CD, 30, bytes.data()});
    assert(g_windowMgr.makeWindowCalls == 1); // failed creation suppresses messages

    g_windowMgr = {};
    HandleAutoSpellList(mode, PacketView{0x01CD, 29, bytes.data()});
    HandleAutoSpellList(mode, PacketView{0x01CD, 30, nullptr});
    assert(g_windowMgr.makeWindowCalls == 0);
    HandleAutoSpellList(mode, PacketView{0x01CE, 30, bytes.data()});
    assert(g_windowMgr.makeWindowCalls == 0);
    return 0;
}
