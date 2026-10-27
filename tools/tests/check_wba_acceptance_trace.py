#!/usr/bin/env python3
"""Check diagnostic metadata/logging used by the combat WBA acceptance walkthrough."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
HEADER = (ROOT / "src/world/GameActor.h").read_text(encoding="utf-8")
HANDLER = (ROOT / "src/world/GameActorMsgHandler.cpp").read_text(encoding="utf-8")
ACTOR = (ROOT / "src/world/GameActor.cpp").read_text(encoding="utf-8")


def body(text: str, marker: str) -> str:
    start = text.rfind(marker)
    assert start >= 0, f"function not found: {marker}"
    opening = text.find("{", start)
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[opening + 1:pos]
    raise AssertionError(f"unclosed function: {marker}")


wba = re.search(r"struct WBA\s*\{(?P<body>[^}]+)\}", HEADER, re.S)
assert wba, "WBA definition missing"
prefix = wba.group("body")
fields = ["message", "damage", "time", "targetGid", "tCellX", "tCellY", "effectId1", "effectId2", "attackedSpeedFactor", "effectLevel", "sourceSkillId"]
positions = [prefix.find(field) for field in fields]
assert all(position >= 0 for position in positions) and positions == sorted(positions), \
    "WBA source-compatible field order or appended skill metadata changed"
assert "sourceSkillId = -1" in prefix, "normal action diagnostics should clearly report unknown skill"
print("PASS WBA source-compatible prefix preserved; diagnostic skill ID is appended and initialized")

am_skill = body(HANDLER, "void CGameActor::Am_Skill(const int* arg1)")
assert am_skill.count("hit.sourceSkillId = skillId;") == 2, \
    "both the action-5 and ordinary Am_Skill WBA routes must carry skill identity"
print("PASS skill identity propagates through both Am_Skill WBA construction paths")

handle = body(ACTOR, "void CGameActor::HandleAttacked(const WBA& hit)")
assert re.search(r"const char\* hitWave\s*=\s*nullptr", handle)
assert re.search(r"hitWave\s*=\s*g_session\.GetWeaponHitWaveName", handle)
assert re.search(r"hitWave\s*=\s*g_session\.GetJobHitWaveName", handle)
assert re.search(r"if\s*\(hitWave\s*&&\s*g_world\.m_player\)[\s\S]*?PlayWave\(hitWave", handle)
assert re.search(r"DbgLog\(\"\[WBA\] msg=%d skill=%d effect=%d/%d wave=%s delivery=%u", handle)
for field in ("hit.message", "hit.sourceSkillId", "hit.effectId1", "hit.effectId2", "hitWave ? hitWave : \"(none)\"", "hit.time"):
    assert field in handle, f"WBA acceptance log omits {field}"
print("PASS delivery log includes message, skill, effects, resolved wave, and scheduled tick")

enqueue = body(ACTOR, "void CGameActor::EnqueueWillBeAttacked(WBA* hit, int runningDamage, int forcedMessage)")
assert "m_willBeAttackedList.push_back(*hit)" in enqueue, "queued WBA must preserve diagnostic skill metadata"
process = body(ACTOR, "void CGameActor::ProcessWillBeAttacked()")
assert re.search(r"now\s*<\s*it->time[\s\S]*?SendMsg\(nullptr,\s*it->message[\s\S]*?it\s*=\s*m_willBeAttackedList\.erase\(it\)", process), \
    "WBA delivery must retain the source tick gate before dispatch"
print("PASS queued WBA metadata reaches delivery after the scheduled-time gate")
