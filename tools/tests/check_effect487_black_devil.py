#!/usr/bin/env python3
"""Compare BLACKDEVIL emitter sources, local route/profile and texture bytes."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
PRIMARY = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="latin-1")
SECONDARY = (ROOT / "Ref/dhxj/RagEffect2.cpp").read_text(encoding="latin-1")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")


def body(source, signature):
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:pos]
    raise AssertionError(f"unterminated function: {signature}")


def compact(source):
    return re.sub(r"\s+", "", source)


pe_helper = body(PE, "void __thiscall CRagEffect::BLACKDEVIL(CRagEffect *this, char *tName)")
primary_helper = body(PRIMARY, "void __thiscall CRagEffect::BLACKDEVIL(CRagEffect *this, char *tName)")
secondary_helper = body(SECONDARY, "void CRagEffect::BLACKDEVIL(char *tName)")
if compact(pe_helper) != compact(primary_helper):
    raise AssertionError("PE and primary-Ref BLACKDEVIL helper bodies differ")
for token in (
    "m_stateCnt == 1", "m_stateCnt == 2", "PP_3DCASTING_5", "m_size=12",
    "RotStart = 270", "RotStart = 271", "distance = 2.5f", "distance = 2.7f",
    "rise_angle=angle-7", "E_DIVISION",
):
    if token not in secondary_helper:
        raise AssertionError(f"secondary-Ref BLACKDEVIL profile missing {token}")

if "case 487: ApplyBlackDevil(\"effect\\\\ring_black.tga\"); return true;" not in DISPATCH:
    raise AssertionError("local effect487 route or ring-black texture changed")
local_helper = body(LOCAL, "void CRagEffect::ApplyBlackDevil(const char* textureName)")
for token in (
    "m_stateCnt != 1 && m_stateCnt != 2", "kPrimTypeCasting5", "LaunchWarpCasting",
    "const float distance[4] = { 2.5f, 5.0f, 7.5f, 10.0f }",
    "const short spin[4] = { 270, 0, 90, 180 }", "const short rise[4] = { 53, 60, 55, 50 }",
    "const float distance[4] = { 2.7f, 5.1999998f, 7.6999998f, 10.2f }",
    "const short spin[4] = { 271, 1, 91, 181 }", "const short rise[4] = { 52, 59, 54, 49 }",
):
    if token not in local_helper:
        raise AssertionError(f"local BLACKDEVIL profile missing {token}")

image = ExeImage(ROOT / "HighPriest.exe")
texture = image.read_cstring_at(0x7198D0)
if texture is None or texture.raw != b"effect\\ring_black.tga":
    raise AssertionError("BLACKDEVIL texture bytes differ at PE VA 0x7198D0")

print("effect487 BLACKDEVIL parity passed: PE/Ref helpers, secondary/local profile, route and exact texture")
