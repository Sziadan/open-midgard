#!/usr/bin/env python3
"""Source and route comparison for effect 47 Torch."""
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


SIGNATURE = "void __thiscall CRagEffect::Torch(CRagEffect *this)"
PE_HANDLER = body(PE, SIGNATURE, PE.index(SIGNATURE + "; // idb") + 1)
REF_HANDLER = body(REF, SIGNATURE)
PE_PROCESS_SIG = "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)"
PE_PROCESS = body(PE, PE_PROCESS_SIG, PE.index(PE_PROCESS_SIG + "; // idb") + 1)
LOCAL_HANDLER = body(CURRENT, "void CRagEffect::SpawnTorch()")

check("canonical PE and primary Ref Torch bodies normalize identically",
      re.sub(r"\s+", " ", PE_HANDLER).strip() == re.sub(r"\s+", " ", REF_HANDLER).strip())
check("PE effect 47 dispatches to Torch",
      re.search(r"case\s+47\s*:\s*CRagEffect::Torch\(this\)", PE_PROCESS) is not None)
check("local effect 47 dispatches to SpawnTorch",
      re.search(r"case\s+47\s*:\s*SpawnTorch\(\);\s*return\s+true;", DISPATCH) is not None)
check("source copies master position before gating the frame-zero spawn",
      PE_HANDLER.index("this->m_pos = this->m_master->m_pos")
      < PE_HANDLER.index("if ( !this->m_stateCnt )"))
check("local valid-master path preserves parent-position copy before frame-zero spawn gate",
      LOCAL_HANDLER.index("ResolveCurrentMaster()")
      < LOCAL_HANDLER.index("m_effectPos = master->m_pos")
      < LOCAL_HANDLER.index("if (m_stateCnt != 0) return;"))

for token in (
    "PP_3DPARTICLE", "m_pattern |= 0x200", "m_duration = m_duration",
    "m_deltaPos2.y = -10.0f", "m_param[0] <= 0.0f", "m_param[1] < 1.0f",
    "m_animSpeed = static_cast<int>(m_param[1])", "torch_01.spr", "torch_01.act",
    "m_alpha = 254.0f", "m_fadeOutCnt = -1", "m_repeatAnim = true",
    "m_numSegments = 1", "ShiftCircleSegments(prim)",
):
    check(f"local Torch retains source anchor: {token}", token in LOCAL_HANDLER)

for phrase in (
    "Torch emits once without RNG or waves",
    "Torch repairs invalid size and speed in persistent parent parameters",
    "Torch keeps source clip-color pattern and constructor alpha/fade/segment defaults",
    "Torch preserves parent duration and source vertical offset in reference routing",
    "Torch requests only the original torch_01 sprite and action",
    "Torch first frame retains alpha and clip-color control while advancing motion",
    "Torch copies parent on later frames without repeated emission",
    "Torch retains positive fractional size and truncates the original animation divisor",
    "missing Torch assets keep reference dispatch without color-name or bitmap fallbacks",
):
    check(f"lifecycle fixture covers {phrase}", phrase in FIXTURE)

print("Torch PE/Ref source, effect47 route and local lifecycle mapping passed.")
