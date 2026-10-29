#!/usr/bin/env python3
"""Source and route comparison for effect 40 Signum."""
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


SIGNATURE = "bool __thiscall CRagEffect::Signum(CRagEffect *this)"
PE_HANDLER = body(PE, SIGNATURE, PE.index(SIGNATURE + ";") + 1)
REF_HANDLER = body(REF, SIGNATURE)
PE_PROCESS_SIG = "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)"
PE_PROCESS = body(PE, PE_PROCESS_SIG, PE.index(PE_PROCESS_SIG + "; // idb") + 1)
LOCAL_HANDLER = body(CURRENT, "bool CRagEffect::ProcessSignum()")

check("canonical PE and primary Ref Signum bodies normalize identically",
      re.sub(r"\s+", " ", PE_HANDLER).strip() == re.sub(r"\s+", " ", REF_HANDLER).strip())
check("PE effect 40 dispatches to Signum",
      re.search(r"case\s+40\s*:\s*result\s*=\s*CRagEffect::Signum\(this\)", PE_PROCESS) is not None)
check("local effect 40 dispatches to ProcessSignum",
      re.search(r"case\s+40\s*:\s*return\s+ProcessEffectFrameLegacy\(\)", DISPATCH) is not None
      and re.search(r"if\s*\(m_type\s*==\s*40\)\s*return\s+ProcessSignum\(\);",
                    body(CURRENT, "bool CRagEffect::ProcessEffectFrameLegacy()")) is not None)
check("source projection precedes its state30 burst and returns shared STR completion",
      PE_HANDLER.index("CRenderer::ProjectVertex") < PE_HANDLER.index("m_stateCnt == 30")
      < PE_HANDLER.index("LaunchEffectPrim(this, PP_2DCIRCLE,")
      and "return CRagEffect::ProcessEZ2STR(this, 0.0, -80.0, 0.0) != 0;" in PE_HANDLER)
check("local state30 burst preserves sound-before-disc and twenty ordered rays",
      LOCAL_HANDLER.index("m_stateCnt == 30") < LOCAL_HANDLER.index("PlayWave(")
      < LOCAL_HANDLER.index("LaunchEffectPrim(PP_2DCIRCLE,")
      < LOCAL_HANDLER.index("for (int i = 0; i < 20; ++i)")
      < LOCAL_HANDLER.index("LaunchEffectPrim(PP_2DFLASH,")
      and "return ProcessEZ2STR(0.0f,-80.0f,0.0f) != 0;" in LOCAL_HANDLER)

for token in (
    '"effect\\\\EF_Signum.wav"', '"effect\\\\EF_Bash.wav"',
    '"effect\\\\alpha_down.tga"', '"EFFECT\\\\alpha_center.tga"',
    "prim->m_radius = 100.0f", "prim->m_pattern |= 1", "prim->m_fadeOutCnt = -1",
    "rand() % 360", "rand() % 60 + 10", "rand() % 25 + 15",
    "rand() % 60 + 30", "rand() % 35 + 95",
):
    check(f"local Signum retains source anchor: {token}", token in LOCAL_HANDLER)

for phrase in (
    "Signum before frame thirty propagates missing STR without burst sound or RNG",
    "Signum frame thirty emits twenty-one primitives with one hundred ordered draws despite reduced effects",
    "Signum plays exact Signum then Bash waves before ray RNG",
    "Signum both waves retain stored-origin horizontal listener deltas and source audio parameters",
    "Signum processes active STR after burst with original offset without changing parent origin or duration",
    "Signum rays preserve five-draw ranges drag duration alpha and disabled constructor fade and change gates",
    "Signum preserves exact texture case source order and no sprite resources",
    "Signum circle starts original thirteen-frame fade at twenty-seven",
):
    check(f"lifecycle fixture covers {phrase}", phrase in FIXTURE)

print("Signum PE/Ref source, effect40 route and local lifecycle mapping passed.")
