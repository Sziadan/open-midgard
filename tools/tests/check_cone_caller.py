"""Check PE/Ref Cone helper evidence, effect71 route, payload and family binding."""
from __future__ import annotations
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
pe = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
ref = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
local = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")


def fn(text: str, sig: str) -> str:
    start = text.rindex(sig)
    opening = text.index("{", start)
    depth = 0
    for i in range(opening, len(text)):
        if text[i] == "{": depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0: return text[start:i + 1]
    raise AssertionError(sig)


def compact(text: str) -> str:
    return re.sub(r"\s+", "", text)


signature = "void __thiscall CRagEffect::Cone(CRagEffect *this)"
if compact(fn(pe, signature)) != compact(fn(ref, signature)):
    raise SystemExit("Cone PE/primary Ref helper mismatch")
main = fn(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
case = main[main.index("case 71:"):main.index("case 72:")]
if "CRagEffect::Cone(this)" not in case:
    raise SystemExit("PE effect71 caller missing")
if "CRagEffect::Cone(this" in ref:
    raise SystemExit("Ref caller omission changed")
if "case 71: return ProcessEffectFrameLegacy();" not in dispatch:
    raise SystemExit("effect71 frame-dispatch route missing")
legacy = fn(local, "bool CRagEffect::ProcessEffectFrameLegacy()")
if "if (m_type == 71)" not in legacy or "ApplyCone();" not in legacy:
    raise SystemExit("effect71 local handler route missing")
handler = fn(local, "void CRagEffect::ApplyCone()")
for token in ("m_stateCnt % 130", "m_duration = 200", "PP_3DPARTICLEORBIT",
              "prim->m_duration = 300", "prim->m_radiusSpeed = 0.2f",
              "prim->m_gravSpeed = 0.1f", "prim->m_gravAccel = 0.005f",
              "prim->m_longitude = 2.0f", "prim->m_longSpeed = 15.0f",
              "prim->m_longAccel = -0.01f", "prim->m_size = 1.2f",
              '"\\xC0\\xCC\\xC6\\xD1\\xC6\\xAE\\\\particle1.spr"',
              '"\\xC0\\xCC\\xC6\\xD1\\xC6\\xAE\\\\particle1.act"',
              "prim->m_numSegments = 1", "ShiftCircleSegments(prim)"):
    if token not in handler:
        raise SystemExit(f"Cone payload missing {token}")
particle = fn(local, "bool CEffectPrim::UsesReferenceParticle() const")
if "m_master->GetEffectType() == 71" not in particle:
    raise SystemExit("effect71 missing from reference particle renderer family")
print("Cone71 parity: complete PE/Ref helper, caller/Ref omission, local route, orbit payload/assets and particle family passed")
