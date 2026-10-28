"""Compare Exit helper and verify effect7 caller, payload, RNG, and renderer routes."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def function(source: str, signature: str) -> str:
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{": depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0: return source[start:pos + 1]
    raise AssertionError(signature)


def compact(text: str) -> str:
    return re.sub(r"\s+", "", text)


pe = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
ref = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
prim_ref = (ROOT / "Ref/RagEffectPrim.cpp").read_text(encoding="latin-1")
local = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")

signature = "void __thiscall CRagEffect::Exit(CRagEffect *this)"
if compact(function(pe, signature)) != compact(function(ref, signature)):
    raise SystemExit("Ref/PE Exit helper mismatch")
main = function(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
case = main[main.index("case 7:"):main.index("case 8:")]
if "Exit(this)" not in case:
    raise SystemExit("canonical effect7 caller changed")
if "Exit(this)" in ref:
    raise SystemExit("Ref caller omission changed; review PE-only caller evidence")
if "void __thiscall CEffectPrim::InitSegment(CEffectPrim *this)" not in prim_ref:
    raise SystemExit("reference InitSegment implementation not found")
if "bool ProcessExit7();" not in header or "case 7: return ProcessExit7();" not in dispatch:
    raise SystemExit("effect7 route/declaration missing")

handler = function(local, "bool CRagEffect::ProcessExit7()")
for token in ("m_effectPos = master->m_pos", "m_stateCnt == 0", "PP_3DCYLINDER", "m_duration = 100",
              "m_outerSize = 4.5f", "m_innerSize = 4.5f", "m_heightSize = 35.0f",
              '"effect\\\\alpha_down.tga"', '"_heal_effect.wav"', "m_stateCnt % 9",
              "g_session.m_isMinEffect", "PP_3DPARTICLEORBIT", "(void)rand()", "rand() % 360",
              "m_gravSpeed = -1.2f", "m_size = 0.55000001f", "SetParticleResources(",
              '"\\xC0\\xCC\\xC6\\xD1\\xC6\\xAE\\\\particle1.spr"',
              '"\\xC0\\xCC\\xC6\\xD1\\xC6\\xAE\\\\particle1.act"',
              "m_numSegments = 1", "InitReferenceSegments(orbit)"):
    if token not in handler:
        raise SystemExit(f"effect7 payload missing {token}")
if handler.count("rand()") != 2:
    raise SystemExit("effect7 must preserve one discarded RNG draw followed by one longitude draw")
families = function(local, "bool CEffectPrim::UsesReferenceCylinder() const") + function(local, "bool CEffectPrim::UsesReferenceParticle() const")
if not re.search(r"case\s+7\s*:(?:\s*case\s+\d+\s*:)*\s*return\s+true;", families) or "m_master->GetEffectType() == 7" not in families:
    raise SystemExit("effect7 cylinder/orbit reference render family missing")
print("Exit7 parity: Ref/PE helper, caller, route, cylinder, sound, orbit RNG/resources, and renderer families passed")
