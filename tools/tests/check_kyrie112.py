#!/usr/bin/env python3
"""Check Kyrie effect112 caller route, timed cues, and PE resource bytes."""
from pathlib import Path
import sys
import re

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage  # noqa: E402

rag = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
ref = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
pe = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")

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

pe_helper = function_body(pe, r"bool __thiscall CRagEffect::Kyrie\(CRagEffect \*this\)")
ref_helper = function_body(ref, r"bool __thiscall CRagEffect::Kyrie\(CRagEffect \*this\)")
if compact(pe_helper) != compact(ref_helper):
    raise AssertionError("canonical PE and primary Ref Kyrie helper bodies differ")
pe_onprocess = function_body(pe, r"unsigned __int8 __thiscall CRagEffect::OnProcess\(CRagEffect \*this\)")
if not re.search(r"case\s+112:\s*result\s*=\s*CRagEffect::Kyrie\(this\);\s*goto LABEL_425;", pe_onprocess):
    raise AssertionError("canonical PE effect112 caller changed")
if re.search(r"bool __thiscall CRagEffect::OnProcess\(CRagEffect \*this\)", ref):
    raise AssertionError("primary Ref unexpectedly contains the omitted OnProcess caller body")
if "CRagEffect::Kyrie(this)" in ref:
    raise AssertionError("primary Ref unexpectedly routes a dispatcher caller through Kyrie")
if "case 112: return ProcessKyrie112();" not in dispatch:
    raise AssertionError("effect112 is not routed to Kyrie")
if "CRagEffect::Kyrie" not in ref or "0x6069d0" not in ref.lower():
    raise AssertionError("Ref Kyrie helper/canonical PE anchor is missing")
image = ExeImage()
for va, expected in (
    (0x71BDF0, b"effect\\priest_kyrie_eleison_b.wav"),
    (0x71BDCC, b"effect\\priest_kyrie_eleison_a.wav"),
):
    actual = image.read_cstring_at(va)
    if actual is None or actual.raw != expected:
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
print("effect112 source check passed: PE/Ref helper, PE caller, Ref caller omission, local route, timed cues and exact resources")
