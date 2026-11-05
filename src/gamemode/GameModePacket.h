#pragma once
//===========================================================================
// GameModePacket.h  –  Packet parsing and dispatch helpers for game mode
//===========================================================================
#include "Types.h"

#include <functional>
#include <unordered_map>

class CGameMode;
class CRagConnection;

enum class PendingDisconnectAction {
    None = 0,
    ReturnToCharSelect = 1,
    ExitToWindows = 2,
};

enum class DisconnectAckDecision {
    Ignore = 0,
    Refused = 1,
    ExitToWindows = 2,
};

struct GameModePacketLoopResult {
    int packetsProcessed = 0;
    int packetsDispatched = 0;
    int packetsUnhandled = 0;
    bool receiveDrained = false;
    bool modeOperationRequested = false;
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
CGameModePacketRouter& GetProductionGameModePacketRouter();
GameModePacketLoopResult ProcessGameModePacketLoop(
    CGameMode& mode,
    CRagConnection& connection,
    CGameModePacketRouter& router,
    int packetBudget);
bool ApplyDisconnectTimeoutAfterPacketLoop(const GameModePacketLoopResult& packetLoop, u32 nowTick);

// Removes runtime actors whose delayed despawn timer has expired.
void CleanupPendingActorDespawns(CGameMode& mode);

// Applies locally known cart-on/cart-off state to the self actor when packet order
// leaves the bootstrap actor without option bits.
void ApplyPendingLocalCartState(CGameMode& mode);
void PrimeLocalCartStateFromEffectState(int effectState);
bool IsKnownLocalCartActive();

bool BeginPendingDisconnectAction(PendingDisconnectAction action);
void MarkPendingDisconnectActionSent(u32 startTick);
PendingDisconnectAction GetPendingDisconnectAction();
PendingDisconnectAction GetExpiredDisconnectAction(bool receiveDrained, u32 nowTick);
void ClearPendingDisconnectAction();
bool ApplyDisconnectTimeoutFallback(PendingDisconnectAction expiredAction);
DisconnectAckDecision DecideDisconnectAck(PendingDisconnectAction action, u16 result);
bool IsReturnToCharSelectAck(PendingDisconnectAction action, u8 result);
void ReturnToLoginModeWithStatus(const char* status);
