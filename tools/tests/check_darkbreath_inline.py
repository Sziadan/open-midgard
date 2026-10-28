"""Verify DarkBreath's orphan helper and canonical inline effect212 emitter."""
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


signature = "void __thiscall CRagEffect::DarkBreath(CRagEffect *this)"
if re.sub(r"\s+", "", fn(pe, signature)) != re.sub(r"\s+", "", fn(ref, signature)):
    raise SystemExit("DarkBreath PE/primary Ref helper mismatch")
for source, label in ((pe, "PE"), (ref, "Ref")):
    if "CRagEffect::DarkBreath(this" in source:
        raise SystemExit(f"{label} unexpectedly invokes orphaned DarkBreath helper")
main = fn(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
starts = [match.start() for match in re.finditer(r"case 212:", main)]
start = next((offset for offset in starts if "PP_3DPARTICLE" in main[offset:offset + 1300]), None)
if start is None:
    raise SystemExit("inline PE effect212 particle setup missing")
case = main[start:start + 1300]
for token in ("!this->m_stateCnt", "LaunchEffectPrim(this, PP_3DPARTICLE",
              "m_duration = 65", "BYTE1(m_renderFlag) |= 2u",
              "m_deltaPos2.y = g_prim->m_deltaPos2.y - 20.0", "m_size = 0.80000001",
              "m_animSpeed = 1", "m_green = 0", "m_blue = 0", "m_fadeOutCnt = 60",
              "SetSprName(g_prim, &byte_71A250)", "SetActName(g_prim, &byte_71A238)",
              "CEffectPrim::InitSegment(g_prim)"):
    if token not in case:
        raise SystemExit(f"inline effect212 payload missing {token}")
if "case 212: SpawnDarkBreath(); return true;" not in dispatch:
    raise SystemExit("local effect212 route missing")
handler = fn(local, "void CRagEffect::SpawnDarkBreath()")
for token in ("m_stateCnt != 0", "PP_3DPARTICLE", "prim->m_duration = 65",
              "prim->m_renderFlag |= 0x200", "prim->m_deltaPos2.y -= 20.0f",
              "prim->m_size = static_cast<float>(80) / 100.0f", "prim->m_animSpeed = 1",
              "prim->m_green = 0", "prim->m_blue = 0", "prim->m_fadeOutCnt = 60",
              "DarkBreath.spr", "DarkBreath.act", "InitReferenceSegments(prim)"):
    if token not in handler:
        raise SystemExit(f"local DarkBreath payload missing {token}")
family = fn(local, "bool CEffectPrim::UsesReferenceParticle() const")
if "GetEffectType() == 212" not in family:
    raise SystemExit("effect212 missing from reference particle family")
print("DarkBreath212 parity: orphan helper, inline PE behavior, local route/payload and reference particle family passed")
