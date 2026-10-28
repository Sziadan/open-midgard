"""Compare Detoxication helper and verify effect21 cadence, RNG, resources and renderer route."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import ro_exe_resolve as resolver


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
local = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")

signature = "void __thiscall CRagEffect::Detoxication(CRagEffect *this)"
if compact(function(pe, signature)) != compact(function(ref, signature)):
    raise SystemExit("Ref/PE Detoxication helper mismatch")
main = function(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
case = main[main.index("case 21:"):main.index("case 22:")]
if "Detoxication(this)" not in case:
    raise SystemExit("canonical effect21 caller changed")
if "Detoxication(this" in ref:
    raise SystemExit("Ref caller omission changed; review PE-only caller evidence")
if "bool ProcessDetoxication21();" not in header or "case 21: return ProcessDetoxication21();" not in dispatch:
    raise SystemExit("effect21 route/declaration missing")

handler = function(local, "bool CRagEffect::ProcessDetoxication21()")
ordered = ["g_session.m_isMinEffect ? 15 : 5", "m_stateCnt % interval", '"effect\\\\EF_Detoxication.wav"',
           "PP_3DPARTICLE", "rand() % 360", "rand() % 6", "MatrixAppendXRotation", "rand() % 30",
           "m_accel =", "rand() % 50", "m_rollSpeed = 3.0f", "m_alphaSpeed =",
           '"\\xC0\\xCC\\xC6\\xD1\\xC6\\xAE\\\\particle2.spr"',
           '"\\xC0\\xCC\\xC6\\xD1\\xC6\\xAE\\\\particle2.act"', "m_numSegments = 1",
           "InitReferenceSegments(prim)"]
positions = [handler.index(token) for token in ordered]
if positions != sorted(positions):
    raise SystemExit("Detoxication cadence, sound, payload, RNG order, resources or segment setup changed")
if handler.count("rand()") != 4:
    raise SystemExit("Detoxication must preserve four ordered random draws per emitted primitive")
family = function(local, "bool CEffectPrim::UsesReferenceParticle() const")
if "m_master->GetEffectType() == 21" not in family:
    raise SystemExit("effect21 particle sprite renderer family route missing")
image = resolver.ExeImage()
assets = [image.read_cstring_at(address).raw for address in (0x71B334, 0x71B31C)]
if assets != [b"\xc0\xcc\xc6\xd1\xc6\xae\\particle2.spr", b"\xc0\xcc\xc6\xd1\xc6\xae\\particle2.act"]:
    raise SystemExit(f"Detoxication particle2 resource bytes changed: {assets!r}")
print("Detoxication21 parity: Ref/PE helper, caller, route, intervals, wave, four-draw RNG, assets and renderer family passed")
