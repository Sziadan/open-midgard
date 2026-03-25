#pragma once
//===========================================================================
// GameModePacket.h  –  Packet parsing and dispatch helpers for game mode
//===========================================================================
#include "Types.h"

#include <functional>
#include <unordered_map>

class CGameMode;

enum class PendingDisconnectAction {
    None = 0,
    ReturnToCharSelect = 1,
};

struct PacketView
{
    u16       packetId = 0;
    u16       packetLength = 0;
    const u8* data = nullptr;
};

// Decode one packet from a raw byte stream.
// Returns true when a complete packet was decoded.
bool TryReadPacket(const u8* stream, int availableBytes, PacketView& outPacket, int& consumedBytes);

// Utility used by movement packets where src/dst is bit-packed.
void DecodeSrcDst(const u8* src, int& srcX, int& srcY, int& dstX, int& dstY, int& srcDir, int& dstDir);

class CGameModePacketRouter
{
public:
    using Handler = std::function<void(CGameMode&, const PacketView&)>;

    void Register(u16 packetId, Handler handler);
    bool Dispatch(CGameMode& gameMode, const PacketView& packet) const;
    void Clear();

private:
    std::unordered_map<u16, Handler> m_handlers;
};

// Registers first-pass handlers (login/map/spawn/move skeleton packets).
void RegisterDefaultGameModePacketHandlers(CGameModePacketRouter& router);

// Removes runtime actors whose delayed despawn timer has expired.
void CleanupPendingActorDespawns(CGameMode& mode);

void SetPendingDisconnectAction(PendingDisconnectAction action);
PendingDisconnectAction GetPendingDisconnectAction();
void ClearPendingDisconnectAction();
