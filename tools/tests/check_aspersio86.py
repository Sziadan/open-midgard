#!/usr/bin/env python3
"""Check effect 86 Aspersio against the reference implementation and PE strings."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage  # noqa: E402

rag = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
ref = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
if "case 86: return ProcessAspersio86();" not in dispatch:
    raise AssertionError("effect 86 is not routed to Aspersio")
if "CRagEffect::Aspersio" not in ref or "0x606010" not in ref:
    raise AssertionError("Ref Aspersio helper/canonical PE anchor is missing")
image = ExeImage()
if image.read_cstring_at(0x71BD10).raw != b"effect\\priest_aspersio.wav":
    raise AssertionError("Aspersio PE wave string changed")
for va, suffix in ((0x71B200, b"particle1.spr"), (0x71B1E8, b"particle1.act")):
    if not image.read_cstring_at(va).raw.endswith(suffix):
        raise AssertionError(f"Aspersio PE particle resource at {va:#x} changed")
start = rag.find("bool CRagEffect::ProcessAspersio86()")
end = rag.find("// TEST_BODY_END aspersio86_handler", start)
if start < 0 or end < 0:
    raise AssertionError("Aspersio handler test boundary is missing")
body = rag[start:end]
for token in (
    "m_stateCnt == 70", "effect\\\\priest_aspersio.wav", "(m_stateCnt % 5) == 0",
    "m_stateCnt > 60 && m_stateCnt < 140", "PP_3DPARTICLEORBIT", "rand() % 360",
    "m_duration = 40", "m_radius = 1.5f", "m_gravSpeed = 0.19f",
    "m_gravAccel = 0.0099999998f", "m_fadeOutCnt = 30", "m_numSegments = 1",
    "ProcessEZ2STR(0.0f, -80.0f, 0.0f)",
):
    if token not in body:
        raise AssertionError(f"Aspersio handler is missing {token}")
if "case PP_3DPARTICLEORBIT" not in rag[rag.find("bool CEffectPrim::UsesReferenceParticle()") :]:
    raise AssertionError("orbit primitive is not in the shared reference particle renderer family")
print("effect 86 source check passed: state-70 audio, 15 orbit pulses, one RNG draw per pulse")
