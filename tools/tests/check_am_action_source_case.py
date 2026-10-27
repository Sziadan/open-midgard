#!/usr/bin/env python3
"""Compare Am_Action's source branches and hit scheduling to the canonical EXE."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
REF = (ROOT / "Ref/GameActorMsgHandler.cpp").read_text(encoding="utf-8", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
IMPL = (ROOT / "src/world/GameActorMsgHandler.cpp").read_text(encoding="utf-8")
WORLD_H = (ROOT / "src/world/World.h").read_text(encoding="utf-8")


def body(text: str, marker: str) -> str:
    start = text.index(marker)
    opening = text.index("{", start)
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{": depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0: return text[opening + 1:pos]
    raise AssertionError(f"unclosed function: {marker}")


def norm(text: str) -> str:
    return re.sub(r"\s+", " ", text)


def switch_labels(function: str, expression: str) -> list[int]:
    labels = []
    for switch in re.finditer(rf"switch\s*\(\s*{re.escape(expression)}\s*\)\s*\{{", function):
        depth = 1
        for line in re.finditer(r"[^\n]*(?:\n|$)", function[switch.end():]):
            text = line.group(0)
            if depth == 1:
                item = re.match(r"\s*case\s+(?:0x([\da-fA-F]+)|(\d+))[uUlL]*\s*:", text)
                if item:
                    value = int(item.group(1), 16) if item.group(1) else int(item.group(2))
                    if value not in labels: labels.append(value)
            depth += text.count("{") - text.count("}")
            if depth == 0: break
    return labels


def ordered_subsequence(values: list[int], expected: list[int], label: str) -> None:
    for value in expected:
        assert value in values, f"{label}: missing action case {value}; found {values}"


ref_marker = "void __thiscall CGameActor::Am_Action(CGameActor *this, CGameObject *sender, int *arg1, int arg2, int arg3)\n{"
exe_marker = ref_marker
ref_action, exe_action = body(REF, ref_marker), body(EXE, exe_marker)
assert norm(ref_action) == norm(exe_action), "complete Ref/EXE CGameActor::Am_Action bodies differ"
source_labels = switch_labels(ref_action, "actorMsg")
assert source_labels == [0, 4, 8, 9, 10, 11, 1, 2, 3], f"source action case order changed: {source_labels}"

current_action = body(IMPL, "void CGameActor::Am_Action(const int* arg1)\n{")
current_labels = switch_labels(current_action, "action")
assert current_labels == source_labels, f"current action labels/order differ: {current_labels}"
for arg_index, name in ((0, "totalDamage"), (1, "leftDamage"), (2, "action"), (3, "targetGid"), (5, "count"), (6, "cellX"), (7, "cellY"), (8, "attackMT"), (9, "attackedMT")):
    assert f"arg1[{arg_index}]" in current_action, f"current payload field {name} moved from arg1[{arg_index}]"

# Target resolution priority and the player-reflection fallback used to face unknown targets.
source_lookup = norm(ref_action)
current_lookup = norm(current_action)
for needle in ("g_session.m_aid == v8", "CWorld::GetGameActorByAID", "CWorld::GetSkillByAID"):
    assert needle in source_lookup, f"source target resolution missing {needle}"
for needle in ("g_session.m_aid == targetGid", "world->GetGameActorByAID(targetGid)", "world->GetSkillByAID(targetGid)"):
    assert needle in current_lookup, f"current target resolution missing {needle}"
assert current_lookup.index("world->GetGameActorByAID(targetGid)") < current_lookup.index("world->GetSkillByAID(targetGid)")
assert "targetPos.x = m_pos.x - (playerPos.x - m_pos.x)" in current_action
assert "targetPos.z = m_pos.z - (playerPos.z - m_pos.z)" in current_action

# Count, damage, counter, speed, and facing branches.
for needle in ("count <= 0 || count > 36", "actorMsg != 8 && actorMsg != 9", "atkEfId / count", "m_isCounter", "LaunchEffect(this, 131"):
    assert needle in source_lookup, f"source preamble/action flow missing {needle}"
assert "count <= 0 || count > 36" in current_action and "action != 8 && action != 9" in current_action
assert "totalDamage / count" in current_action
assert "counter->SendMsg(this, 50, 0, 0, 0)" in current_action, "counter effect must attach to the actor sender"
assert "SetModifyFactorOfmotionSpeed(attackMT)" in current_action
assert "std::atan2(static_cast<double>(targetPos.x) - m_pos.x" in current_action
assert "/ 3.141592 * 180.0" in current_action

# Source-to-current attack message map after counter reset.
for message, source_text, current_text in (
    (39, "case 4: case 9:", "case 4: case 9:"),
    (93, "case 10: actorMsg = 93;", "case 10: message = 93;"),
    (94, "actorMsg = 94;", "case 11: message = m_isCounter ? 26 : 94;"),
):
    assert source_text in source_lookup, f"source mapping for message {message} missing"
    assert current_text in current_lookup, f"current mapping for message {message} missing"
assert "effectId1 = 1" in current_action and "effectId1 = atkEfId" in source_lookup
assert "int message = 26" in current_action
assert "message = 39" in current_action and "message = 93" in current_action
assert "message = m_isCounter ? 26 : 94" in current_action
assert "effectId2 = (m_job >= 4046 && m_job <= 4048) ? 434 : 0" in current_action
assert "int effectId2 = -2" in current_action
assert "m_job == 1016 || m_job == 1420" in current_action and "delay += 192" in current_action
assert "m_job == 1285 || m_job == 1830" in current_action and "delay += 912" in current_action
assert "m_job == 1286 || m_job == 1829 || m_job == 1287" in current_action and "delay += 408" in current_action

# WBA field map and timing/data delivery. These checks deliberately compare semantic fields,
# because the current C++ struct uses named members where the source uses decompiler locals.
field_map = {
    "time": "hit.time = delay + timeGetTime()",
    "effectId1": "hit.effectId1 = effectId1",
    "targetGid": "hit.targetGid = m_gid",
    "tCellY": "hit.tCellY = cellY",
    "message": "hit.message = message",
    "tCellX": "hit.tCellX = cellX",
    "effectId2": "hit.effectId2 = effectId2",
    "effectLevel": "hit.effectLevel = 1",
    "damage": "hit.damage = perHit",
    "attackedSpeedFactor": "static_cast<double>(attackedMT) * 0.0034722222",
}
for field, needle in field_map.items():
    assert needle in current_action, f"current WBA payload missing {field}: {needle}"
for source_field in ("wba.time = delayTime + timeGetTime()", "wba.effectId1 = atkEfId", "wba.targetGid = attackedMT", "wba.tCellY = tCellY", "wba.message = actorMsg", "wba.tCellX = tCellX", "wba.effectId2 = v25", "wba.effectLevel = 1", "wba.damage = v19", "wba.attackedSpeedFactor = attackedSpeedFactor"):
    assert source_field in source_lookup, f"source WBA payload missing {source_field}"
assert "target->SendMsg(this, 27" in current_action
assert "doubleAttackTerm = 200" in EXE and "kDoubleAttackTermMs = 200" in IMPL
assert "kDoubleAttackTermMs / 2" in current_action
assert "kDoubleAttackTermMs / 4 + kDoubleAttackTermMs + kDoubleAttackTermMs / 2" in current_action
assert "hit.time += static_cast<u32>(kDoubleAttackTermMs)" in current_action
assert "leftDamage && (m_job < 1000 || m_job > 4000)" in current_action
assert "leftDamage && (CGameActor::GetJob(this) < 1000 || CGameActor::GetJob(this) > 4000)" in source_lookup
assert "target->SendMsg(this, 27, reinterpret_cast<msgparam_t>(&hit), running, 0)" in current_action
assert "hit.attackedSpeedFactor = static_cast<float>(static_cast<double>(attackedMT) * 0.0034722222)" in current_action
assert "GetSkillByAID(u32 aid)" in WORLD_H

print("Am_Action: complete Ref/EXE bodies and ordered action IDs match; target resolution, action mapping, counter attachment, WBA payload, and hit schedules verified")
