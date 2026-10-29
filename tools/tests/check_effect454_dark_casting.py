#!/usr/bin/env python3
"""Compare effect454 DarkCasting PE/Ref helpers, local route and resources."""
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


pe_helper = body(PE, "void __thiscall CRagEffect::DarkCasting(CRagEffect *this)")
primary_helper = body(PRIMARY, "void __thiscall CRagEffect::DarkCasting(CRagEffect *this)")
secondary_helper = body(SECONDARY, "void CRagEffect::DarkCasting()")
if compact(pe_helper) != compact(primary_helper):
    raise AssertionError("PE and primary-Ref DarkCasting helpers differ")
for token in (
    "m_pos = m_master->m_pos", "m_stateCnt == 0", 'PlayWave("effect\\\\EF_BeginSpell.wav"',
    "SAINTCASTING(45,\"effect\\\\ring_black.tga\",6)",
    "SAINTCASTING(25,\"effect\\\\ring_black.tga\",6)",
):
    if token not in secondary_helper:
        raise AssertionError(f"secondary Ref DarkCasting helper missing {token}")

if "case 454:" not in DISPATCH or 'ApplyPairedSaintCasting("effect\\\\ring_black.tga", 6, true, 0.0f)' not in DISPATCH:
    raise AssertionError("local effect454 route/variant/texture differs")
local_helper = body(LOCAL, "void CRagEffect::ApplyPairedSaintCasting(const char* textureName, int variant, bool playWave, float waveYOffset)")
for token in (
    "playWave && m_stateCnt == 0", 'TryPlayEffectWaveAt(wavePos, "effect\\\\EF_BeginSpell.wav")',
    "ApplySaintCasting(45, textureName, variant)", "ApplySaintCasting(25, textureName, variant)",
):
    if token not in local_helper:
        raise AssertionError(f"local shared casting helper missing {token}")

image = ExeImage(ROOT / "HighPriest.exe")
for va, expected, label in (
    (0x7193CC, b"effect\\EF_BeginSpell.wav", "wave"),
    (0x7198D0, b"effect\\ring_black.tga", "texture"),
):
    resource = image.read_cstring_at(va)
    if resource is None or resource.raw != expected:
        raise AssertionError(f"DarkCasting {label} bytes differ at PE VA 0x{va:06X}")

print("effect454 DarkCasting parity passed: PE/Ref helpers, local route/order and exact resources")
