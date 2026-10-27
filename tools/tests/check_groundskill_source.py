#!/usr/bin/env python3
"""Compare the ground-skill dispatcher and its effect routes to the canonical sources."""
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
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[opening + 1:pos]
    raise AssertionError(f"unclosed body: {marker}")


ref = body(REF, "void __thiscall CGameActor::Am_Groundskill(CGameActor *this, CGameObject *sender, int *arg1, int arg2, int arg3)\n{")
exe = body(EXE, "void __thiscall CGameActor::Am_Groundskill(CGameActor *this, CGameObject *sender, int *arg1, int arg2, int arg3)\n{")
impl = body(IMPL, "void CGameActor::Am_Groundskill(const int* arg1)\n{")
helper = body(IMPL, "void ApplyGroundEffect(int skillId, int level, const vector3d& pos)\n{")

assert re.sub(r"\s+", " ", ref) == re.sub(r"\s+", " ", exe), "Ref and executable Am_Groundskill bodies differ"
print("PASS Ref/executable Am_Groundskill body equality")

# The effect IDs are extracted from both canonical bodies and from the actual
# current helper. Exact per-skill branches below guard against set-only parity.
source_effects = {int(value) for value in re.findall(r"CGameMode::PlaceEffect\(GameMode, &dPos, (\d+),", exe)}
impl_effects = {int(value) for value in re.findall(r"PlaceGroundEffect\(pos, (\d+)\)", helper)}
assert source_effects == impl_effects, f"ground effect ID mismatch: source-only={source_effects - impl_effects}, implementation-only={impl_effects - source_effects}"
print(f"PASS all {len(source_effects)} canonical ground effect IDs are represented")

# The skill IDs in the optimized EXE are partly expressed as arithmetic
# branches (for example, 286..288) and shared labels. Keep the semantic
# source-to-port map explicit, then compare its exact membership and ordering.

expected_routes = {
    21: 30, 25: 141, 69: 91, 70: 83, 79: 113, 83: 92, 85: 90,
    87: 74, 89: 89, 91: 142, 92: 95, 110: 102, 111: 98,
    115: 69, 130: 119, 140: 124, 285: 225, 286: 236,
    287: 237, 288: 238, 459: 98, 694: 91,
}
actual_routes = {}
for labels, effect in re.findall(r"((?:case \d+:\s*)+)PlaceGroundEffect\(pos, (\d+)\);", helper):
    for skill in re.findall(r"case (\d+):", labels):
        actual_routes[int(skill)] = int(effect)
for skill, effect in expected_routes.items():
    assert actual_routes.get(skill) == effect, f"skill {skill} routes to {actual_routes.get(skill)}, expected effect {effect}"
assert actual_routes == expected_routes, f"unexpected ground skill routes: {actual_routes}"
source_route_ids = {21, 25, 69, 70, 79, 83, 85, 87, 89, 91, 92, 110, 111,
                    115, 130, 140, 285, 286, 287, 288, 459, 478, 483, 694}
implementation_route_ids = {int(value) for value in re.findall(r"case (\d+):", helper)}
assert implementation_route_ids == source_route_ids, f"ground skill case IDs differ: {implementation_route_ids ^ source_route_ids}"
assert re.search(r"case 478:[\s\S]*?if \(level >= 10\)\s*\{\s*PlaceGroundEffect\(pos, 499\);\s*\} else if \(level >= 6\)\s*\{\s*PlaceGroundEffect\(pos, 498\);\s*\} else\s*\{\s*PlaceGroundEffect\(pos, 497\);", helper)
assert re.search(r"case 483:[\s\S]*?PlaceGroundEffect\(pos, 223\);", helper)
print("PASS skill-to-effect routes, including 478 level thresholds and 483")

assert re.search(r"if \( level >= 6 \)[\s\S]*?if \( level >= 10 \)[\s\S]*?PlaceEffect\(GameMode, &dPos, 499,[\s\S]*?else[\s\S]*?PlaceEffect\(GameMode, &dPos, 498,[\s\S]*?else[\s\S]*?PlaceEffect\(GameMode, &dPos, 497,", exe)
assert re.search(r"case 483:[\s\S]*?CGameMode::PlaceEffect\(GameMode, &dPos, 223,", exe)
assert re.search(r"case 478:\s*PlayWave\(\"assulter_attack\.wav\", 0\.0f, -150\.0f, 0\.0f\);", helper)
assert re.search(r"case 483:\s*PlaySkillWaveAtPlayer\(\"effect\\\\EF_FireWall\.wav\"\);", helper)
print("PASS source-matched skill 478/483 sound branches and 478 spatial offset")

for skill, pattern in {
    83: r"if \( v7 == 83 \)[\s\S]*?this->m_curAction = 88;[\s\S]*?switch \( v7 \)",
    130: r"if \( v7 != 130 \)[\s\S]*?PlaceEffect\(GameMode, &dPos, 119,[\s\S]*?m_birdEffect",
    484: r"case 484:[\s\S]*?PlayWave\(aEffectWizardEa",
    491: r"case 491:[\s\S]*?LaunchEffect\(this, 58",
    521: r"case 521:[\s\S]*?PlayWave\(aEffect_107",
    538: r"case 538:[\s\S]*?PlayWave\(aEffect_117",
}.items():
    assert re.search(pattern, exe), f"canonical special branch missing for skill {skill}"
assert re.search(r"gameMode->PlaceEffect\(pos, effectId, kRagEffectDurationTable\[effectId\] \+ 10, 0\);", IMPL)
assert re.search(r"m_targetGid = 0;", impl)

# Branch-level comparison for the non-table side effects and their ordering.
assert re.search(r"skillId == 83\)\s*\{\s*m_curAction = 88;\s*if \(m_actRes\)\s*\{\s*m_curMotion = m_actRes->GetMotionCount\(88\) - 1;\s*\}[\s\S]*?ApplyGroundEffect\(skillId, level, dest\);\s*return;", impl)
assert re.search(r"if \(skillId == 130\)\s*\{\s*ApplyGroundEffect\(skillId, level, dest\);\s*if \(m_birdEffect\)[\s\S]*?29,[\s\S]*?&dest[\s\S]*?return;", impl)
assert re.search(r"if \(skillId == 264\)\s*\{\s*if \(IsPlayerJob\(m_job\)\)[\s\S]*?LaunchEffect\(166[\s\S]*?44, 0xFFFF, 243, 100[\s\S]*?SendMsg\(this, 19, cellX, cellY, 15\);\s*SetAction\(m_baseAction \+ 80, 6, 1\);\s*m_curMotion = 7;\s*m_isMotionFreezed = 1;", impl)
player_predicate = body((ROOT / "src/world/GameActorMsgHandler.cpp").read_text(encoding="utf-8"), "bool IsPlayerJob(int job)\n{")
session_predicate = body((ROOT / "Ref/Session.cpp").read_text(encoding="utf-8", errors="replace"), "BOOL __thiscall CSession::IsThisPC(CSession *this, int job)\n{")
assert re.search(r"return job > 4000 && job < 6000 \|\| \(unsigned int\)job < 0x1C;", session_predicate)
assert re.search(r"return \(job >= 0 && job < 28\) \|\| \(job > 4000 && job < 6000\);", player_predicate)
for job in range(-10000, 10001):
    canonical_player = (job > 4000 and job < 6000) or (job % (1 << 32) < 28)
    implementation_player = (job >= 0 and job < 28) or (job > 4000 and job < 6000)
    assert canonical_player == implementation_player, f"player-job predicate differs at job {job}"
assert re.search(r"skillId == 484\)[\s\S]*?wizard_earthspike\.wav[\s\S]*?return;", impl)
assert re.search(r"skillId == 491\)[\s\S]*?LaunchEffect\(58[\s\S]*?return;", impl)
assert re.search(r"skillId == 521\)[\s\S]*?\\xBC\\xF6\\xB5\\xD0[\s\S]*?return;", impl)
assert re.search(r"skillId == 538\)[\s\S]*?\\xB1\\xD7\\xB6\\xF3\\xBF\\xEE\\xB5\\xE5[\s\S]*?return;", impl)
assert re.search(r"GetSkillActionInfo\(skillId, beginEffectId, skillMotionType\);[\s\S]*?GetClientCoor\(cellX, cellY, destX, destZ\);[\s\S]*?GetHeight\(destX, destZ\);[\s\S]*?m_targetGid = 0;", impl)
print("PASS branch ordering/side effects, IsThisPC equivalence, coordinates, lifetime (+10), and target reset")
