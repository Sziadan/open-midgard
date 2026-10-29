#!/usr/bin/env python3
"""Compare effects180/184 HitDark PE/Ref caller and local shared emitter route."""
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
        if source[offset] == "{": depth += 1
        elif source[offset] == "}":
            depth -= 1
            if depth == 0: return source[opening + 1:offset]
    raise AssertionError(f"unterminated {signature}")


def compact(text: str) -> str:
    return re.sub(r"\s+", "", text)


pe_helper = function_body(PE, r"void __thiscall CRagEffect::HitDark\(CRagEffect \*this\)")
ref_helper = function_body(REF, r"void __thiscall CRagEffect::HitDark\(CRagEffect \*this\)")
if compact(pe_helper) != compact(ref_helper):
    raise AssertionError("canonical PE and primary Ref HitDark helper bodies differ")
pe_onprocess = function_body(PE, r"unsigned __int8 __thiscall CRagEffect::OnProcess\(CRagEffect \*this\)")
if not re.search(r"case\s+180:\s*case\s+184:\s*CRagEffect::HitDark\(this\);\s*break;", pe_onprocess):
    raise AssertionError("canonical PE effects180/184 caller changed")
for effect_id in (180, 184):
    if not re.search(rf"case\s+{effect_id}:\s*return ProcessEffectFrameLegacy\(\);", DISPATCH):
        raise AssertionError(f"local effect{effect_id} dispatch route missing")
    if not re.search(rf"m_type\s*==\s*{effect_id}", function_body(LOCAL, r"bool CRagEffect::ProcessEffectFrameLegacy\(\)")):
        raise AssertionError(f"local effect{effect_id} legacy handler route missing")
if "void SpawnHitDark();" not in HEADER:
    raise AssertionError("local HitDark emitter declaration missing")
local_dark = function_body(LOCAL, r"void CRagEffect::SpawnHitDark\(\)")
if not re.search(r"SpawnHit\(true\);", local_dark):
    raise AssertionError("local HitDark no longer selects the shared dark hit emitter")
shared = function_body(LOCAL, r"void CRagEffect::SpawnHit\(bool dark\)")
ordered = ("if(m_stateCnt!=0)return;", "ResolveCurrentMaster()", "180.0-master->m_roty",
           "LaunchEffectPrim(PP_3DCYLINDER", 'GetTexture("effect\\\\ring_blue.tga",false)',
           "dark||!g_session.m_isMinEffect", "LaunchHitParticle(this,facing,false,dark,false)",
           "LaunchHitParticle(this,facing,true,dark,false)")
positions = [compact(shared).find(compact(token)) for token in ordered]
if any(pos < 0 for pos in positions) or positions != sorted(positions):
    raise AssertionError("shared local hit emitter ordering, unconditional dark gate, or profiles changed")
particle = function_body(LOCAL, r"void LaunchHitParticle\(CRagEffect\* effect")
for token in ("if(dark)prim->m_tintColor=RGB(20,20,20)", "PP_3DPARTICLEGRAVITY", "PP_3DPARTICLE",
              "particle1.spr", "particle1.act", "m_numSegments=3", "InitReferenceSegments(prim)"):
    if compact(token) not in compact(particle):
        raise AssertionError(f"local dark hit particle profile missing {token}")
cylinder = function_body(LOCAL, r"bool CEffectPrim::UsesReferenceCylinder\(\) const")
if not re.search(r"case 180: case 184:", cylinder):
    raise AssertionError("effects180/184 missing from reference cylinder selector")
particle_family = function_body(LOCAL, r"bool CEffectPrim::UsesReferenceParticle\(\) const")
for effect_id in (180, 184):
    if not re.search(rf"GetEffectType\(\)\s*==\s*{effect_id}", particle_family):
        raise AssertionError(f"effect{effect_id} missing from reference particle selector")
image = ExeImage(ROOT / "HighPriest.exe")
for va, expected in ((0x71943C, b"effect\\ring_blue.tga"),
                     (0x71B200, bytes.fromhex("c0ccc6d1c6ae5c7061727469636c65312e737072")),
                     (0x71B1E8, bytes.fromhex("c0ccc6d1c6ae5c7061727469636c65312e616374"))):
    actual = image.read_cstring_at(va)
    if actual is None or actual.raw != expected:
        raise AssertionError(f"canonical HitDark resource bytes differ at VA 0x{va:X}")
print("effects180/184 HitDark parity passed: PE/Ref helper, grouped caller, local shared route/profiles and exact assets")
