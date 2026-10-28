#!/usr/bin/env python3
"""Check Kyrie effect112 caller route, timed cues, and PE resource bytes."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage  # noqa: E402

rag = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
ref = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
if "case 112: return ProcessKyrie112();" not in dispatch:
    raise AssertionError("effect112 is not routed to Kyrie")
if "CRagEffect::Kyrie" not in ref or "0x6069d0" not in ref.lower():
    raise AssertionError("Ref Kyrie helper/canonical PE anchor is missing")
image = ExeImage()
for va, expected in (
    (0x71BDF0, b"effect\\priest_kyrie_eleison_b.wav"),
    (0x71BDCC, b"effect\\priest_kyrie_eleison_a.wav"),
):
    if image.read_cstring_at(va).raw != expected:
        raise AssertionError(f"Kyrie PE sound at {va:#x} changed")
start = rag.find("bool CRagEffect::ProcessKyrie112()")
end = rag.find("// TEST_BODY_END kyrie112_handler", start)
if start < 0 or end < 0:
    raise AssertionError("Kyrie handler test boundary is missing")
body = rag[start:end]
for token in (
    "m_stateCnt == 0", "priest_kyrie_eleison_b.wav", "m_stateCnt == 15",
    "priest_kyrie_eleison_a.wav", "ProcessEZ2STR(0.0f, -80.0f, 0.0f)",
):
    if token not in body:
        raise AssertionError(f"Kyrie handler is missing {token}")
print("effect112 source check passed: state-zero B cue, state-15 A cue, shared STR continuation")
