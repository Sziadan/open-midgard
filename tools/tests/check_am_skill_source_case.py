#!/usr/bin/env python3
"""Compare Am_Skill's source/EXE branches and current skill WBA routing."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
REF = (ROOT / "Ref/GameActorMsgHandler.cpp").read_text(encoding="utf-8", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
IMPL = (ROOT / "src/world/GameActorMsgHandler.cpp").read_text(encoding="utf-8")


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


def labels(function: str, expression: str) -> list[int]:
    values = []
    for switch in re.finditer(rf"switch\s*\(\s*{re.escape(expression)}\s*\)\s*\{{", function):
        depth = 1
        for line in re.finditer(r"[^\n]*(?:\n|$)", function[switch.end():]):
            value = line.group(0)
            if depth == 1:
                match = re.match(r"\s*case\s+(?:0x([\da-fA-F]+)|(\d+))[uUlL]*\s*:", value)
                if match:
                    item = int(match.group(1), 16) if match.group(1) else int(match.group(2))
                    if item not in values: values.append(item)
            depth += value.count("{") - value.count("}")
            if depth == 0: break
    return values


ref_marker = "void __thiscall CGameActor::Am_Skill(CGameActor *this, CGameObject *sender, int *arg1, int arg2, int arg3)\n{"
ref_skill, exe_skill = body(REF, ref_marker), body(EXE, ref_marker)
assert norm(ref_skill) == norm(exe_skill), "complete Ref/EXE CGameActor::Am_Skill bodies differ"
current = body(IMPL, "void CGameActor::Am_Skill(const int* arg1)\n{")
src, cur = norm(ref_skill), norm(current)

# Packet payload, target priority, actor-only bird update and mirrored fallback.
for index in (0, 1, 2, 3, 5, 6, 7, 8, 9, 10):
    assert f"arg1[{index}]" in current, f"current skill payload omits arg1[{index}]"
assert "g_session.m_aid == v13" in src and "CWorld::GetGameActorByAID" in src and "CWorld::GetSkillByAID" in src
assert cur.index("g_session.m_aid == targetGid") < cur.index("world->GetGameActorByAID(targetGid)") < cur.index("world->GetSkillByAID(targetGid)")
assert "if ((skillId == 129 || skillId == 381) && m_birdEffect)" in current
bird_at = cur.index("m_birdEffect->SendMsg(nullptr, 29")
assert cur.index("world->GetGameActorByAID(targetGid)") < bird_at < cur.index("world->GetSkillByAID(targetGid)")
assert "targetPos.x = m_pos.x - (playerPos.x - m_pos.x)" in current
assert "targetPos.y = m_pos.y - (playerPos.y - m_pos.y)" in current
assert "targetPos.z = m_pos.z - (playerPos.z - m_pos.z)" in current

# Count normalization, skill 56 level, early action 5 path, and state/motion flow.
assert "v9 <= 0 || v9 > 36" in src and "count <= 0 || count > 36" in current
assert "v10 == 56" in src and "skillId == 56" in current
assert {4, 5, 6, 8, 9}.issubset(set(labels(ref_skill, "action")))
assert {4, 5, 6, 8, 9}.issubset(set(labels(current, "action")))
assert 10 in labels(ref_skill, "action") and 10 in labels(current, "action")
assert "action < 8 || action > 9" in current
assert "action == 5 && target && skillId != 47 && skillId != 11 && skillId != 413 && skillId != 273" in current
for field in ("hit.time = timeGetTime() +", "hit.targetGid = m_gid", "hit.damage = damage", "hit.effectId1 = -1", "hit.effectId2 = -1", "hit.tCellX = cellX", "hit.tCellY = cellY", "hit.effectLevel = level", "hit.message = 96"):
    assert field in current, f"action-5 WBA field missing {field}"
assert "target->SendMsg(this, 27, reinterpret_cast<msgparam_t>(&hit), 0, 0)" in current
assert "static_cast<double>(attackedMT) * 0.0034722222" in current
for field in ("hit.damage = damage", "hit.effectId2 = effectId2", "hit.message = message", "hit.tCellX = cellX", "hit.effectId1 = effectId1", "hit.effectLevel = level", "hit.time = delay + timeGetTime()", "hit.targetGid = m_gid", "hit.tCellY = cellY"):
    assert field in current, f"normal WBA payload missing {field}"
assert "GetSkillActionInfo(skillId, beginEffectId, skillMotionType)" in current
assert "skillId != 143 && skillId != 263 && skillId != 272 && skillId != 273 && skillId != 271 && skillId != 371" in current
assert "g_session.LaunchSkillSpecialEffect(skillId, count, this, target)" in current

# Attack-effect creation and effectId1 dispatch table, including source's zero default.
assert "effectId1a = CSession::GetAttackEffectIdBySkillId" in src
assert "effectId2 = CSession::GetAttackedEffectIdBySkillId" in src
assert "effectId1c = 0" in src
resolve_helper = norm(body(IMPL, "int ResolveSkillAttackEffect(int skillId, int damage, int& effectId2)\n{"))
assert "int effectId = 0" in resolve_helper
for needle in ("skillId >= 411 && skillId <= 472", "skillId >= 493", "skillId == 129 || skillId == 347 || skillId == 397 || skillId == 654", "skillId >= 56 && skillId <= 59", "skillId >= 184 && skillId <= 189"):
    assert needle in resolve_helper, f"effectId1 exclusion/default table missing {needle}"
for needle in ("case 7:", "case 17:", "case 19:", "return 49", "return 51", "return 52", "return 152", "return 147", "return 180", "return 549", "case 136:", "return 0"):
    assert needle in resolve_helper, f"effectId1 mapping missing {needle}"
assert "if (damage != 0)" in resolve_helper and "effectId2 = -1" in resolve_helper
assert {2, 3, 4, 5}.issubset(set(labels(ref_skill, "level")))
assert {2, 3, 4, 5}.issubset(set(labels(current, "level")))
assert "if (effectId2 == 186)" in current
launch_counted = body(IMPL, "CRagEffect* LaunchCounted(CAbleToMakeEffect& owner, int effectId, int count)\n{")
assert "if (effectId < 0)" not in launch_counted
assert launch_counted.index("owner.LaunchEffect(effectId") < launch_counted.index("if (effect)") < launch_counted.index("effect->SendMsg(effect, 44, count")

# Attack action routing and the exact message-131 cases.
for skill_id in (5, 62, 413, 419, 421):
    assert f"skillId == {skill_id}" in current, f"skill {skill_id} message-131 route missing"
assert "skillId == 5 || skillId == 62 || skillId == 413 || skillId == 419 || skillId == 421" in current
assert "skillId == 415 || skillId == 417" in current and "hit.time += 300" in current
assert "skillId == 271 || skillId == 379" in current
assert "skillId == 273" in current and "target->LaunchEffect(704" in current
assert "count == 1 && skillId != 371 && skillId != 199" in current
assert "skillId == 77" in current and "hit.time += 500" in current
assert "doubleAttackTerm = 200" in EXE and "kDoubleAttackTermMs = 200" in IMPL

# Effect post-state branches and per-extra-hit routes, including conditional RNG consumption.
post_ids = {263, 271, 266, 267, 272, 273, 370, 371, 372, 413}
assert post_ids.issubset(set(labels(ref_skill, "skillId")))
assert post_ids.issubset(set(labels(current, "skillId")))
assert "switch (skillId)" in current and "hit.effectId2 = -1" in current
assert "if (skillId == 56)" in current and "hit.time += 80" in current
assert "if (skillId == 136)" in current and "hit.effectId2 = 122" in current
assert "if (hit.damage / 10)" in current and "std::rand();" in current
assert "if (extra == 2)" in current and "hit.effectId1 = 1" in current
current_rand = cur.index("std::rand();")
assert cur.index("SendMsg(this, 27", cur.index("if (skillId == 136)")) < current_rand < cur.index("hit.message = savedMessage", current_rand)
assert "skillId == 272" in current and "skillId == 372 || skillId == 394" in current
assert "skillId == 413 || skillId == 415 || skillId == 417 || skillId == 419 || skillId == 421" in current

print("Am_Skill: complete Ref/EXE bodies match; target, action, effect table, WBA routes, post-effects and ordered skill-136 RNG verified")
