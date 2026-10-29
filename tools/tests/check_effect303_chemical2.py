#!/usr/bin/env python3
"""Compare effect303 Chemical2 PE/Ref helper, caller, local behavior and resource."""
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


helper_signature = "void __thiscall CRagEffect::CHEMICAL2(CRagEffect *this, __int16 time)"
pe_helper = function_body(pe, helper_signature)
ref_helper = function_body(ref, helper_signature)
if compact(pe_helper) != compact(ref_helper):
    raise AssertionError("PE and primary-Ref CHEMICAL2 helper bodies differ")


def caller_case(source):
    match = re.search(r"case\s+0x12F\s*:[\s\S]*?return\s+1\s*;", source)
    if match is None:
        raise AssertionError("OnProcess2 effect303 caller missing")
    return match.group(0)


pe_caller = caller_case(function_body(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess2(CRagEffect *this)"))
ref_caller = caller_case(function_body(primary, "unsigned __int8 __thiscall CRagEffect::OnProcess2(CRagEffect *this)"))
if compact(pe_caller) != compact(ref_caller):
    raise AssertionError("PE and primary-Ref effect303 caller branches differ")
for token in (
    "CHEMICAL2(this, 0)", "CHEMICAL2(this, 1)", "this->m_stateCnt == 44",
    "GameMode->CMode::SendMsg(GameMode, 135, 0, 0, 0)",
    "PlayWave(aEffect_5, xi, 0.0, zi, 250, 40, 1.0)",
    "m_stateCnt >= 20 && m_stateCnt <= 120", "m_master->m_BodyLight += 8",
    "SetArgb(this->m_master, -1, 255, 255, 150)",
):
    if token not in pe_caller:
        raise AssertionError(f"effect303 caller missing {token}")

if "case 303: return OnProcess2Legacy();" not in route:
    raise AssertionError("local effect303 OnProcess2 route missing")
local_onprocess2 = function_body(local, "bool CRagEffect::OnProcess2Legacy()")
local_case_match = re.search(r"case 303:\s*\{[\s\S]*?\n    \}", local_onprocess2)
if local_case_match is None:
    raise AssertionError("local effect303 branch missing")
local_case = local_case_match.group(0)
for token in (
    "ApplyChemical2(0)", "ApplyChemical2(1)", "m_stateCnt == 44", "SendMsg(135, 0, 0, 0)",
    'TryPlayEffectWaveAt(wavePos, "effect\\\\\\xBF\\xAC\\xC1\\xD6\\xBA\\xD8\\xB0\\xDD.wav")',
    "m_stateCnt >= 20 && m_stateCnt <= 120", "master->m_BodyLight += 8",
    "master->SetArgb(-1, 255, 255, 150)",
):
    if token not in local_case:
        raise AssertionError(f"local effect303 branch missing {token}")

local_helper = function_body(local, "void CRagEffect::ApplyChemical2(short time)")
for token in (
    "m_stateCnt != 40", "kPrimTypeChemical2", "prim->m_renderFlag |= 0x200u",
    "prim->m_duration = m_duration", r'GetTexture("effect\\slash01.tga", false)',
    "m_effectPos.y -= 9.0f", "m_cachedPos.y -= 9.0f", "band < 4", "gi.life = 1",
    "gi.distance = 100.0f", "gi.alphaB = 0",
):
    if token not in local_helper:
        raise AssertionError(f"local CHEMICAL2 payload missing {token}")

image = ExeImage(ROOT / "HighPriest.exe")
texture = image.read_cstring_at(0x71C9E4)
if texture is None or texture.raw != b"effect\\slash01.tga":
    raise AssertionError("CHEMICAL2 slash texture bytes differ at PE VA 0x71C9E4")
wave_symbol = image.read_cstring_at(0x6F1FF0)
if wave_symbol is None or wave_symbol.raw != b"effect\\":
    raise AssertionError("CHEMICAL2 wave symbol's recoverable PE prefix differs")

print("effect303 CHEMICAL2 parity passed: PE/Ref helper/caller, local route/payload and recoverable resource bytes")
