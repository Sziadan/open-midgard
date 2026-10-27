#!/usr/bin/env python3
"""Verify ProcessDamageSituation's sole runtime-wave producer is legacy HP_Battle UI."""
from pathlib import Path
import hashlib
import re

ROOT = Path(__file__).resolve().parents[2]
REF_GAME = (ROOT / "Ref/GameMode.cpp").read_text(encoding="utf-8", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
HP_BATTLE = (ROOT / "Ref/HP_Battle.cpp").read_text(encoding="utf-8", errors="replace")
SESSION_REF = (ROOT / "Ref/Session2.cpp").read_text(encoding="utf-8", errors="replace")
CURRENT_MODE = (ROOT / "src/gamemode/GameMode.cpp").read_text(encoding="utf-8")
CURRENT_PACKET = (ROOT / "src/gamemode/GameModePacket.cpp").read_text(encoding="utf-8")
CURRENT_ACTOR = (ROOT / "src/world/GameActor.cpp").read_text(encoding="utf-8")


def body(text: str, marker: str) -> tuple[str, str]:
    start = text.rfind(marker)
    assert start >= 0, f"definition missing: {marker}"
    opening = text.find("{", start)
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[start:opening].strip(), text[opening + 1:pos]
    raise AssertionError(f"unclosed function: {marker}")


normalize = lambda value: re.sub(r"\s+", " ", value).strip()
marker = "void __thiscall CGameMode::ProcessDamageSituation(CGameMode *this)"
ref_sig, ref = body(REF_GAME, marker)
exe_sig, exe = body(EXE, marker)
assert normalize(ref_sig) == normalize(exe_sig) == normalize(marker)
assert normalize(ref) == normalize(exe), "Ref and canonical EXE ProcessDamageSituation differ"
normalized = normalize(ref)
assert len(normalized) == 7493
assert hashlib.sha256(normalized.encode()).hexdigest() == "bc8eb0125f4e9b333be75635dcde6c49da2bbc63dd5e580aa58de4e873f54099"
assert ref.count("PlayWave(") == 1 and exe.count("PlayWave(") == 1

# Exact ordered sound branch: due queue entry -> resolved actor -> positive damage
# -> nonlethal branch -> actor state 4 -> non-PC and non-skill -> random normal wave.
sound_at = ref.index("PlayWave(")
prefix = ref[:sound_at]
required_order = [
    "Next->_Value.DamageTime > timeGetTime()",
    "CSession::GetGameActorByAid(&g_session, Next->_Value.DamageChar)",
    "DamageNum >= 0",
    "m_Hp - DamageNum <= 0",
    "CGameActor::SetState(GameActorByAid, 4)",
    "!CGameActor::IsThisPC(GameActorByAid, v14) && !g_used_Skill",
    "CSession::GetWeaponHitWaveName(&g_session, -1)",
]
positions = [prefix.index(item) for item in required_order]
assert positions == sorted(positions), "damage sound guards/effects changed order"
assert "PlayWave(WeaponHitWaveName, 0.0, 0.0, 0.0, 250, 40, 1.0)" in ref

# All queue producers belong to Ref's legacy HP battle UI module; Session2 only
# defines the queue append helper. The canonical EXE names each caller UI class.
assert HP_BATTLE.count("CSession::SetDamageSituation(") == 17
assert SESSION_REF.count("CSession::SetDamageSituation(") == 2
ref_callers = {}
for path in (ROOT / "Ref").rglob("*.cpp"):
    if "dhxj" in path.parts:
        continue
    count = path.read_text(encoding="utf-8", errors="replace").count("CSession::SetDamageSituation(")
    if count:
        ref_callers[path.relative_to(ROOT).as_posix()] = count
assert ref_callers == {"Ref/HP_Battle.cpp": 17, "Ref/Session2.cpp": 2}
assert EXE.count("CSession::SetDamageSituation(") == 19  # 17 calls + declaration + definition
exe_lines = EXE.splitlines()
caller_classes = set()
caller_count = 0
for index, line in enumerate(exe_lines):
    if "CSession::SetDamageSituation(" not in line or line.lstrip().startswith("void __thiscall"):
        continue
    caller_count += 1
    header = next(
        (exe_lines[pos].strip() for pos in range(index, -1, -1)
         if "__thiscall UIWindowMgr::" in exe_lines[pos]),
        "",
    )
    match = re.search(r"UIWindowMgr::([A-Za-z0-9_]+)", header)
    assert match, f"non-UI SetDamageSituation caller at EXE line {index + 1}"
    caller_classes.add(match.group(1))
assert caller_count == 17
assert caller_classes == {"Aco_Battle", "Boy_ActinField", "Help_Battle", "Mob_Battle"}

wave_rows = (ROOT / "docs/skill/generated/runtime_wave_callsites.tsv").read_text(encoding="utf-8").splitlines()
scope_row = next(row for row in wave_rows if row.startswith("0x57E27C\t"))
assert "scope-excluded (legacy HP_Battle UI" in scope_row
assert "0x57E27C" not in (ROOT / "docs/skill/BLOCKED.md").read_text(encoding="utf-8")

# Modern hit updates use server packets/WBA and have no legacy damage queue.
assert "ProcessDamageSituation" not in CURRENT_MODE
assert "g_DM_List" not in CURRENT_MODE + CURRENT_PACKET + CURRENT_ACTOR
assert "HandleSkillDamageNotify" in CURRENT_PACKET
assert "void CGameActor::HandleAttacked(const WBA& hit)" in CURRENT_ACTOR
print("PASS Ref/EXE ProcessDamageSituation equality (7,493 normalized body chars), sole ordered sound branch, and all 17 queue producers confined to legacy HP_Battle UI; current WBA packet route has no matching queue")
