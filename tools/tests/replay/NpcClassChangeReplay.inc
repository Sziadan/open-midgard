class NpcReplayBillboardTexture final : public CTexture {
public:
    explicit NpcReplayBillboardTexture(int& destructions) : destructions_(destructions) {}
    ~NpcReplayBillboardTexture() override { ++destructions_; }
private:
    int& destructions_;
};

void NpcClassChangeReplay()
{
    SetCase("npc_class_change_replay", "PK20.02", "selected-20080910-npc-class-change-01b0",
        "production game-mode packet router -> 0x01B0 NPC class change");

    CGameMode mode;
    CWorld world;
    mode.m_world = &world;
    int borrowedTextureDestructions = 0;
    int ownedTextureDestructions = 0;
    NpcReplayBillboardTexture borrowedTexture(borrowedTextureDestructions);
    CPc npc;
    CMsgEffect child;
    npc.m_gid = 0x11223344u;
    npc.m_job = 100;
    npc.m_sex = 1;
    npc.m_bodyPalette = 3;
    npc.m_xSize = 17;
    npc.m_ySize = 23;
    npc.m_pos = vector3d{ 4.0f, 5.0f, 6.0f };
    npc.m_isVisible = 1;
    npc.m_isVisibleBody = 1;
    npc.m_curMotion = 9;
    child.m_masterGid = npc.m_gid;
    child.m_masterActor = &npc;
    npc.m_msgEffectList.push_back(&child);
    world.m_actorList.push_back(&npc);
    mode.m_runtimeActors.emplace(npc.m_gid, &npc);
    npc.m_billboardTexture = &borrowedTexture;
    npc.m_billboardTextureOwned = 0;

    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    const auto dispatch = [&](u32 gid, u32 job, u8 ignoredByte = 0) {
        const std::vector<u8> bytes = { 0xb0, 0x01,
            u8(gid), u8(gid >> 8), u8(gid >> 16), u8(gid >> 24), ignoredByte,
            u8(job), u8(job >> 8), u8(job >> 16), u8(job >> 24) };
        PacketView packet;
        int consumed = 0;
        Require(TryReadPacket(bytes.data(), static_cast<int>(bytes.size()), packet, consumed)
            && consumed == static_cast<int>(bytes.size()), "01B0 fixture did not parse as an 11-byte packet");
        Require(router.Dispatch(mode, packet), "production router did not register NPC class-change packet 01B0");
    };

    dispatch(npc.m_gid, 100);
    Require(npc.m_job == 100 && npc.m_billboardTexture == &borrowedTexture,
        "same-class 01B0 was not a no-op");

    const vector3d originalPosition = npc.m_pos;
    world.m_billboardFrameCacheValid = true;
    dispatch(npc.m_gid, 101, 0xa5);
    Require(npc.m_job == 101, "01B0 did not update the existing NPC class");
    Require(npc.m_sex == 0 && npc.m_bodyPalette == 0,
        "01B0 did not reset the NPC appearance sex and palette");
    Require(world.GetGameActorByAID(npc.m_gid) == &npc && mode.m_runtimeActors.at(npc.m_gid) == &npc
        && npc.m_msgEffectList.size() == 1 && npc.m_msgEffectList.front() == &child
        && child.m_masterGid == npc.m_gid && child.m_masterActor == &npc,
        "01B0 replaced the actor or lost its attached effect");
    Require(npc.m_xSize == 17 && npc.m_ySize == 23 && npc.m_pos.x == originalPosition.x
        && npc.m_pos.y == originalPosition.y && npc.m_pos.z == originalPosition.z
        && npc.m_isVisible == 1 && npc.m_isVisibleBody == 1 && npc.m_curMotion == 9,
        "01B0 changed NPC dimensions, position, visibility, or motion state");
    Require(npc.m_billboardTexture == nullptr && borrowedTextureDestructions == 0,
        "class change did not discard the borrowed billboard without deleting it");
    Require(!world.m_billboardFrameCacheValid && world.m_billboardFrameCacheDirty,
        "class change did not invalidate cached billboard geometry");

    const size_t deferredActorCount = mode.m_runtimeActors.size();
    const size_t seenActorCount = mode.m_aidList.size();
    dispatch(0x55667788u, 102);
    Require(world.m_actorList.size() == 1 && npc.m_job == 101
        && mode.m_runtimeActors.size() == deferredActorCount && mode.m_aidList.size() == seenActorCount,
        "01B0 created deferred actor state for an unknown GID");

    const std::array<u8, 10> truncatedBytes = { 0xb0, 0x01, 0x44, 0x33, 0x22, 0x11, 0, 102, 0, 0 };
    PacketView truncatedPacket{};
    int truncatedConsumed = 0;
    Require(!TryReadPacket(truncatedBytes.data(), static_cast<int>(truncatedBytes.size()),
        truncatedPacket, truncatedConsumed), "framer accepted a truncated 01B0");
    truncatedPacket.packetId = 0x01b0;
    truncatedPacket.packetLength = static_cast<u16>(truncatedBytes.size());
    truncatedPacket.data = truncatedBytes.data();
    Require(router.Dispatch(mode, truncatedPacket) && npc.m_job == 101,
        "malformed 01B0 changed the existing NPC");
    dispatch(npc.m_gid, 0x80000000u);
    Require(npc.m_job == 101, "01B0 accepted a class value outside the int range");

    // 01D7 carries look changes and remains independently routed from 01B0.
    const std::vector<u8> lookBytes = { 0xd7, 0x01,
        u8(npc.m_gid), u8(npc.m_gid >> 8), u8(npc.m_gid >> 16), u8(npc.m_gid >> 24),
        1, 0x34, 0x12, 0, 0 };
    PacketView lookPacket;
    int lookConsumed = 0;
    Require(TryReadPacket(lookBytes.data(), static_cast<int>(lookBytes.size()), lookPacket, lookConsumed)
        && lookConsumed == static_cast<int>(lookBytes.size()) && router.Dispatch(mode, lookPacket),
        "01D7 look packet did not dispatch independently");
    Require(npc.m_headType == 0x1234 && npc.m_job == 101,
        "01D7 hair look update did not remain independent from NPC class change");

    npc.m_billboardTexture = new NpcReplayBillboardTexture(ownedTextureDestructions);
    npc.m_billboardTextureOwned = 1;
    dispatch(npc.m_gid, 1285);
    Require(npc.m_job == 1285 && npc.m_billboardTexture == nullptr && ownedTextureDestructions == 1,
        "sprite-to-GR2 did not release the owned billboard exactly once");
    dispatch(npc.m_gid, 1288);
    Require(npc.m_job == 1288, "GR2-to-GR2 did not change the class");
    dispatch(npc.m_gid, 111);
    dispatch(npc.m_gid, 111);
    Require(npc.m_job == 111 && npc.m_xSize == 17 && npc.m_ySize == 23,
        "GR2-to-hidden or repeated hidden class change changed the hotspot dimensions");
    dispatch(npc.m_gid, 101);
    Require(npc.m_job == 101 && npc.m_billboardTexture == nullptr && ownedTextureDestructions == 1,
        "hidden-to-sprite reused a stale billboard or released the texture twice");
}
