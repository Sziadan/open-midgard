#!/usr/bin/env python3
"""Gate SetForceAnimation2 against its Ref and canonical EXE source bodies."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
REF = (ROOT / "Ref/GameActor.cpp").read_text(encoding="utf-8", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")

MARKER = "void __thiscall CRenderObject::SetForceAnimation2(\n        CRenderObject *this,"


def function_parts(text: str, marker: str) -> tuple[str, str]:
    search_from = 0
    start = -1
    opening = -1
    while True:
        candidate = text.find(marker, search_from)
        if candidate < 0:
            break
        brace = text.find("{", candidate)
        semicolon = text.find(";", candidate)
        if brace >= 0 and (semicolon < 0 or brace < semicolon):
            start, opening = candidate, brace
            break
        search_from = candidate + len(marker)
    if start < 0:
        raise AssertionError(f"definition not found: {marker}")
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[start:opening].strip(), text[opening + 1:pos]
    raise AssertionError("unclosed SetForceAnimation2 body")


ref_signature, ref_body = function_parts(REF, MARKER)
exe_signature, exe_body = function_parts(EXE, MARKER)
normalize = lambda value: re.sub(r"\s+", " ", value).strip()
assert normalize(ref_signature) == normalize(exe_signature), "Ref and executable SetForceAnimation2 signatures differ"
assert normalize(ref_body) == normalize(exe_body), "Ref and executable SetForceAnimation2 bodies differ"
print("PASS exact Ref/executable signature and complete body equality")

expected_params = [
    "CRenderObject *this", "unsigned __int8 act", "unsigned __int8 mot",
    "unsigned __int8 act2", "unsigned __int8 mot2", "unsigned __int8 act3",
    "unsigned __int8 mot3", "unsigned __int8 act4", "unsigned __int8 mot4",
    "unsigned __int8 act5", "unsigned __int8 mot5", "int isAnim",
    "unsigned int endTick", "int isLoop",
]
for param in expected_params:
    assert param in normalize(ref_signature), f"canonical function signature is missing {param}"

ordered_operations = [
    "m_forceAct2[1] = act2", "m_forceMot2[1] = mot2",
    "m_forceAct2[2] = act3", "m_forceMot2[2] = mot3",
    "m_forceAct2[3] = act4", "m_forceMot2[0] = mot",
    "m_forceMot = mot", "m_forceMot2[3] = mot4",
    "m_forceAct2[0] = act", "m_forceAct = act",
    "m_isForceAnimLoop = isLoop", "m_forceAct2[4] = act5",
    "v15 = 0", "m_forceMot2[4] = mot5", "m_forceStartMot = 0",
    "m_forceMaxMot = 4", "m_isForceAnimation = isAnim",
    "m_isForceAnimFinish = 0", "m_isForceState2 = 1",
    "m_isForceState = 0", "m_isForceState3 = 0",
    "if ( isAnim )", "v15 = endTick + timeGetTime()",
    "m_forceStateEndTick = v15", "m_forceStateCnt = 1",
]
positions = [ref_body.index(operation) for operation in ordered_operations]
assert positions == sorted(positions), "SetForceAnimation2 field writes/flags/timer are out of canonical order"
assert len(re.findall(r"timeGetTime\s*\(\)", ref_body)) == 1, "conditional timer must sample the clock once"
timer_if = re.search(r"if \( isAnim \)\s*v15 = endTick \+ timeGetTime\(\);", ref_body)
assert timer_if, "end timer must be set only when isAnim is true"
assert ref_body.index("v15 = 0") < timer_if.start() < ref_body.index("m_forceStateEndTick = v15")
print("PASS ordered five-frame action/motion writes, force-state flags, and conditional end timer")
