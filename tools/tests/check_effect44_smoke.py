#!/usr/bin/env python3
"""Source and route comparison for effect 44 Smoke."""
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


SIGNATURE = "void __thiscall CRagEffect::Smoke(CRagEffect *this)"
PE_HANDLER = body(PE, SIGNATURE, PE.index(SIGNATURE + "; // idb") + 1)
REF_HANDLER = body(REF, SIGNATURE)
PE_PROCESS_SIG = "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)"
PE_PROCESS = body(PE, PE_PROCESS_SIG, PE.index(PE_PROCESS_SIG + "; // idb") + 1)
LOCAL_HANDLER = body(CURRENT, "void CRagEffect::SpawnSmoke()")

check("canonical PE and primary Ref Smoke bodies normalize identically",
      re.sub(r"\s+", " ", PE_HANDLER).strip() == re.sub(r"\s+", " ", REF_HANDLER).strip())
check("PE effect 44 dispatches to Smoke",
      re.search(r"case\s+44\s*:\s*CRagEffect::Smoke\(this\)", PE_PROCESS) is not None)
check("local effect 44 dispatches to SpawnSmoke",
      re.search(r"case\s+44\s*:\s*SpawnSmoke\(\);\s*return\s+true;", DISPATCH) is not None)
check("source and local copy the parent before the state-zero emission gate",
      PE_HANDLER.index("this->m_pos = this->m_master->m_pos")
      < PE_HANDLER.index("if ( !this->m_stateCnt )")
      and LOCAL_HANDLER.index("m_effectPos = master->m_pos")
      < LOCAL_HANDLER.index("if (m_stateCnt != 0) return;"))
check("local smoke uses one count draw then three ordered draws per particle",
      LOCAL_HANDLER.index("rand() % 4 + 1")
      < LOCAL_HANDLER.index("rand() % 30 + 75")
      < LOCAL_HANDLER.index("rand() % 360")
      < LOCAL_HANDLER.index("rand() % 3 + 3")
      and LOCAL_HANDLER.count("LaunchEffectPrim(PP_3DPARTICLE,") == 1)

for token in (
    "prim->m_duration = m_duration", "MatrixAppendXRotation", "MatrixAppendYRotation",
    "prim->m_deltaPos2.y = -9.0f", "prim->m_rollSpeed = 0.75f", "prim->m_size = 1.5f",
    "prim->m_fadeOutCnt = prim->m_duration - prim->m_duration / 3",
    r"\xB1\xBC\xB6\xD2\xBF\xAC\xB1\xE2.spr", r"\xB1\xBC\xB6\xD2\xBF\xAC\xB1\xE2.act",
    "prim->m_alpha = 254.0f", "prim->m_animSpeed = 4", "prim->m_numSegments = 1",
    "ShiftCircleSegments(prim)",
):
    check(f"local Smoke retains source anchor: {token}", token in LOCAL_HANDLER)

for phrase in (
    "Smoke consumes one count draw then X-angle, Y-angle and speed for each particle",
    "Smoke copies parent position without a wave call",
    "Smoke retains original constructor alpha instead of synthetic fade-in",
    "Smoke preserves source size, offset, roll and two-thirds fade point",
    "Smoke initializes one reference sprite segment with looping divisor four",
    "Smoke speed draws preserve original minimum and maximum",
    "Smoke composes X rotation followed by Y rotation with production matrix code",
    "Smoke X-angle draw reaches original upper boundary 104 degrees",
    "Smoke loads only the original recovered sprite and action",
    "Smoke updates parent position after frame zero without re-emission or RNG",
    "Smoke maximum count draw emits exactly four particles",
):
    check(f"lifecycle fixture covers {phrase}", phrase in FIXTURE)

print("Smoke PE/Ref source, effect44 route and local lifecycle mapping passed.")
