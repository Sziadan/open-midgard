#!/usr/bin/env python3
"""Check Max Power effect104 route, cue timing, and PE resource strings."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage  # noqa: E402

rag = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
ref = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
if "case 104: return ProcessMaxPower104();" not in dispatch:
    raise AssertionError("effect104 is not routed to Max Power")
if "CRagEffect::MaxPower" not in ref or "0x606640" not in ref.lower():
    raise AssertionError("Ref Max Power helper/canonical PE anchor is missing")
image = ExeImage()
for va, expected in (
    (0x71BDA4, b"effect\\black_maximize_power_circle.wav"),
    (0x71BD7C, b"effect\\black_maximize_power_sword.wav"),
    (0x71BD50, b"effect\\black_maximize_power_sword_bic.wav"),
):
    if image.read_cstring_at(va).raw != expected:
        raise AssertionError(f"Max Power PE cue at {va:#x} changed")
start = rag.find("bool CRagEffect::ProcessMaxPower104()")
end = rag.find("// TEST_BODY_END maxpower104_handler", start)
if start < 0 or end < 0:
    raise AssertionError("Max Power handler test boundary is missing")
body = rag[start:end]
for token in (
    "m_stateCnt == 0", "m_stateCnt == 40 || m_stateCnt == 47", "m_stateCnt == 65",
    "black_maximize_power_circle.wav", "black_maximize_power_sword.wav",
    "black_maximize_power_sword_bic.wav", "ProcessEZ2STR(0.0f,-80.0f,0.0f)",
):
    if token not in body:
        raise AssertionError(f"Max Power handler is missing {token}")
print("effect104 source check passed: four timed cues and PE-backed wave bytes")
