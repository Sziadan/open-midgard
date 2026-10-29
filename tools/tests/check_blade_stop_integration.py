#!/usr/bin/env python3
"""Bind Blade Stop packet/runtime fixture to the Ref and canonical PE routes."""
import re
import sys
from pathlib import Path

local_packet_path, ref_packet_path, local_actor_path, ref_actor_path, pe_path, fixture_path = map(Path, sys.argv[1:7])
local_packet = local_packet_path.read_text(encoding="utf-8", errors="replace")
ref_packet = ref_packet_path.read_text(encoding="utf-8", errors="replace")
local_actor = local_actor_path.read_text(encoding="utf-8", errors="replace")
ref_actor = ref_actor_path.read_text(encoding="utf-8", errors="replace")
pe = pe_path.read_text(encoding="utf-8", errors="replace")
fixture = fixture_path.read_text(encoding="utf-8", errors="replace")


def definition(text: str, marker: str) -> str:
    pos = 0
    while True:
        start = text.find(marker, pos)
        if start < 0:
            raise AssertionError(f"definition missing: {marker}")
        brace = text.find("{", start)
        semicolon = text.find(";", start)
        if brace >= 0 and (semicolon < 0 or brace < semicolon):
            depth = 0
            for end in range(brace, len(text)):
                if text[end] == "{":
                    depth += 1
                elif text[end] == "}":
                    depth -= 1
                    if depth == 0:
                        return text[start:end + 1]
            raise AssertionError(f"unclosed definition: {marker}")
        pos = start + len(marker)


def flat(text: str) -> str:
    return re.sub(r"\s+", "", text)


def require_order(source: str, tokens: tuple[str, ...], label: str) -> None:
    normalized = flat(source)
    cursor = 0
    for token in tokens:
        position = normalized.find(flat(token), cursor)
        if position < 0:
            raise AssertionError(f"{label} operation order missing/out of order: {token}")
        cursor = position + len(flat(token))


local_handler = definition(local_packet, "void HandleBladeStop(CGameMode& mode, const PacketView& packet)")
ref_handler = definition(ref_packet, "void __thiscall CGameMode::Zc_BladeStop(")
pe_handler = definition(pe, "void __thiscall CGameMode::Zc_BladeStop(")
for token in (
    "packet.packetLength < 14", "ResolveSkillPacketActor(mode, aid)",
    "source->SetForceAnimation(96, 4, 4, 96, 4, 1, 0x5F5E0FFu, 0)",
    "source->m_isSprArgbFixed = 1", "source->SetArgb(-1, 64, 64, 64)",
    "target->m_isMotionFreezed = 1", "target->m_isSprArgbFixed = 1",
    "source->SetForceAnimation(96, 4, 4, 96, 4, 1, 1u, 0)",
    "target->m_isMotionFreezed = 0", "source->SetArgb(-1, 255, 255, 255)",
):
    if token not in local_handler:
        raise AssertionError(f"local Blade Stop handler missing {token}")
for label, handler in (("Ref", ref_handler), ("PE", pe_handler)):
    normalized = flat(handler)
    for token in map(flat, (
        "0x5F5E0FFu", "m_isSprArgbFixed = 1", "m_isMotionFreezed = 1",
        "1u, 0", "m_isMotionFreezed = 0", "255, 255, 255",
    )):
        if token not in normalized:
            raise AssertionError(f"{label} Blade Stop handler missing {token}")
if "router.Register(0x01D1, HandleBladeStop)" not in local_packet:
    raise AssertionError("local packet router does not register Blade Stop 0x01D1")
require_order(local_handler, (
    "source->SetForceAnimation(96, 4, 4, 96, 4, 1, 0x5F5E0FFu, 0)",
    "source->m_isSprArgbFixed = 1", "source->SetArgb(-1, 64, 64, 64)",
    "target->m_isMotionFreezed = 1", "target->m_isSprArgbFixed = 1",
    "target->SetArgb(-1, 64, 64, 64)"), "local start")
require_order(local_handler, (
    "source->SetForceAnimation(96, 4, 4, 96, 4, 1, 1u, 0)",
    "source->m_isSprArgbFixed = 0", "source->SetArgb(-1, 255, 255, 255)",
    "target->m_isMotionFreezed = 0", "target->m_isSprArgbFixed = 0",
    "target->SetArgb(-1, 255, 255, 255)"), "local release")
for label, handler in (("Ref", ref_handler), ("PE", pe_handler)):
    require_order(handler, (
        "SetForceAnimation(m_player, 96, 4, 4, 96, 4, 1, 0x5F5E0FFu, 0)",
        "m_player->m_isSprArgbFixed = 1", "SetArgb(m_player, -1, 64, 64, 64)",
        "GameActorByAID->m_isMotionFreezed = 1", "GameActorByAID->m_isSprArgbFixed = 1",
        "SetArgb(GameActorByAID, -1, 64, 64, 64)"), f"{label} start")
    require_order(handler, (
        "SetForceAnimation(v6, 96, 4, 4, 96, 4, 1, 1u, 0)",
        "v6->m_isSprArgbFixed = 0", "SetArgb(v6, -1, 255, 255, 255)",
        "v7->m_isMotionFreezed = 0", "v7->m_isSprArgbFixed = 0",
        "SetArgb(v7, -1, 255, 255, 255)"), f"{label} release")

local_force = definition(local_actor, "void CRenderObject::SetForceAnimation(")
ref_force = definition(ref_actor, "void __thiscall CRenderObject::SetForceAnimation(")
pe_force = definition(pe, "void __thiscall CRenderObject::SetForceAnimation(")
for label, force in (("Ref", ref_force), ("PE", pe_force)):
    normalized = flat(force)
    for token in ("m_forceStateEndTick", "endTick", "timeGetTime()", "m_isForceState = 1"):
        if flat(token) not in normalized:
            raise AssertionError(f"{label} force-animation source missing {token}")
for token in (
    "m_forceAct = static_cast<u8>(act)", "m_forceMot = static_cast<u8>(mot)",
    "m_forceStateEndTick = isAnim ? endTick + timeGetTime() : 0",
):
    if token not in local_force:
        raise AssertionError(f"local forced animation setup missing {token}")
if "0x5630d0" not in ref_actor.lower() or "005630D0" not in pe:
    raise AssertionError("Ref/PE SetForceAnimation VA 0x5630D0 provenance is missing")

process_motion = definition(local_actor, "void CRenderObject::ProcessForcedMotion()")
for token in (
    "m_curAction = m_forceAct + Get8Dir(m_roty)", "m_curMotion = m_forceMot",
    "timeGetTime() >= m_forceStateEndTick", "m_isForceState = 0",
    "m_forceStateEndTick = 0", "m_motionType = 1", "m_isMotionFinished = 1",
):
    if token not in process_motion:
        raise AssertionError(f"local forced-motion body missing {token}")
if "0x555500" not in ref_actor.lower() or "00555500" not in pe:
    raise AssertionError("Ref/PE ProcessMotion provenance anchor 0x555500 is missing")

for token in (
    "0x5F5E0FFu + 1000u", "m_curAction == 98", "m_curMotion == 4",
    "target.m_isMotionFreezed == 1", "now = 2000", "m_forceStateEndTick == 2001",
    "now = 2001", "m_isForceState == 0", "target.m_isMotionFreezed == 0",
):
    if flat(token) not in flat(fixture):
        raise AssertionError(f"Blade Stop lifecycle fixture missing assertion token: {token}")

print("Blade Stop packet-to-forced-pose provenance passed; render/visual acceptance excluded.")
