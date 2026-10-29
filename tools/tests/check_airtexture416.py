#!/usr/bin/env python3
"""Check effect416 AirTexture's PE/Ref helper, resource and local route."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage  # noqa: E402

pe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
ref = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
rag = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
if "case 416: return ProcessAirTexture416();" not in dispatch:
    raise AssertionError("effect416 is not routed to its dedicated AirTexture handler")
if ExeImage().read_cstring_at(0x71A2DC).raw != b"effect\\emp shock.tga":
    raise AssertionError("effect416 PE texture bytes changed")
def body(source: str, signature: str) -> str:
    start = source.find(signature)
    if start < 0:
        raise AssertionError(f"missing {signature}")
    opening = source.find("{", start + len(signature))
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{": depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0: return source[opening + 1:index]
    raise AssertionError(f"unterminated {signature}")

pe_case_start = pe.find("case 416:")
pe_case_end = pe.find("case 417:", pe_case_start)
pe_case = pe[pe_case_start:pe_case_end]
if pe_case_start < 0 or pe_case_end < 0 or "CRagEffect::AirTexture(this, aEffectEmpShock)" not in pe_case:
    raise AssertionError("PE OnProcess no longer contains effect416")
signature = "void __thiscall CRagEffect::AirTexture(CRagEffect *this, char *tName)"
ref_helper = body(ref, signature)
pe_helper = body(pe, "//----- (0060FE40)")
if "decompiled from address: 0x60fe40" not in ref.lower() or "PP_AIRTEXTURE" not in ref_helper:
    raise AssertionError("primary Ref AirTexture helper anchor/body is missing")
if "PP_AIRTEXTURE" not in pe_helper or "rand() % 360" not in pe_helper:
    raise AssertionError("canonical PE AirTexture helper behavior is missing")
if re.sub(r"\s+", "", pe_helper) != re.sub(r"\s+", "", ref_helper):
    raise AssertionError("PE and primary Ref AirTexture helper bodies diverged")
pe_renderer = body(pe, "//----- (00622530)")
local_renderer = body(rag, "void RenderAirTexture(const CEffectPrim& prim, const matrix& viewMatrix)")
if "vecB_now" not in pe_renderer or not all(name in local_renderer for name in ("gi.sparkX", "gi.sparkY", "gi.sparkZ")):
    raise AssertionError("AirTexture target-position field mapping differs from the PE renderer")
start = rag.find("bool CRagEffect::ProcessAirTexture416()")
end = rag.find("// TEST_BODY_END airtexture416_handler", start)
if start < 0 or end < 0:
    raise AssertionError("local AirTexture handler test boundary is missing")
body = rag[start:end]
for token in ("m_effectPos = master->m_pos", "PP_AIRTEXTURE", 'effect\\\\emp shock.tga',
              "gi.fullDisplayAngle = static_cast<short>(rand() % 360)", "gi.sparkX = m_deltaPos.x",
              "gi.riseAngle", "prim->m_gi[i].life = 0"):
    if token not in body:
        raise AssertionError(f"local AirTexture handler is missing {token}")
print("effect416 source check passed: PE/Ref identity, master follow, exact texture and one-shot AirTexture payload")
