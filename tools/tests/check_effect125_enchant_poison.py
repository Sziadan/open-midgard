#!/usr/bin/env python3
"""Compare effects20/125 EnchantPoison PE/Ref helper, route and resources."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")


def function_body(source: str, signature: str) -> str:
    matches = list(re.finditer(signature, source))
    if not matches:
        raise AssertionError(f"missing {signature}")
    opening = source.find("{", matches[-1].end())
    depth = 0
    for offset in range(opening, len(source)):
        if source[offset] == "{":
            depth += 1
        elif source[offset] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:offset]
    raise AssertionError(f"unterminated {signature}")


def compact(text: str) -> str:
    return re.sub(r"\s+", "", text)


signature = r"void __thiscall CRagEffect::EnchantPoison\(CRagEffect \*this\)"
if compact(function_body(PE, signature)) != compact(function_body(REF, signature)):
    raise AssertionError("canonical PE and primary Ref EnchantPoison helper bodies differ")

pe_onprocess = function_body(PE, r"unsigned __int8 __thiscall CRagEffect::OnProcess\(CRagEffect \*this\)")
if not re.search(r"case\s+20:\s*case\s+125:\s*CRagEffect::EnchantPoison\(this\);\s*break;", pe_onprocess):
    raise AssertionError("canonical PE cases20/125 no longer share EnchantPoison caller")
for effect_id in (20, 125):
    if not re.search(rf"case\s+{effect_id}:\s*SpawnEnchantPoison\(\);\s*return true;", DISPATCH):
        raise AssertionError(f"local effect{effect_id} route changed")
if "void SpawnEnchantPoison();" not in HEADER:
    raise AssertionError("local EnchantPoison emitter declaration missing")

local_helper = function_body(LOCAL, r"void CRagEffect::SpawnEnchantPoison\(\)")
for token in ('"effect\\\\assasin_enchantpoison.wav"', '"effect\\\\EF_PoisonAttack.wav"',
              "if (m_stateCnt == 0)", "if (m_stateCnt % 5 == 0)",
              "LaunchEffectPrim(PP_3DPARTICLE, vector3d{})", "ConfigureEnchantPoisonParticle(prim, 30, 40, 50)"):
    if token not in local_helper:
        raise AssertionError(f"local EnchantPoison operation missing {token}")
particle = function_body(LOCAL, r"void ConfigureEnchantPoisonParticle\(CEffectPrim\* prim, int speedSpan, int sizeBase, int sizeSpan\)")
for token in ("rand() % 360", "rand() % 6 + 2", "rand() % speedSpan + 30", "rand() % sizeSpan + sizeBase",
              "particle3.spr", "particle3.act", "ShiftCircleSegments(prim)"):
    if token not in particle:
        raise AssertionError(f"local EnchantPoison shared particle profile missing {token}")
particle_family = function_body(LOCAL, r"bool CEffectPrim::UsesReferenceParticle\(\) const")
for effect_id in (20, 125):
    if not re.search(rf"m_master->GetEffectType\(\)\s*==\s*{effect_id}", particle_family):
        raise AssertionError(f"effect{effect_id} missing from the source-backed particle family")

image = ExeImage(ROOT / "HighPriest.exe")
for va, expected in ((0x71BC58, b"effect\\assasin_enchantpoison.wav"),
                     (0x71BC3C, b"effect\\EF_PoisonAttack.wav"),
                     (0x71BC24, b"\xC0\xCC\xC6\xD1\xC6\xAE\\particle3.spr"),
                     (0x71BC0C, b"\xC0\xCC\xC6\xD1\xC6\xAE\\particle3.act")):
    actual = image.read_cstring_at(va)
    if actual is None or actual.raw != expected:
        raise AssertionError(f"canonical EnchantPoison resource bytes differ at VA 0x{va:X}")

print("effects20/125 EnchantPoison parity passed: PE/Ref helper, grouped callers/routes, particle profile/family and four exact resource strings")
