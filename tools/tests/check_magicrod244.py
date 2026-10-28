#!/usr/bin/env python3
"""Check effect244's ignored STR result and Magic Rod state-zero cue."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage  # noqa: E402

pe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
secondary = (ROOT / "Ref/dhxj/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
rag = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
if "case 244: return ProcessMagicRod244();" not in dispatch:
    raise AssertionError("effect244 is not routed to Magic Rod")
onprocess = pe[pe.find("unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)\n{") :]
pe_case = onprocess[onprocess.find("case 244:") :]
if "ProcessEZ2STR(this, 0.0, -80.0, 0.0);" not in pe_case[:250] or "CRagEffect::MagicRod(this);" not in pe_case[:250]:
    raise AssertionError("PE effect244 no longer ignores STR completion before Magic Rod")
if "MagicRod" not in secondary:
    raise AssertionError("secondary Ref Magic Rod evidence is missing")
if ExeImage().read_cstring_at(0x71CC5C).raw != b"effect\\sage_magic rod.wav":
    raise AssertionError("Magic Rod PE wave string changed")
start = rag.find("bool CRagEffect::ProcessMagicRod244()")
end = rag.find("// TEST_BODY_END magicrod244_handler", start)
if start < 0 or end < 0:
    raise AssertionError("Magic Rod handler boundary is missing")
body = rag[start:end]
if "ProcessEZ2STR(0.0f,-80.0f,0.0f);" not in body or "return true" not in body:
    raise AssertionError("Magic Rod does not ignore STR completion and remain alive")
if "m_stateCnt == 0" not in body or "sage_magic rod.wav" not in body or "m_effectPos=master->m_pos" not in body:
    raise AssertionError("Magic Rod master follow or state-zero cue is missing")
print("effect244 source check passed: ignored STR completion, master-follow cue, PE wave bytes")
