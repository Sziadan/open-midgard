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
print("PASS special branches, world-effect lifetime (+10 frames), and target reset")
