#!/usr/bin/env python3
"""Compare the base actor state dispatcher and its source-level presets."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
REF = (ROOT / "Ref/GameActor.cpp").read_text(encoding="utf-8", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
IMPL = (ROOT / "src/world/GameActor.cpp").read_text(encoding="utf-8")
MSG_REF = (ROOT / "Ref/MsgStrings.cpp").read_text(encoding="utf-8", errors="replace")
errors: list[str] = []


def check(condition: bool, message: str) -> None:
    if not condition:
        errors.append(message)


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


def switch_case_blocks(function_body: str, switch_pattern: str) -> dict[str, str]:
    match = re.search(switch_pattern, function_body)
    if not match:
        return {}
    opening = function_body.index("{", match.end())
    depth = 1
    labels: list[tuple[int, int, str]] = []
    pos = opening + 1
    while pos < len(function_body):
        if depth == 1:
            label = re.match(r"\s*(case\s+([^:]+)|default)\s*:", function_body[pos:])
            if label:
                labels.append((pos + label.start(), pos + label.end(), label.group(2).strip() if label.group(2) else "default"))
                pos += label.end()
                continue
        char = function_body[pos]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                break
        pos += 1
    blocks: dict[str, str] = {}
    for index, (start, content_start, label) in enumerate(labels):
        end = labels[index + 1][0] if index + 1 < len(labels) else pos
        blocks[label] = function_body[content_start:end]
    return blocks


ref_marker = "void __thiscall CGameActor::SetState(CGameActor *this, int stateId)\n{"
impl_marker = "void CGameActor::SetState(int state) {"
ref = body(REF, ref_marker)
exe = body(EXE, ref_marker)
impl = body(IMPL, impl_marker)
check(re.sub(r"\s+", " ", ref) == re.sub(r"\s+", " ", exe), "complete Ref/EXE CGameActor::SetState bodies differ")
if not errors:
    print("PASS complete Ref/EXE CGameActor::SetState body equality")

event_marker = "bool __cdecl IsEventAID(unsigned int Aid)\n{"
event_ref = body(MSG_REF, event_marker)
event_exe = body(EXE, event_marker)
event_impl = body(IMPL, "bool IsEventAID(u32 aid)\n{")
check(re.sub(r"\s+", " ", event_ref) == re.sub(r"\s+", " ", event_exe), "Ref/EXE IsEventAID helper bodies differ")
check(re.sub(r"\s+", " ", event_ref.replace("Aid", "aid")) == re.sub(r"\s+", " ", event_impl), "current IsEventAID predicate differs from source")

ref_cases = switch_case_blocks(ref, r"switch\s*\(\s*v4\s*\)")
exe_cases = switch_case_blocks(exe, r"switch\s*\(\s*v4\s*\)")
impl_cases = switch_case_blocks(impl, r"switch\s*\(\s*state\s*\)")
check(set(ref_cases) == set(exe_cases), f"Ref/EXE top-level state IDs differ: {set(ref_cases) ^ set(exe_cases)}")

state_aliases = {
    "kMoveStateId": 1,
    "kGameActorSkillStateId": 7,
    "kGameActorPickupStateId": 5,
    "kGameActorAttackStateId": 2,
    "kSecondAttackStateId": 9,
    "kGameActorCastingStateId": 8,
    "kGameActorCastingLoopStateId": 13,
    "kHitReactionStateId": 4,
    "kDeathStateId": 3,
    "kGameActorDeathStateId": 3,
}


def normalized_ids(cases: dict[str, str]) -> set[int | str]:
    result: set[int | str] = set()
    for key in cases:
        if key == "default":
            continue
        if key in state_aliases:
            result.add(state_aliases[key])
        elif re.fullmatch(r"-?\d+", key):
            result.add(int(key))
        else:
            result.add(key)
    return result


def ordered_ids(cases: dict[str, str]) -> list[int | str]:
    return [state_aliases.get(key, int(key) if re.fullmatch(r"-?\d+", key) else key)
            for key in cases if key != "default"]


source_ids = normalized_ids(ref_cases)
exe_ids = normalized_ids(exe_cases)
current_ids = normalized_ids(impl_cases)
check(source_ids == exe_ids, f"source/EXE state ID inventories differ: {source_ids ^ exe_ids}")
check(current_ids == source_ids, f"current top-level state IDs differ: missing={source_ids - current_ids}, extra={current_ids - source_ids}")
check(ordered_ids(ref_cases) == ordered_ids(exe_cases), "Ref/EXE state branch order differs")
check(ordered_ids(impl_cases) == ordered_ids(ref_cases), "current state branch order differs from source")
print(f"Source state IDs: {sorted(source_ids)}")
print(f"Current state IDs: {sorted(current_ids)}")


def branch(state_id: int) -> str:
    return impl_cases.get(str(state_id), "")


# Branch anchors for the base states. These assert the consequential actions,
# guards, target-facing path, and state-specific messages rather than merely
# checking labels.
base_anchors = {
    0: r"SetAction\(0,\s*4,\s*0\)[\s\S]*?IsEventAID\(m_gid\)[\s\S]*?m_isMotionFinished\s*=\s*1",
    1: r"m_isAsuraAttack[\s\S]*?SetAction\(8,\s*6,\s*0\)",
    2: r"SetAction\(80,\s*3,\s*1\)[\s\S]*?atan2[\s\S]*?GetAttackMotion\(\)",
    3: r"m_isForceState\s*=\s*0[\s\S]*?m_isForceState2\s*=\s*0[\s\S]*?m_isForceState3\s*=\s*0[\s\S]*?SendMsg\(nullptr,\s*59[\s\S]*?SendMsg\(nullptr,\s*62[\s\S]*?SetAction\(64,\s*4,\s*1\)",
    4: r"m_isForceState[\s\S]*?m_isForceState2[\s\S]*?m_isForceState3[\s\S]*?SendMsg\([^,]*,\s*87[\s\S]*?SetAction\(48,\s*5,\s*1\)",
    5: r"m_isCounter\s*=\s*0[\s\S]*?m_isMotionFreezed\s*=\s*0[\s\S]*?SetAction\(24,\s*6,\s*1\)",
    7: r"m_stateId\s*==\s*6[\s\S]*?SetAction\(80,\s*3,\s*1\)[\s\S]*?m_targetGid[\s\S]*?MakeArrowEffect\(m_pos,\s*m_targetGid,\s*28\.0f,\s*m_job\)[\s\S]*?m_attackMotion",
    9: r"SetAction\(88,\s*3,\s*1\)[\s\S]*?atan2[\s\S]*?GetAttackMotion\(\)",
    11: r"SetAction\(32,\s*6,\s*1\)",
    12: r"SetAction\(40,\s*3,\s*1\)",
    13: r"IsPlayerJob\(m_job\)[\s\S]*?IsSecondAttack\(m_job,\s*m_sex,\s*GetWeapon\(\)\)[\s\S]*?SetAction\(secondAttack \? 88 : 80,\s*4,\s*1\)[\s\S]*?SetAction\(16,\s*4,\s*1\)[\s\S]*?m_isMotionFreezed\s*=\s*1",
}
for state_id, pattern in base_anchors.items():
    branch_text = branch(state_id)
    if state_id == 2:  # state 2 and 9 are adjacent labels sharing one source arm
        branch_text += branch(9)
    check(bool(re.search(pattern, branch_text)), f"state {state_id} implementation branch anchor missing or incomplete")
for state_id in (2, 7, 9):
    check(bool(re.search(r"m_job\s*==\s*1285[\s\S]*?MakeArrowEffect\(m_pos,\s*m_targetGid,\s*28\.0f,\s*m_job\)", branch(state_id) + (branch(9) if state_id == 2 else ""))),
          f"state {state_id} job-1285 special attack arrow route missing")

# Direct preset states: (action, motion, start, max, finish action, finish
# motion, timer ms, effect id). This checks every field in the seven-value
# force-animation preset, not just the initial action/motion.
simple_presets = {
    16: (96, 1, 1, 3, 0, 0, 99_999_999, None),
    17: (96, 1, 1, 3, 96, 1, 99_999_999, None),
    18: (88, 0, 0, 0, 88, 0, 3_000, None),
    21: (96, 0, 0, 0, 96, 0, 2_000, 449),
    22: (96, 2, 2, 2, 96, 2, 2_000, 449),
    23: (96, 3, 3, 3, 96, 3, 2_000, 449),
    24: (96, 4, 4, 4, 96, 4, 2_000, 449),
    25: (24, 1, 1, 1, 24, 1, 2_000, 449),
    27: (96, 5, 5, 5, 96, 5, 3_000, None),
    30: (96, 1, 1, 1, 96, 1, 2_000, None),
    33: (96, 2, 2, 2, 96, 2, 1_000, None),
    34: (96, 3, 3, 3, 96, 3, 1_000, None),
    35: (96, 0, 0, 1, 96, 1, 500, None),
    36: (96, 2, 2, 5, 96, 5, 1_000, None),
    39: (96, 4, 4, 5, 96, 5, 500, None),
    40: (40, 0, 0, 2, 40, 2, 500, None),
    42: (40, 0, 0, 0, 40, 0, 500, None),
}
force_speeds = {
    16: 10, 17: 10, 18: 10, 19: 10, 20: 10, 21: 10, 22: 10,
    23: 10, 24: 10, 25: 10, 26: 4, 27: 10, 28: 10, 30: 10,
    31: 5, 32: 5, 33: 10, 34: 10, 35: 6, 36: 6, 37: 10,
    38: 6, 39: 6, 40: 6, 41: 10, 42: 10,
}
for state_id, speed in force_speeds.items():
    check(bool(re.search(rf"m_forceAnimSpeed\s*=\s*{speed}\b", branch(state_id))), f"state {state_id} force animation speed {speed} missing")

for state_id, (action, motion, start, maximum, finished_action, finished_motion, timer_ms, effect_id) in simple_presets.items():
    text = branch(state_id)
    check(bool(text), f"state {state_id} preset implementation branch missing")
    for field, expected in (("m_forceAct", action), ("m_forceMot", motion), ("m_forceStartMot", start),
                            ("m_forceMaxMot", maximum), ("m_forceFinishedAct", finished_action),
                            ("m_forceFinishedMot", finished_motion)):
        check(bool(re.search(rf"{field}\s*=\s*{expected}\b", text)), f"state {state_id} {field}={expected} missing")
    check(bool(re.search(rf"m_forceStateEndTick\s*=\s*timeGetTime\(\)\s*\+\s*{timer_ms}\b", text)), f"state {state_id} timer +{timer_ms} missing")
    loop_flag = 1 if state_id in (16, 17) else 0
    for field, expected in (("m_isForceAnimLoop", loop_flag), ("m_isForceAnimation", 1),
                            ("m_isForceAnimFinish", 0), ("m_isForceState", 1),
                            ("m_isForceState3", 0), ("m_isForceState2", 0),
                            ("m_forceStateCnt", 1)):
        check(bool(re.search(rf"{field}\s*=\s*{expected}\b", text)), f"state {state_id} {field}={expected} missing")
    if effect_id is not None:
        check(bool(re.search(rf"LaunchEffect\({effect_id}\b", text)), f"state {state_id} effect {effect_id} missing")

# Presets expressed as SetForceAnimation2 sequences. Keep each tuple fully
# explicit, including branch-dependent weapon/job cases and timer.
force2_patterns = {
    19: r"SetForceAnimation2\(40,\s*0,\s*40,\s*0,\s*40,\s*0,\s*40,\s*1,\s*40,\s*1,\s*1,\s*(?:3000|0xBB8u)",
    20: r"SetForceAnimation2\(24,\s*1,\s*24,\s*1,\s*24,\s*1,\s*24,\s*1,\s*8,\s*5,\s*1,\s*(?:3000|0xBB8u)",
    26: r"m_forceAnimSpeed\s*=\s*4[\s\S]*?m_job\s*==\s*4049[\s\S]*?SetForceAnimation2\(88,\s*4,\s*88,\s*5,\s*88,\s*6,\s*88,\s*7,\s*88,\s*2,\s*1,\s*(?:500|0x1F4u)[\s\S]*?SetForceAnimation2\(40,\s*2,\s*40,\s*3,\s*40,\s*4,\s*40,\s*4,\s*40,\s*4,\s*1,\s*(?:500|0x1F4u)",
    28: r"SetForceAnimation2\(40,\s*0,\s*40,\s*1,\s*40,\s*2,\s*40,\s*3,\s*40,\s*4,\s*1,\s*(?:1000|0x3E8u)",
    31: r"SetForceAnimation2\(80,\s*3,\s*80,\s*5,\s*80,\s*7,\s*80,\s*8,\s*80,\s*2,\s*1,\s*(?:500|0x1F4u)",
    32: r"SetForceAnimation2\(88,\s*4,\s*88,\s*5,\s*88,\s*6,\s*88,\s*7,\s*88,\s*2,\s*1,\s*(?:500|0x1F4u)",
    38: r"SetForceAnimation2\(96,\s*3,\s*96,\s*2,\s*96,\s*1,\s*96,\s*0,\s*96,\s*0,\s*1,\s*(?:1000|0x3E8u)",
}
for state_id, pattern in force2_patterns.items():
    check(bool(re.search(pattern, branch(state_id))), f"state {state_id} complete SetForceAnimation2 tuple/timer missing")

# State 19's replacement effects, common loop/finish flag initialization,
# shared timers, and the three source RNG branch predicates are all observable
# behavior, not implementation details that can be inferred from compilation.
for effect_id in (445, 411):
    check(bool(re.search(rf"case\s+19\s*:[\s\S]*?LaunchEffect\({effect_id}\b", impl)), f"state 19 effect {effect_id} missing")
state19 = branch(19)
state19_operations = [
    r"SetForceAnimation2\(40,\s*0,\s*40,\s*0,\s*40,\s*0,\s*40,\s*1,\s*40,\s*1,\s*1,",
    r"m_effectList\.end\(\)[\s\S]*?SendMsg\(nullptr,\s*109,\s*446",
    r"m_effectList\.end\(\)[\s\S]*?SendMsg\(nullptr,\s*109,\s*445",
    r"LaunchEffect\(445", r"LaunchEffect\(411",
]
state19_positions = [re.search(operation, state19).start() if re.search(operation, state19) else -1
                     for operation in state19_operations]
check(all(position >= 0 for position in state19_positions) and state19_positions == sorted(state19_positions),
      "state 19 force preset, effect removals (446 then 445), and 445/411 launch order differ")
check(bool(re.search(r"m_isForceAnimLoop\s*=\s*1[\s\S]*?m_isForceAnimation\s*=\s*1[\s\S]*?m_isForceAnimFinish\s*=\s*0[\s\S]*?m_isForceState\s*=\s*1[\s\S]*?m_isForceState3\s*=\s*0[\s\S]*?m_isForceState2\s*=\s*0", impl)), "loop preset shared force-state flags missing")
check(bool(re.search(r"(?:std::)?rand\(\)[^;]{0,24}&\s*0x80000001", impl)), "source RNG mask `rand() & 0x80000001` missing")
for state_id, timeout in ((37, 500), (38, 1000), (41, 500)):
    check(bool(re.search(rf"case\s+{state_id}\s*:[\s\S]*?rand\(\)[\s\S]*?timeGetTime\(\)\s*\+\s*{timeout}\b", impl)), f"state {state_id} random choice/timer branch missing")
check(bool(re.search(r"case\s+37\s*:[\s\S]*?rand\(\)[\s\S]*?m_forceMot\s*=\s*2[\s\S]*?m_forceMot\s*=\s*3", impl)), "state 37 random motion choices 2/3 missing")
check(bool(re.search(r"case\s+38\s*:[\s\S]*?rand\(\)[\s\S]*?m_forceMaxMot\s*=\s*3[\s\S]*?SetForceAnimation2\(96,\s*3,\s*96,\s*2,\s*96,\s*1,\s*96,\s*0,\s*96,\s*0", impl)), "state 38 random direct/reverse animation alternatives missing")
check(bool(re.search(r"case\s+41\s*:[\s\S]*?rand\(\)[\s\S]*?m_forceMot\s*=\s*1[\s\S]*?m_forceMot\s*=\s*2", impl)), "state 41 random motion choices 1/2 missing")
check("case 29:" not in impl, "state 29 is not a source top-level state and must not be added")
check(bool(re.search(r"if \(state == -1\)\s*\{\s*return;\s*\}", impl)), "state -1 early return missing")
check(bool(re.search(r"if \(state == 4 && m_stateId == 1\)\s*\{\s*m_stateId = 0;\s*\}", impl)), "state-4 prior-move transition missing")
check(bool(re.search(r"m_curAction = m_baseAction \+ Get8Dir\(m_roty\);[\s\S]*?if \(m_stateId == 0 \|\| m_stateId == 6\)[\s\S]*?if \(state != 4\)\s*\{\s*m_stateId = state;\s*\}[\s\S]*?m_stateStartTick = timeGetTime\(\);", impl)),
      "common state/action/oldstate/timestamp tail differs")

if errors:
    for error in errors:
        print(f"FAIL {error}")
    raise SystemExit(1)
print("PASS exact state inventory and all base/preset branch anchors")
