#!/usr/bin/env python3
"""Check Storm Gust effect89 route, helper behavior, and PE resource strings."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage  # noqa: E402

rag = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
ref = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
if "case 89: return ProcessStormGust89();" not in dispatch:
    raise AssertionError("effect89 is not routed to Storm Gust")
if "CRagEffect::StormGust" not in ref or "0x605a40" not in ref.lower():
    raise AssertionError("Ref Storm Gust helper/canonical PE anchor is missing")
image = ExeImage()
for va, expected in ((0x71BCD8, b"effect\\wizard_stormgust.wav"), (0x71B5FC, b"effect\\ice.tga")):
    if image.read_cstring_at(va).raw != expected:
        raise AssertionError(f"Storm Gust PE resource at {va:#x} changed")
start = rag.find("bool CRagEffect::ProcessStormGust89()")
end = rag.find("// TEST_BODY_END stormgust89_handler", start)
if start < 0 or end < 0:
    raise AssertionError("Storm Gust handler test boundary is missing")
body = rag[start:end]
for token in (
    "m_stateCnt == 0", "effect\\\\wizard_stormgust.wav", "g_session.m_isMinEffect",
    "(m_stateCnt % 29) == 0", "m_stateCnt > 30 && m_stateCnt <= 150",
    "i<2", "PP_3DQUADHORN", "rand()%360", "rand()%25+3", "rand()%100+400",
    "215-m_stateCnt", "m_pattern|=1", "m_heightSize=20.0f", "m_latitude=100.0f",
    "m_fadeOutCnt=prim->m_duration-10", "effect\\\\ice.tga",
    "ProcessEZ2STR(0.0f,-80.0f,0.0f)",
):
    if token not in body:
        raise AssertionError(f"Storm Gust handler is missing {token}")
quad = rag[rag.find("bool CEffectPrim::UsesReferenceQuadHorn()") :]
if "GetEffectType() == 89" not in quad:
    raise AssertionError("Storm Gust is missing from the reference quad-horn renderer family")
print("effect89 source check passed: minimum-effects gate, four double-particle pulses, wave and ice resources")
