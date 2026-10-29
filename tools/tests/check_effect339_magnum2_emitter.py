#!/usr/bin/env python3
"""Compare effect339 Magnum2 emitter helpers, caller, cues and resources."""
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


def effect_case(source):
    match = re.search(r"case\s+0x153\s*:[\s\S]*?return\s+1\s*;", source)
    if match is None:
        raise AssertionError("OnProcess2 effect339 case missing")
    return match.group(0)


signature = "void __thiscall CRagEffect::Magnum2(CRagEffect *this, char *tName)"
pe_helper = function_body(pe, signature)
ref_helper = function_body(ref, signature)
if compact(pe_helper) != compact(ref_helper):
    raise AssertionError("PE and primary-Ref Magnum2 emitter helpers differ")
pe_caller = effect_case(function_body(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess2(CRagEffect *this)"))
ref_caller = effect_case(function_body(primary, "unsigned __int8 __thiscall CRagEffect::OnProcess2(CRagEffect *this)"))
if compact(pe_caller) != compact(ref_caller):
    raise AssertionError("PE and primary-Ref effect339 caller branches differ")
if "CRagEffect::Magnum2(this, aEffectRingYell)" not in pe_caller:
    raise AssertionError("PE effect339 texture argument changed")
if r'case 339: ApplyMagnum2("effect\\ring_yellow.tga"); return true;' not in route:
    raise AssertionError("local effect339 texture/route mapping changed")

for state, symbol in ((20, "aPermeterAttack"), (30, "aEffectEfMagnum")):
    if f"m_stateCnt == {state}" not in pe_helper or f"PlayWave({symbol}" not in pe_helper:
        raise AssertionError(f"PE Magnum2 state{state} wave cue changed")
for token in (
    "if ( !m_stateCnt )", "v6 >= 25", "v6 <= 55", "!(v6 % 3)",
    "this->m_flag + 1", "v7 < 11", "PP_MAGNUM2", "m_size = 5.0", "m_totalTexture = 1",
    "v10 = v6 - 25", "20.0 - v12 * 0.69999999", "v12 * 0.2 + 5.0",
):
    if token not in pe_helper:
        raise AssertionError(f"PE Magnum2 emission profile missing {token}")

local_helper = function_body(local, "void CRagEffect::ApplyMagnum2(const char* textureName)")
for token in (
    r'playAtListener("permeter_attack.wav")', r'playAtListener("effect\\EF_MagnumBreak.wav")',
    "m_stateCnt == 0", "m_stateCnt < 25", "m_stateCnt > 55", "m_stateCnt % 3",
    "++m_flag", "m_flag >= 11", "kPrimTypeMagnum2", "prim->m_renderFlag = 5",
    "prim->m_duration = m_duration", "prim->m_size = 5.0f", "band < 2",
    "gi.rotStart = static_cast<short>(6 * age + 9 * band)",
    "20.0f - static_cast<float>(age) * 0.69999999f",
    "static_cast<float>(age) * 0.2f + 5.0f",
):
    if token not in local_helper:
        raise AssertionError(f"local Magnum2 emitter missing {token}")

image = ExeImage(ROOT / "HighPriest.exe")
for va, expected in (
    (0x71CC78, b"permeter_attack.wav"),
    (0x71B3CC, b"effect\\EF_MagnumBreak.wav"),
    (0x7193E8, b"effect\\ring_yellow.tga"),
):
    resource = image.read_cstring_at(va)
    if resource is None or resource.raw != expected:
        raise AssertionError(f"Magnum2 resource bytes differ at PE VA 0x{va:06X}")

print("effect339 Magnum2 parity passed: PE/Ref helper/caller, local emission profile, two waves and texture bytes")
