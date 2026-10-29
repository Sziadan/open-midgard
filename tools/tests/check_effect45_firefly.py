#!/usr/bin/env python3
"""Source and route comparison for effect 45 FireFly."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="cp1252", errors="replace")
CURRENT = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
FIXTURE = (ROOT / "tools/tests/EffectLifecycleTests.cpp").read_text(encoding="utf-8")


def body(text: str, marker: str, start: int = 0) -> str:
    index = text.index(marker, start)
    opening = text.index("{", index)
    depth = 0
    for cursor in range(opening, len(text)):
        if text[cursor] == "{":
            depth += 1
        elif text[cursor] == "}":
            depth -= 1
            if depth == 0:
                return text[opening + 1:cursor]
    raise AssertionError(f"unclosed body: {marker}")


def check(label: str, condition: bool) -> None:
    assert condition, f"FAIL {label}"
    print(f"PASS {label}")


SIGNATURE = "void __thiscall CRagEffect::FireFly(CRagEffect *this)"
PE_HANDLER = body(PE, SIGNATURE, PE.index(SIGNATURE + "; // idb") + 1)
REF_HANDLER = body(REF, SIGNATURE)
PE_PROCESS_SIG = "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)"
PE_PROCESS = body(PE, PE_PROCESS_SIG, PE.index(PE_PROCESS_SIG + "; // idb") + 1)
LOCAL_HANDLER = body(CURRENT, "void CRagEffect::SpawnFireFly()")
LOCAL_PROCESS = body(CURRENT, "bool CRagEffect::ProcessEffectFrameLegacy()")

check("canonical PE and primary Ref FireFly bodies normalize identically",
      re.sub(r"\s+", " ", PE_HANDLER).strip() == re.sub(r"\s+", " ", REF_HANDLER).strip())
check("PE effect 45 dispatches to FireFly",
      re.search(r"case\s+45\s*:\s*CRagEffect::FireFly\(this\)", PE_PROCESS) is not None)
check("local effect 45 dispatches to SpawnFireFly",
      re.search(r"case\s+45\s*:\s*return\s+ProcessEffectFrameLegacy\(\)", DISPATCH) is not None
      and re.search(r"if\s*\(m_type\s*==\s*45\)\s*\{\s*SpawnFireFly\(\);\s*return\s+true;\s*\}", LOCAL_PROCESS) is not None)
check("source and local emit only at state zero",
      re.search(r"if\s*\(\s*!this->m_stateCnt\s*\)", PE_HANDLER) is not None
      and "if (m_stateCnt != 0) return;" in LOCAL_HANDLER)
check("local particle preserves six ordered random draws",
      LOCAL_HANDLER.index("rand() % 35 + 15")
      < LOCAL_HANDLER.index("rand() % 360")
      < LOCAL_HANDLER.index("rand() % 360", LOCAL_HANDLER.index("rand() % 360") + 1)
      < LOCAL_HANDLER.index("rand() % 13 + 2")
      < LOCAL_HANDLER.index("rand() % 25 + 5")
      < LOCAL_HANDLER.index("rand() % 70 + 30"))

for token in (
    "prim->m_pattern |= 0x84", "prim->m_duration = 140", "m_duration = 140",
    "MatrixAppendYRotation", "MatrixAppendXRotation", "prim->m_count = rand() % 25 + 5",
    "prim->m_AlphaShow = true", "prim->m_chPoint = {40.0f,100.0f}",
    "prim->m_chVal1 = {120.0f,0.0f}", "prim->m_chVal2 = {200.0f,80.0f}",
    "particle1.spr", "particle1.act", "prim->m_fadeOutCnt = -1",
    "prim->m_numSegments = 1", "ShiftCircleSegments(prim)",
):
    check(f"local FireFly retains source anchor: {token}", token in LOCAL_HANDLER)

for phrase in (
    "FireFly zero frame sets parent lifetime and original random-steering twinkle profile",
    "FireFly preserves six-draw speed longitude latitude radius interval size order",
    "FireFly stores rotated launch offset while preserving effect origin",
    "FireFly enables keyed alpha with source twinkle rise and constructor fade",
    "FireFly preserves ordered frame alpha and maximum-alpha key arrays",
    "FireFly initializes segment after exact particle-one resource requests",
    "FireFly integrates previous launch matrix and speed before random steering replacement",
    "FireFly non-steering frame consumes no RNG and clamps alpha to current key maximum",
):
    check(f"lifecycle fixture covers {phrase}", phrase in FIXTURE)

print("FireFly PE/Ref source, effect45 route and local lifecycle mapping passed.")
