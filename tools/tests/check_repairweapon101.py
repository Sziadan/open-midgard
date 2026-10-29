#!/usr/bin/env python3
"""Check Repair Weapon effect101 route and PE-backed cue."""
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

pe_helper = function_body(pe, r"bool __thiscall CRagEffect::RepairWeapon\(CRagEffect \*this\)")
ref_helper = function_body(ref, r"bool __thiscall CRagEffect::RepairWeapon\(CRagEffect \*this\)")
if compact(pe_helper) != compact(ref_helper):
    raise AssertionError("canonical PE and primary Ref RepairWeapon helper bodies differ")
pe_onprocess = function_body(pe, r"unsigned __int8 __thiscall CRagEffect::OnProcess\(CRagEffect \*this\)")
if not re.search(r"case\s+101:\s*result\s*=\s*CRagEffect::RepairWeapon\(this\);\s*goto LABEL_425;", pe_onprocess):
    raise AssertionError("canonical PE effect101 caller changed")
if re.search(r"bool __thiscall CRagEffect::OnProcess\(CRagEffect \*this\)", ref) or "CRagEffect::RepairWeapon(this)" in ref:
    raise AssertionError("primary Ref unexpectedly contains the omitted OnProcess caller")
if "case 101: return ProcessRepairWeapon101();" not in dispatch:
    raise AssertionError("effect101 is not routed to Repair Weapon")
if "CRagEffect::RepairWeapon" not in ref or "0x606480" not in ref.lower():
    raise AssertionError("Ref Repair Weapon helper/canonical PE anchor is missing")
cue = ExeImage().read_cstring_at(0x71BD2C)
if cue is None or cue.raw != b"effect\\black_weapon_repair_a.wav":
    raise AssertionError("Repair Weapon PE cue changed")
start = rag.find("bool CRagEffect::ProcessRepairWeapon101()")
end = rag.find("// TEST_BODY_END repairweapon101_handler", start)
if start < 0 or end < 0:
    raise AssertionError("Repair Weapon handler test boundary is missing")
body = rag[start:end]
for token in (
    "m_stateCnt == 20", "m_stateCnt == 55", "black_weapon_repair_a.wav",
    "ProcessEZ2STR(0.0f, -80.0f, 0.0f)",
):
    if token not in body:
        raise AssertionError(f"Repair Weapon handler is missing {token}")
print("effect101 source check passed: PE/Ref helper, PE caller, Ref caller omission, local route, state20/55 cue and exact bytes")
