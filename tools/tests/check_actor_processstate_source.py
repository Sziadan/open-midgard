#!/usr/bin/env python3
"""Compare base actor ProcessState against Ref/EXE at state and branch level."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
REF = (ROOT / "Ref/GameActor.cpp").read_text(encoding="utf-8", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
IMPL = (ROOT / "src/world/GameActor.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "src/world/GameActor.h").read_text(encoding="utf-8")
errors: list[str] = []


def check(ok: bool, message: str) -> None:
    if not ok:
        errors.append(message)


def function_body(text: str, marker: str) -> str:
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
    raise AssertionError(f"unclosed function body: {marker}")


def case_blocks(body: str) -> dict[str, str]:
    match = re.search(r"switch\s*\(\s*(?:this->)?m_stateId\s*\)", body)
    if not match:
        return {}
    opening = body.index("{", match.end())
    depth = 1
    labels: list[tuple[int, int, str]] = []
    pos = opening + 1
    while pos < len(body):
        if depth == 1:
            label = re.match(r"\s*(case\s+([^:]+)|default)\s*:", body[pos:])
            if label:
                key = label.group(2).strip() if label.group(2) else "default"
                labels.append((pos + label.start(), pos + label.end(), key))
                pos += label.end()
                continue
        if body[pos] == "{":
            depth += 1
        elif body[pos] == "}":
            depth -= 1
            if depth == 0:
                break
        pos += 1
    blocks: dict[str, str] = {}
    for index, (_, content_start, key) in enumerate(labels):
        end = labels[index + 1][0] if index + 1 < len(labels) else pos
        blocks[key] = body[content_start:end]
    return blocks


ref_marker = "unsigned __int8 __thiscall CGameActor::ProcessState(CGameActor *this)\n{"
impl_marker = "u8 CGameActor::ProcessState() {"
ref = function_body(REF, ref_marker)
exe = function_body(EXE, ref_marker)
impl = function_body(IMPL, impl_marker)
normalize = lambda value: re.sub(r"\s+", " ", value).strip()
check(normalize(ref) == normalize(exe), "complete Ref/EXE CGameActor::ProcessState bodies differ")
if normalize(ref) == normalize(exe):
    print("PASS complete Ref/EXE ProcessState body equality")

ref_cases = case_blocks(ref)
exe_cases = case_blocks(exe)
impl_cases = case_blocks(impl)

# Resolve the actual current C++ state constants rather than hard-coding only
# the state values represented by their names.
constant_exprs: dict[str, str] = {}
for source in (HEADER, IMPL):
    for name, value in re.findall(r"constexpr\s+int\s+(\w+)\s*=\s*([^;]+);", source):
        constant_exprs[name] = value.strip()
constant_values: dict[str, int] = {}


def resolve_constant(name: str, active: set[str] | None = None) -> int | None:
    if name in constant_values:
        return constant_values[name]
    active = active or set()
    if name in active or name not in constant_exprs:
        return None
    active.add(name)
    expression = constant_exprs[name]
    if re.fullmatch(r"-?\d+|0[xX][0-9a-fA-F]+", expression):
        value = int(expression, 0)
    elif re.fullmatch(r"\w+", expression):
        value = resolve_constant(expression, active)
    else:
        value = None
    if value is not None:
        constant_values[name] = value
    return value


def id_for(label: str) -> int | str:
    if re.fullmatch(r"-?\d+|0[xX][0-9a-fA-F]+", label):
        return int(label, 0)
    resolved = resolve_constant(label)
    return resolved if resolved is not None else label


def ordered_ids(cases: dict[str, str]) -> list[int | str]:
    return [id_for(label) for label in cases if label != "default"]


source_ids = set(ordered_ids(ref_cases))
exe_ids = set(ordered_ids(exe_cases))
current_ids = set(ordered_ids(impl_cases))
check(source_ids == exe_ids, f"Ref/EXE ProcessState IDs differ: {source_ids ^ exe_ids}")
check(ordered_ids(ref_cases) == ordered_ids(exe_cases), "Ref/EXE ProcessState branch order differs")
check(current_ids == source_ids,
      f"current ProcessState IDs differ: missing={source_ids-current_ids}, extra={current_ids-source_ids}")
check(ordered_ids(impl_cases) == ordered_ids(ref_cases), "current ProcessState state order differs from source")
print(f"Source ordered state IDs: {ordered_ids(ref_cases)}")
print(f"Current ordered state IDs: {ordered_ids(impl_cases)}")


def has(text: str, pattern: str) -> bool:
    return re.search(pattern, text) is not None


def branch_source(cases: dict[str, str], state: int) -> str:
    return " ".join(value for label, value in cases.items() if id_for(label) == state)


def current_group(*states: int) -> str:
    return " ".join(value for label, value in impl_cases.items() if id_for(label) in states)


# Assert the canonical source itself still contains the audited decisions,
# protecting these current-client anchors from a stale/manual interpretation.
source_anchors = {
    "freeze expiry and vanish alpha": r"m_freezeEndTick[\s\S]*?m_vanishTime[\s\S]*?m_sprColor\.a",
    "movement path and Asura preset": r"GetNextCellInfo[\s\S]*?m_isAsuraAttack[\s\S]*?m_forceAct\s*=\s*96[\s\S]*?m_forceStateEndTick",
    "idle pet-emotion branch": r"m_petEmotionStartTick[\s\S]*?rand\(\)\s*%\s*100\s*<\s*25[\s\S]*?PetTalkNumber",
    "death corpse gate": r"m_willBeAttackedList\._Size[\s\S]*?SendMsg\([^;]*13[\s\S]*?MakeCorpse",
    "knockback interpolation": r"m_damageDestX[\s\S]*?m_lastProcessStateTime[\s\S]*?m_damageDestZ",
    "casting attack/recharge": r"IsSecondAttack[\s\S]*?SetAction\(this,\s*88[\s\S]*?m_skillRechargeEndTick[\s\S]*?83",
    "shared timestamps and height": r"m_lastProcessStateTime\s*=\s*v38[\s\S]*?m_lastServerTime\s*=\s*v39[\s\S]*?GetHeight",
}
for description, pattern in source_anchors.items():
    check(has(ref, pattern), f"canonical source anchor missing: {description}")

# Required source-to-current branch behavior. Each grouped case arm is checked
# as a whole because adjacent labels in C++ share one body.
current_anchors = {
    "freeze expiry uses strict source boundary": r"m_freezeEndTick\s*&&\s*timeGetTime\(\)\s*>\s*m_freezeEndTick",
    "vanish early return and alpha update": r"if\s*\(\s*Time\s*>\s*m_vanishTime\s*\)[\s\S]*?return\s+0[\s\S]*?m_sprColor\.a",
    "state 0 idle completion and pet emotion": (0, r"m_motionType\s*==\s*1[\s\S]*?ProcessMotion\(\)[\s\S]*?m_petEmotionStartTick[\s\S]*?rand\(\)\s*%\s*100\s*<\s*25[\s\S]*?PetTalkNumber"),
    "state 1 path-cell timing and movement-motion distance": (1, r"FindActivePathSegment|GetNextCellInfo[\s\S]*?m_lastServerTime[\s\S]*?m_dist[\s\S]*?ProcessMotionWithDist"),
    "state 1 special stance and Asura force tuple/timer": (1, r"m_gid\s*==\s*3[\s\S]*?SetState\(11\)[\s\S]*?m_isAsuraAttack[\s\S]*?m_forceAct\s*=\s*96[\s\S]*?m_forceStateEndTick\s*=\s*timeGetTime\(\)\s*\+\s*3000"),
    "states 2/9/12 process then return idle": (2, 9, 12, r"ProcessMotion\(\)[\s\S]*?m_isMotionFinished[\s\S]*?SetState\(0\)"),
    "state 3 waits for attacks, sends 13, and creates corpse": (3, r"m_isForceState\s*=\s*0[\s\S]*?ProcessMotion\(\)[\s\S]*?m_willBeAttackedList\.empty\(\)[\s\S]*?SendMsg\([^;]*13[\s\S]*?MakeCorpse"),
    "state 4 interpolates damage destination from prior process time": (4, r"m_damageDestX[\s\S]*?m_lastProcessStateTime[\s\S]*?m_damageDestZ[\s\S]*?SetState\(0\)"),
    "states 5/7 process motion without added freeze behavior": (5, 7, r"m_isCounter\s*=\s*0[\s\S]*?ProcessMotion\(\)[\s\S]*?m_isMotionFinished[\s\S]*?SetState\(0\)"),
    "state 8 periodically faces target without recharge handling": (8, r"m_targetGid[\s\S]*?%\s*34[\s\S]*?m_roty\s*=\s*std::atan2"),
    "state 10 clears counter only": (10, r"m_isCounter\s*=\s*0"),
    "state 11 repeats state after motion finishes": (11, r"ProcessMotion\(\)[\s\S]*?SetState\(11\)"),
    "state 13 selects attack action, freezes, then completes recharge": (13, r"IsSecondAttack\([\s\S]*?SetAction\(secondAttack\s*\?\s*88\s*:\s*80[\s\S]*?m_isMotionFreezed\s*=\s*1[\s\S]*?m_skillRechargeEndTick[\s\S]*?SendMsg\(this,\s*83"),
    "states 16-28 and 30-42 clear counter and process motion": (*range(16, 29), *range(30, 43), r"m_isCounter\s*=\s*0[\s\S]*?ProcessMotion\(\)"),
}
for description, entry in current_anchors.items():
    *states, pattern = entry
    check(has(current_group(*states), pattern), f"current implementation mismatch: {description}")
check(has(impl, r"m_lastProcessStateTime\s*=\s*timeGetTime\(\)[\s\S]*?m_lastServerTime\s*=\s*serverTime[\s\S]*?if\s*\(\s*posChanged\s*\)[\s\S]*?GetHeight"),
      "current implementation mismatch: common timestamp and conditional terrain-height tail")

# These client-only integrations are allowed to coexist with the source
# state machine, and are required to remain reachable after the port.
integration_anchors = {
    "queued attack dispatch": r"ProcessWillBeAttacked\(\)",
    "current hide-fade integration": r"CancelHideFade\(\)",
    "current effect-state integration": r"m_effectLaunchCnt",
    "queued hit-wave integration": r"ProcessQueuedHitWave\(\s*(?:\*this)?\s*\)",
    "movement-stall diagnostic integration": r"g_moveStallTraceByGid",
}
for description, pattern in integration_anchors.items():
    check(has(impl, pattern), f"permitted current integration missing: {description}")

if errors:
    for error in errors:
        print(f"FAIL {error}")
    raise SystemExit(1)
print("PASS exact ProcessState inventory and source branch/integration anchors")
