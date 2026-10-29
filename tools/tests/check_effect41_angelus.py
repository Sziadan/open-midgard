#!/usr/bin/env python3
"""Source and route comparison for effect 41 Angelus."""
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


SIGNATURE = "bool __thiscall CRagEffect::Angelus(CRagEffect *this)"
PE_HANDLER = body(PE, SIGNATURE, PE.index(SIGNATURE + ";") + 1)
REF_HANDLER = body(REF, SIGNATURE)
PE_PROCESS_SIG = "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)"
PE_PROCESS = body(PE, PE_PROCESS_SIG, PE.index(PE_PROCESS_SIG + "; // idb") + 1)
CASE_START = PE_PROCESS.index("case 41:")
CASE_END = PE_PROCESS.index("case 42:", CASE_START)
PE_CASE = PE_PROCESS[CASE_START:CASE_END]
PE_COMMON = PE_PROCESS[PE_PROCESS.index("LABEL_424:"):PE_PROCESS.index("case 259:", PE_PROCESS.index("LABEL_424:"))]
LOCAL_HANDLER = body(CURRENT, "bool CRagEffect::ProcessAngelus()")

check("canonical PE and primary Ref Angelus helper bodies normalize identically",
      re.sub(r"\s+", " ", PE_HANDLER).strip() == re.sub(r"\s+", " ", REF_HANDLER).strip())
check("PE OnProcess case41 keeps the inlined zero-state Angelus wave route",
      "if ( !this->m_stateCnt )" in PE_CASE and "PlayWave(aEffectEfAngelu" in PE_CASE
      and "goto LABEL_424;" in PE_CASE)
check("PE shared Angelus tail calls ProcessEZ2STR with source offsets and propagates completion",
      "ProcessEZ2STR(this, 0.0, -80.0, 0.0)" in PE_COMMON
      and "if ( !result )\n              return result;" in PE_COMMON)
check("local effect41 dispatches to ProcessAngelus",
      re.search(r"case\s+41\s*:\s*return\s+ProcessAngelus\(\)", DISPATCH) is not None)
check("local helper preserves zero-state audio before shared STR processing",
      LOCAL_HANDLER.index("if (m_stateCnt == 0)") < LOCAL_HANDLER.index("PlayWave(")
      < LOCAL_HANDLER.index("return ProcessEZ2STR(0.0f,-80.0f,0.0f) != 0;"))

for token in (
    '"effect\\\\EF_Angelus.wav"', "m_effectPos.x-listener.x", "m_effectPos.z-listener.z",
    "250,40,1.0f", "ProcessEZ2STR(0.0f,-80.0f,0.0f)",
):
    check(f"local Angelus retains source anchor: {token}", token in LOCAL_HANDLER)

for phrase in (
    "Angelus plays zero-frame audio before propagating missing STR despite reduced effects",
    "Angelus exact sound uses stored effect origin and horizontal listener deltas with source parameters",
    "Angelus missing STR preserves screen fields without extra actor projection",
    "Angelus missing STR on later frames returns false without repeating audio",
    "Angelus active STR continues after zero-frame sound",
    "Angelus STR applies original minus-eighty offset to actor projection",
    "Angelus follows moving actor for STR while retaining stored audio origin and parent lifetime",
    "Angelus propagates exhausted STR completion without extra audio",
):
    check(f"lifecycle fixture covers {phrase}", phrase in FIXTURE)

print("Angelus PE/Ref source, effect41 inline route and local lifecycle mapping passed.")
