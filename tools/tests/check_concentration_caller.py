"""Check the orphaned Concentration helper and canonical inline 218/219 routes."""
from __future__ import annotations
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage, SymbolTable

pe = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
ref = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
local = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")


def fn(text: str, sig: str) -> str:
    start = text.rindex(sig)
    opening = text.index("{", start)
    depth = 0
    for i in range(opening, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[start:i + 1]
    raise AssertionError(sig)


def compact(text: str) -> str:
    return re.sub(r"\s+", "", text)


helper = "bool __thiscall CRagEffect::Concentration(CRagEffect *this)"
if compact(fn(pe, helper)) != compact(fn(ref, helper)):
    raise SystemExit("Concentration PE/primary Ref helper mismatch")
for source, label in ((pe, "PE"), (ref, "Ref")):
    if "CRagEffect::Concentration(this" in source:
        raise SystemExit(f"{label} unexpectedly calls orphaned Concentration helper")

main = fn(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
case218 = main[main.index("case 218:"):main.index("case 219:")]
case219 = main[main.index("case 219:"):main.index("case 220:")]
for case, state, speed, duration in ((case218, "!this->m_stateCnt", "2.0", "50"),
                                     (case219, "this->m_stateCnt == 30", "1.9", "30")):
    for token in ("this->m_pos = (*p_m_master)->m_pos", state,
                  "PlayWave(aEffectAcConcen", f"CRagEffect::PotionPillar(this, {speed}, {duration})"):
        if token not in case:
            raise SystemExit(f"inline potion concentration case missing {token}")

handler = fn(local, "bool CRagEffect::ProcessPotionConcentration()")
for token in ("m_effectPos = master->m_pos", "m_stateCnt == (awakening ? 30 : 0)",
              '"effect\\\\ac_concentration.wav"',
              "ApplyPotionPillar(awakening ? 1.9f : 2.0f, awakening ? 30 : 50)",
              "ProcessEZ2STR(0.0f, -80.0f, 0.0f)"):
    if token not in handler:
        raise SystemExit(f"local concentration route missing {token}")
if "case 218: return ProcessPotionConcentration();" not in dispatch or \
   "case 219: return ProcessPotionConcentration();" not in dispatch:
    raise SystemExit("effect218/219 local routes missing")
pillar = fn(local, "void CRagEffect::ApplyPotionPillar(float speed, int duration)")
for token in ("PP_3DCYLINDER", "prim->m_duration = duration", "prim->m_heightSpeed = speed",
              '"effect\\\\alpha_down.tga"'):
    if token not in pillar:
        raise SystemExit(f"PotionPillar cylinder payload missing {token}")
cylinder = fn(local, "bool CEffectPrim::UsesReferenceCylinder() const")
for effect_id in (218, 219):
    if f"case {effect_id}:" not in cylinder:
        raise SystemExit(f"effect{effect_id} missing from reference cylinder family")
wave = SymbolTable(ExeImage()).resolve_name("aEffectAcConcen")
if wave is None or wave.raw != b"effect\\ac_concentration.wav":
    raise SystemExit(f"canonical concentration wave bytes changed: {wave!r}")
print("Concentration parity: orphan helper identity, inline PE 218/219 behavior, local routes, cylinder payload/family and exact wave bytes passed")
