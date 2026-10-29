#!/usr/bin/env python3
"""Compare combo-delay packet and skill272 animation timing routes to Ref/PE."""
import re
import sys
from pathlib import Path

local_packet_path, ref_packet_path, local_actor_path, ref_actor_path, local_render_path, ref_render_path, pe_path = map(Path, sys.argv[1:8])
local_packet = local_packet_path.read_text(encoding="utf-8", errors="replace")
ref_packet = ref_packet_path.read_text(encoding="utf-8", errors="replace")
local_actor = local_actor_path.read_text(encoding="utf-8", errors="replace")
ref_actor = ref_actor_path.read_text(encoding="utf-8", errors="replace")
local_render = local_render_path.read_text(encoding="utf-8", errors="replace")
ref_render = ref_render_path.read_text(encoding="utf-8", errors="replace")
pe = pe_path.read_text(encoding="utf-8", errors="replace")


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


def branch(switch_body: str, label: str, next_label: str) -> str:
    start = switch_body.find(label)
    if start < 0:
        raise AssertionError(f"switch branch missing: {label}")
    end = switch_body.find(next_label, start + len(label))
    if end < 0:
        raise AssertionError(f"switch branch terminator missing: {next_label}")
    return switch_body[start:end]


def flat(text: str) -> str:
    return re.sub(r"\s+", "", text)


local_handle = definition(local_packet, "void HandleComboDelay(CGameMode& mode, const PacketView& packet)")
for token in (
    "packet.packetLength < 10", "ReadLE32(packet.data + 2)", "ReadLE32(packet.data + 6)",
    "g_session.SetForceAnimEndTick(aid, tick)", "actor->SetForceAnimEndTick(tick)",
):
    if token not in local_handle:
        raise AssertionError(f"local 0x01D2 handler missing {token}")
if "router.Register(0x01D2, HandleComboDelay)" not in local_packet:
    raise AssertionError("local packet router does not register combo delay 0x01D2")

ref_combo_packet = definition(ref_packet, "void __thiscall CGameMode::Zc_ComboDelay(")
pe_combo_packet = definition(pe, "void __thiscall CGameMode::Zc_ComboDelay(")
for label, source in (("Ref", ref_combo_packet), ("PE", pe_combo_packet)):
    normalized = flat(source)
    for token in map(flat, (
        "CSession::SetForceAnimEndTick(&g_session", "buf + 2", "buf + 6",
        "CRenderObject::SetForceAnimEndTick", "CWorld::GetGameActorByAID",
    )):
        if token not in normalized:
            raise AssertionError(f"{label} combo packet route missing {token}")

local_skill_method = definition(local_actor, "void CGameActor::Am_Skill(const int* arg1)")
ref_skill_method = definition(ref_actor, "void __thiscall CGameActor::Am_Skill(")
pe_skill_method = definition(pe, "void __thiscall CGameActor::Am_Skill(")
local_272 = branch(local_skill_method, "case 272:", "case 273:")
ref_272 = branch(ref_skill_method, "case 272:", "case 273:")
pe_272 = branch(pe_skill_method, "case 272:", "case 273:")
for label, source in (("local", local_272), ("Ref", ref_272), ("PE", pe_272)):
    normalized = flat(source)
    checks = (
        "SetForceAnimation(this,96,5,5,96,5,1", "GetForceAnimEndTick",
        "LaunchEffect", "262", "263", "273",
    ) if label != "local" else (
        "SetForceAnimation(96,5,5,96,5,1,g_session.GetForceAnimEndTick(m_gid),0)",
        "target->LaunchEffect(262", "LaunchEffect(263", "target->LaunchEffect(273",
    )
    for token in map(flat, checks):
        if token not in normalized:
            raise AssertionError(f"{label} skill272 branch missing {token}")
    positions = [normalized.find(flat(("262", "263", "273")[i])) for i in range(3)]
    if not positions[0] < positions[1] < positions[2]:
        raise AssertionError(f"{label} skill272 effect order is not 262,263,273")

local_force = definition(local_render, "void CRenderObject::SetForceAnimation(")
ref_force = definition(ref_render, "void __thiscall CRenderObject::SetForceAnimation(")
pe_force = definition(pe, "void __thiscall CRenderObject::SetForceAnimation(")
if "m_forceStateEndTick=isAnim?endTick+timeGetTime():0" not in flat(local_force):
    raise AssertionError("local force animation does not add the sampled current tick to duration")
for label, source in (("Ref", ref_force), ("PE", pe_force)):
    normalized = flat(source)
    if "m_forceStateEndTick=endTick+Time" not in normalized or "Time=timeGetTime()" not in normalized:
        raise AssertionError(f"{label} force animation lacks source endTick + current clock behavior")

if "0x5630d0" not in ref_render.lower():
    raise AssertionError("Ref SetForceAnimation VA 0x5630D0 is missing")
if "005630D0" not in pe:
    raise AssertionError("SetForceAnimation PE/Ref provenance VA 0x5630D0 is missing")

print("Combo delay integration comparator passed: 0x01D2 packet storage, skill272 timing/effect order, and PE/Ref clock provenance.")
