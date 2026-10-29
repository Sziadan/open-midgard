#!/usr/bin/env python3
"""Compare effect300 Chemical Protection PE/Ref helpers, caller and resources."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage  # noqa: E402

pe = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
ref = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="latin-1")
primary = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
local = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
route = (ROOT / "src/world/RagEffectOnProcess2Cases.generated.inc").read_text(encoding="utf-8")


def function_body(source, signature):
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 0
    for position in range(opening, len(source)):
        if source[position] == "{":
            depth += 1
        elif source[position] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:position]
    raise AssertionError(f"unterminated function {signature}")


def compact(source):
    return re.sub(r"\s+", "", source)


helper_signature = "void __thiscall CRagEffect::CHEMICALPROTECTION(CRagEffect *this, __int16 time)"
pe_helper = function_body(pe, helper_signature)
ref_helper = function_body(ref, helper_signature)
if compact(pe_helper) != compact(ref_helper):
    raise AssertionError("PE and primary-Ref CHEMICALPROTECTION helper bodies differ")


def caller_case(source):
    match = re.search(r"case\s+0x12C\s*:[\s\S]*?return\s+1\s*;", source)
    if match is None:
        raise AssertionError("OnProcess2 effect300 caller missing")
    return match.group(0)


pe_caller = caller_case(function_body(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess2(CRagEffect *this)"))
ref_caller = caller_case(function_body(primary, "unsigned __int8 __thiscall CRagEffect::OnProcess2(CRagEffect *this)"))
if compact(pe_caller) != compact(ref_caller):
    raise AssertionError("PE and primary-Ref effect300 caller branches differ")
for token in ("PlayWave(aApocalipsAttac", "CHEMICALPROTECTION(this, 30)", "CHEMICALPROTECTION(this, 60)"):
    if token not in pe_caller:
        raise AssertionError(f"effect300 caller missing {token}")

if "case 300: return OnProcess2Legacy();" not in route:
    raise AssertionError("local effect300 OnProcess2 route missing")
local_onprocess2 = function_body(local, "bool CRagEffect::OnProcess2Legacy()")
local_case_match = re.search(r"case 300:\s*\{[\s\S]*?\n    \}", local_onprocess2)
if local_case_match is None:
    raise AssertionError("local effect300 branch missing")
local_case = local_case_match.group(0)
for token in (
    "m_stateCnt == 0", 'TryPlayEffectWaveAt(wavePos, "apocalips_attack.wav")',
    "ApplyChemicalProtection(30)", "ApplyChemicalProtection(60)", "m_cachedPos.y -= 9.0f",
):
    if token not in local_case:
        raise AssertionError(f"local effect300 branch missing {token}")

local_helper = function_body(local, "void CRagEffect::ApplyChemicalProtection(short time)")
for token in (
    "m_stateCnt != 0", "kPrimTypeChemical", "prim->m_renderFlag = 5", "prim->m_duration = m_duration",
    r'GetTexture("effect\\shockwave_c.bmp", false)', "band < 4", "gi.life = 1", "gi.alphaB = 0",
    "gi.distance = 100.0f", "time + 90 * band",
):
    if token not in local_helper:
        raise AssertionError(f"local Chemical Protection payload missing {token}")

image = ExeImage(ROOT / "HighPriest.exe")
for va, expected, label in (
    (0x7192C0, b"apocalips_attack.wav", "wave"),
    (0x71C9CC, b"effect\\shockwave_c.bmp", "texture"),
):
    resource = image.read_cstring_at(va)
    if resource is None or resource.raw != expected:
        raise AssertionError(f"Chemical Protection {label} bytes differ at PE VA 0x{va:06X}")

print("effect300 Chemical Protection parity passed: PE/Ref helper/caller, local route/payload and exact resources")
