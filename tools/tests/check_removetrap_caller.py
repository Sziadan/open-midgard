"""Compare RemoveTrap helper and verify effect100 cadence, RNG, resources, and renderer route."""
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

signature = "void __thiscall CRagEffect::RemoveTrap(CRagEffect *this)"
if compact(function(pe, signature)) != compact(function(ref, signature)):
    raise SystemExit("Ref/PE RemoveTrap helper mismatch")
main = function(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
case = main[main.index("case 100:"):main.index("case 101:")]
if "RemoveTrap(this)" not in case: raise SystemExit("canonical effect100 caller changed")
if "RemoveTrap(this" in ref: raise SystemExit("Ref caller omission changed; review PE-only caller evidence")
if "bool ProcessRemoveTrap100();" not in header or "case 100: return ProcessRemoveTrap100();" not in dispatch:
    raise SystemExit("effect100 route/declaration missing")

handler = function(local, "bool CRagEffect::ProcessRemoveTrap100()")
for token in ('"effect\\\\hunter_removetrap.wav"', "m_stateCnt == 0", "m_stateCnt != 0 && m_stateCnt != 7",
              "index < 12", "PP_3DPARTICLEORBIT", "m_duration = 50", "m_radiusSpeed = 0.2f",
              "(void)rand()", "m_gravSpeed = -2.0f", "rand() % 10", "rand() % 360", "m_size = 0.80000001f",
              "m_deltaPos2 = {", "m_fadeOutCnt = 35", '"\\xC0\\xCC\\xC6\\xD1\\xC6\\xAE\\\\particle1.spr"',
              '"\\xC0\\xCC\\xC6\\xD1\\xC6\\xAE\\\\particle1.act"', "m_numSegments = 1",
              "InitReferenceSegments(prim)"):
    if token not in handler: raise SystemExit(f"RemoveTrap payload missing {token}")
if handler.count("rand()") != 3:
    raise SystemExit("RemoveTrap must consume three RNG draws per particle")
family = function(local, "bool CEffectPrim::UsesReferenceParticle() const")
if "m_master->GetEffectType() == 100" not in family:
    raise SystemExit("effect100 reference orbit-particle render family missing")
image = resolver.ExeImage()
assets = [image.read_cstring_at(address).raw for address in (0x71B200, 0x71B1E8)]
if assets != [b"\xc0\xcc\xc6\xd1\xc6\xae\\particle1.spr", b"\xc0\xcc\xc6\xd1\xc6\xae\\particle1.act"]:
    raise SystemExit(f"RemoveTrap particle1 resource bytes changed: {assets!r}")
print("RemoveTrap100 parity: Ref/PE helper, caller, route, two 12-particle bursts, 3-draw RNG, assets and orbit renderer passed")
