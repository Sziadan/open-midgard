#!/usr/bin/env python3
"""Compare packet anchors for recovery/resurrection and document local differences."""

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/GameModePacket.cpp").read_text(encoding="latin-1")
LOCAL = (ROOT / "src/gamemode/GameModePacket.cpp").read_text(encoding="utf-8")


def body(source, signature):
    start = source.rindex(signature)
    brace = source.index("{", start)
    depth = 0
    for pos in range(brace, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[brace + 1:pos]
    raise AssertionError(f"unterminated function: {signature}")


def compact(text):
    return re.sub(r"\s+", "", text)


recovery_sig = "void __thiscall CGameMode::Zc_recovery(CGameMode *this, const char *buf)"
resurrection_sig = "void __thiscall CGameMode::Zc_resurrection(CGameMode *this, const char *buf)"
for signature in (recovery_sig, resurrection_sig):
    pe_body = body(PE, signature)
    ref_body = body(REF, signature)
    assert compact(pe_body) == compact(ref_body), f"PE/Ref mismatch: {signature}"

ref_recovery = body(REF, recovery_sig)
for token in (
    "*((_WORD *)buf + 1) == 5", "*((_WORD *)buf + 1) == 7",
    "-16711936, 114", "-16776961, 114", "LaunchEffect(m_player, 331",
    "LaunchEffect(m_player, 332",
):
    assert token in ref_recovery, f"canonical Recovery branch missing {token}"

local_recovery = body(LOCAL, "void HandleRecovery(CGameMode& mode, const PacketView& packet)")
for token in (
    "const int effectId=ResolveRecoveryEffectId(statusType)", "if(effectId<0)return;",
    "rawAmount<=0x7fff ? int(rawAmount) : int(rawAmount)-0x10000",
    "g_session.GetCharacterVitals()", "previousValue+static_cast<u32>(amount)",
    "ApplySelfStatusUpdate(mode,statusType,updatedValue)", "InvalidateBasicInfoWindow()",
    "AdvanceEffectStatusRandomState()", "EmitRecoveryNumber(actor,actor,amount,ResolveRecoveryNumberColor(statusType))",
):
    assert token in local_recovery, f"selected Recovery behavior missing {token}"
assert "amount == 0" not in local_recovery
assert "ResolveRecoveryNumberColor(u32 statusType)" in LOCAL
assert "return statusType == kStatusSp ? kRecoverySpNumberColor : kRecoveryHpNumberColor;" in LOCAL

ref_resurrection = body(REF, resurrection_sig)
for token in (
    "CWorld::GetGameActorByAID(this->m_world, *(_DWORD *)(buf + 2))",
    "SendMsg(GameActorByAID, 0, 98, 0, 0, 0)",
    "*(_DWORD *)(buf + 2) == g_session.m_aid",
    "m_isOnQuest = 0", "m_isPlayerDead = 0",
):
    assert token in ref_resurrection, f"canonical Resurrection behavior missing {token}"

local_resurrection = body(LOCAL, "void HandleActorResurrection(CGameMode& mode, const PacketView& packet)")
for token in (
    "FindRuntimeActorForVanish(mode, gid)", "actor->SendMsg(actor, 98, 0, 0, 0)",
    "IsLocalPlayerActor(mode, gid)", "mode.m_isOnQuest = 0", "mode.m_isPlayerDead = 0",
    "mode.m_waitingUseItemAck = 0",
):
    assert token in local_resurrection, f"local Resurrection behavior missing {token}"

assert "packet.packetLength < 6" in local_recovery

# Local resurrection intentionally adds a packet-length/zero-GID guard and clears
# the pending item-ack flag; it also resolves local actors through its runtime map
# and fallback instead of issuing Ref's separate unconditional local-player send.
local_actor_resolver = body(LOCAL, "CGameActor* FindRuntimeActorForVanish(CGameMode& mode, u32 gid)")
assert "mode.m_runtimeActors.find(gid)" in local_actor_resolver
assert "IsLocalPlayerActor(mode, gid) && mode.m_world && mode.m_world->m_player" in local_actor_resolver
assert "packet.packetLength < 8" in local_resurrection and "if (gid == 0)" in local_resurrection

print("Recovery and Resurrection PE/Ref method bodies match each other.")
print("Local packet tests cover HP/SP recovery and runtime actor resolution.")
print("Recovery consumes selected signed16 deltas, session-owned32-bit addition, zero feedback and recognized statuses only; actor absence remains defensive.")
print("Resurrection adds defensive guards and clears the pending item-ack flag; its runtime resolver can coalesce the source's separate local-player send when the local actor is found.")
