#!/usr/bin/env python3
"""Compare canonical PE/Ref ThrowItem setup used by effects298 and299."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage  # noqa: E402

pe = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
ref = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="latin-1")
local = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")


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


signature = "void __thiscall CRagEffect::ThrowItem(CRagEffect *this, char *tName, char F1)"
pe_helper = function_body(pe, signature)
ref_helper = function_body(ref, signature)
if compact(pe_helper) != compact(ref_helper):
    raise AssertionError("PE and primary-Ref ThrowItem helpers differ")
if "rand(" in pe_helper or "random(" in pe_helper:
    raise AssertionError("canonical ThrowItem helper unexpectedly uses RNG")

onprocess = function_body(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
for effect_id, texture_va, variant in ((298, 0x71A2BC, 0), (299, 0x71A298, 1)):
    case = re.search(rf"case\s+{effect_id}\s*:[\s\S]*?break\s*;", onprocess)
    if case is None or f"CRagEffect::ThrowItem(this, &byte_{texture_va:06X}, {variant})" not in case.group(0):
        raise AssertionError(f"PE effect{effect_id} ThrowItem caller or argument mapping changed")
    if f"case {effect_id}: return ProcessThrowItemEffect();" not in dispatch:
        raise AssertionError(f"effect{effect_id} local dispatch route missing")

image = ExeImage(ROOT / "HighPriest.exe")
expected_resources = {
    0x71A2BC: b"\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\item\\\xBA\xF3\xBA\xB4.bmp",
    0x71A298: b"\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\item\\\xBA\xF3\xC6\xF7\xBC\xC7\xBA\xB4.bmp",
}
for va, expected in expected_resources.items():
    resource = image.read_cstring_at(va)
    if resource is None or resource.raw != expected:
        raise AssertionError(f"ThrowItem texture bytes differ at PE VA 0x{va:06X}")

emitter = function_body(local, "void CRagEffect::ApplyThrowItem(const char* textureName, int variant)")
for token in (
    "prim->m_renderFlag = 5", "prim->m_duration = m_duration", "prim->m_size = 0.0f",
    "prim->m_deltaPos = m_deltaPos", "gi.maxHeight = std::sqrt(dx * dx + dz * dz)",
    "gi.life = 1", "gi.process = 0", "gi.alphaB = 255", "gi.arcBaselineY = origin.y - 8.0f",
    "gi.height[8] = 0.0f", "gi.flag[0] = 0", "gi.flag[1] = static_cast<u8>(steps)",
):
    if token not in emitter:
        raise AssertionError(f"local ThrowItem payload missing {token}")
local_dispatch = function_body(local, "bool CRagEffect::ProcessThrowItemEffect()")
for effect_id, expected_texture, variant in (
    (298, r"\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\item\\\xBA\xF3\xBA\xB4.bmp", 0),
    (299, r"\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\item\\\xBA\xF3\xC6\xF7\xBC\xC7\xBA\xB4.bmp", 1),
):
    match = re.search(rf"case {effect_id}:[\s\S]*?return true;", local_dispatch)
    if match is None or f"ApplyThrowItem(\"{expected_texture}\", {variant})" not in match.group(0):
        raise AssertionError(f"local effect{effect_id} ThrowItem texture/variant mapping changed")

print("effects298/299 ThrowItem parity passed: PE/Ref helper, callers, local route/payload and exact executable textures")
