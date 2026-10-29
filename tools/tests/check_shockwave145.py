#!/usr/bin/env python3
"""Check ShockWave effect145's looping STR termination semantics."""
from pathlib import Path
import sys
import re

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage  # noqa: E402
pe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
ref = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
rag = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
str_table = (ROOT / "src/world/RagEffectStrNameTable.generated.inc").read_text(encoding="utf-8")

def init_body(source):
    marker = "void __thiscall CRagEffect::Init(CRagEffect *this, CRenderObject *master, int effectId, vector3d fDeltaPos)"
    start = source.rindex(marker)
    opening = source.index("{", start)
    depth = 0
    for offset in range(opening, len(source)):
        if source[offset] == "{": depth += 1
        elif source[offset] == "}":
            depth -= 1
            if depth == 0: return source[opening + 1:offset]
    raise AssertionError("unterminated Init")

def init_case(source):
    match = re.search(r"case\s+145\s*:[\s\S]*?\bbreak\s*;", source)
    if not match: raise AssertionError("Init effect145 case missing")
    return re.sub(r"\s+", "", match.group(0))
if "case 145: return ProcessShockwave145();" not in dispatch:
    raise AssertionError("effect145 is not routed to its looping ShockWave handler")
if "case 145:" not in ref or "sprintf(StrName, aShockwaveStr);" not in ref:
    raise AssertionError("Ref effect145 no longer selects ShockWave.str")
pe_init = init_case(init_body(pe))
ref_init = init_case(init_body(ref))
if pe_init != ref_init or "sprintf(StrName,aShockwaveStr);break;" not in pe_init:
    raise AssertionError("PE and primary Ref Init effect145 mappings differ")
if '{ 145, "ShockWave.str", nullptr, nullptr, 0, 0 }' not in str_table:
    raise AssertionError("local STR catalog no longer maps effect145 to ShockWave.str")
resource = ExeImage(ROOT / "HighPriest.exe").read_cstring_at(0x71AE10)
if resource is None or resource.raw != b"ShockWave.str":
    raise AssertionError("ShockWave.str PE bytes differ at VA 0x71AE10")
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
print("effect145 source check passed: PE/Ref Init mapping, local catalog, exact resource bytes and looping STR reset")
