#include <array>
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using s16 = std::int16_t;
using u32 = std::uint32_t;
using msgparam_t = std::intptr_t;

struct PacketView {
    u16 packetId;
    u16 packetLength;
    const u8* data;
};

struct SHOW_IMAGE_INFO {
    std::string name;
    int type = -1;
    int x = 0;
    int y = 0;
};

class CGameMode {
public:
    SHOW_IMAGE_INFO m_showImageInfo;

    int SendMsg(int message, msgparam_t wparam, msgparam_t lparam, msgparam_t extra);
};

u16 ReadLE16(const u8* data)
{
    return static_cast<u16>(data[0]) | static_cast<u16>(data[1] << 8);
}

u32 ReadLE32(const u8* data)
{
    return static_cast<u32>(data[0])
        | (static_cast<u32>(data[1]) << 8)
        | (static_cast<u32>(data[2]) << 16)
        | (static_cast<u32>(data[3]) << 24);
}

void UpdateShowImageInfo(CGameMode& mode, const char* imageName, int type);
#include "ShowImageStateBody.inc"

int CGameMode::SendMsg(int message, msgparam_t wparam, msgparam_t lparam, msgparam_t extra)
{
    if (message == 113) {
        UpdateShowImageInfo(*this, reinterpret_cast<const char*>(wparam), static_cast<int>(lparam));
    }
    return 0;
}

#include "ShowImagePacketBody.inc"

void PutName(std::array<u8, 19>& bytes, const char* name)
{
    bytes.fill(0);
    bytes[0] = 0x45;
    bytes[1] = 0x01;
    std::memcpy(bytes.data() + 2, name, (std::min)(std::strlen(name), size_t{16}));
}

int main()
{
    CGameMode mode;
    std::array<u8, 19> bytes{};
    PutName(bytes, "notice");
    bytes[18] = 255;
    HandleShowImage(mode, PacketView{0x0145, 19, bytes.data()});
    assert(mode.m_showImageInfo.name == "notice.bmp");
    assert(mode.m_showImageInfo.type == -1);

    PutName(bytes, "portrait.bmp");
    bytes[18] = 2;
    HandleShowImage(mode, PacketView{0x0145, 19, bytes.data()});
    assert(mode.m_showImageInfo.name == "portrait.bmp");
    assert(mode.m_showImageInfo.type == 2);

    PutName(bytes, "upper.BMP");
    bytes[18] = 3;
    HandleShowImage(mode, PacketView{0x0145, 19, bytes.data()});
    assert(mode.m_showImageInfo.name == "upper.BMP");
    assert(mode.m_showImageInfo.type == 3);

    PutName(bytes, "mixed.Bmp");
    bytes[18] = 4;
    HandleShowImage(mode, PacketView{0x0145, 19, bytes.data()});
    assert(mode.m_showImageInfo.name == "mixed.Bmp.bmp");

    const std::string previousName = mode.m_showImageInfo.name;
    HandleShowImage(mode, PacketView{0x0145, 18, bytes.data()});
    HandleShowImage(mode, PacketView{0x0145, 19, nullptr});
    assert(mode.m_showImageInfo.name == previousName);
    return 0;
}
