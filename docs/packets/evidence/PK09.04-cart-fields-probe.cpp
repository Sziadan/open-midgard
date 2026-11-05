#include <cstddef>
#include <cstdio>
#include "server_packets.hpp"
static_assert(PACKETVER == 20080910);
static_assert(sizeof(NORMALITEM_INFO) == 22);
static_assert(sizeof(EQUIPITEM_INFO) == 26);
static_assert(offsetof(EQUIPITEM_INFO, WearState) == 8);
static_assert(offsetof(EQUIPITEM_INFO, HireExpireDate) == 20);
static_assert(offsetof(EQUIPITEM_INFO, bindOnEquipType) == 24);
int main() {
    std::printf("PACKETVER=%d normal=%zu equipment=%zu wear=%zu expiry=%zu bind=%zu\n",
        PACKETVER, sizeof(NORMALITEM_INFO), sizeof(EQUIPITEM_INFO),
        offsetof(EQUIPITEM_INFO, WearState), offsetof(EQUIPITEM_INFO, HireExpireDate),
        offsetof(EQUIPITEM_INFO, bindOnEquipType));
}
