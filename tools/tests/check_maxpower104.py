#!/usr/bin/env python3
"""Check Max Power effect104 route, cue timing, and PE resource strings."""
from pathlib import Path
import sys
import re

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage  # noqa: E402

rag = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
pe = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
ref = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")

def function_body(source, signature):
    matches = list(re.finditer(signature, source))
    if not matches:
        raise AssertionError(f"missing {signature}")
    opening = source.find("{", matches[-1].end())
    depth = 0
    for offset in range(opening, len(source)):
        if source[offset] == "{": depth += 1
        elif source[offset] == "}":
            depth -= 1
            if depth == 0: return source[opening + 1:offset]
    raise AssertionError(f"unterminated {signature}")

def compact(text):
    return re.sub(r"\s+", "", text)

pe_helper = function_body(pe, r"bool __thiscall CRagEffect::MaxPower\(CRagEffect \*this\)")
ref_helper = function_body(ref, r"bool __thiscall CRagEffect::MaxPower\(CRagEffect \*this\)")
if compact(pe_helper) != compact(ref_helper):
    raise AssertionError("canonical PE and primary Ref MaxPower helper bodies differ")
pe_onprocess = function_body(pe, r"unsigned __int8 __thiscall CRagEffect::OnProcess\(CRagEffect \*this\)")
if not re.search(r"case\s+104:\s*result\s*=\s*CRagEffect::MaxPower\(this\);\s*goto LABEL_425;", pe_onprocess):
    raise AssertionError("canonical PE effect104 caller changed")
if re.search(r"bool __thiscall CRagEffect::OnProcess\(CRagEffect \*this\)", ref) or "CRagEffect::MaxPower(this)" in ref:
    raise AssertionError("primary Ref unexpectedly contains the omitted OnProcess caller")
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
print("effect104 source check passed: PE/Ref helper, PE caller, Ref caller omission, local route and four timed cues/resources")
