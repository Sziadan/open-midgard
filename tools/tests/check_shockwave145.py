#!/usr/bin/env python3
"""Check ShockWave effect145's looping STR termination semantics."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
pe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
ref = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
rag = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
if "case 145: return ProcessShockwave145();" not in dispatch:
    raise AssertionError("effect145 is not routed to its looping ShockWave handler")
if "case 145:" not in ref or "sprintf(StrName, aShockwaveStr);" not in ref:
    raise AssertionError("Ref effect145 no longer selects ShockWave.str")
if "decompiled from address: 0x606d20" not in ref.lower() or "crageffect::shockwave" not in ref.lower():
    raise AssertionError("primary Ref ShockWave helper anchor is missing")
onprocess = pe[pe.find("unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)\n{") :]
case_start = onprocess.find("case 145:")
case_end = onprocess.find("case 153:", case_start)
case = onprocess[case_start:case_end]
if case_start < 0 or case_end < 0 or "ProcessEZ2STR(this, 0.0, -80.0, 0.0)" not in case:
    raise AssertionError("PE effect145 STR processing is missing")
for token in ("m_iCurLayer = 0", "m_stateCnt = 0", "m_cEndLayer = cLayer", "m_aiCurAniKey"):
    if token not in case:
        raise AssertionError(f"PE ShockWave completion reset is missing {token}")
start = rag.find("bool CRagEffect::ProcessShockwave145()")
end = rag.find("// TEST_BODY_END shockwave145_handler", start)
if start < 0 or end < 0:
    raise AssertionError("local ShockWave handler test boundary is missing")
body = rag[start:end]
if "ProcessEZ2STR(0.0f,-80.0f,0.0f)" not in body or "InitEZ2STRFrame()" not in body or "return true" not in body:
    raise AssertionError("local ShockWave handler does not restart its STR clip after completion")
print("effect145 source check passed: completed STR resets clip cursors and remains alive")
