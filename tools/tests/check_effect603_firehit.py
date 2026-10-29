#!/usr/bin/env python3
"""Compare effect603 FireHit STR-only behavior with the PE and primary Ref."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
DISPATCH = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")
CATALOG = (ROOT / "src/world/RagEffectCatalog.generated.inc").read_text(encoding="utf-8")


def function_body(source: str, signature: str, last: bool = False) -> str:
    starts = list(re.finditer(signature, source))
    if not starts:
        raise AssertionError(f"missing {signature}")
    start = starts[-1] if last else starts[0]
    opening = source.find("{", start.end())
    depth = 0
    for offset in range(opening, len(source)):
        if source[offset] == "{":
            depth += 1
        elif source[offset] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:offset]
    raise AssertionError(f"unterminated {signature}")


pe_dispatch = function_body(PE, r"__thiscall CRagEffect::OnProcess3\(CRagEffect \*this\)", last=True)
ref_dispatch = function_body(REF, r"unsigned __int8 __thiscall CRagEffect::OnProcess3\(CRagEffect \*this\)")
for label, body in (("PE", pe_dispatch), ("Ref", ref_dispatch)):
    branch = re.search(r"case\s+0x239:\s*case\s+0x25B:\s*goto\s+\$L146677", body)
    if branch is None:
        raise AssertionError(f"{label} effect603 must join the shared STR tail")
if not re.search(r"\$L146677:\s*result\s*=\s*CRagEffect::ProcessEZ2STR\(this,\s*0\.0,\s*-80\.0,\s*0\.0\);\s*if\s*\(\s*!result\s*\)\s*return result;\s*return 1", REF):
    raise AssertionError("Ref effect603 shared tail must propagate STR completion")
if not re.search(r"\$L146677:\s*result\s*=\s*CRagEffect::ProcessEZ2STR\(this,\s*0\.0,\s*-80\.0,\s*0\.0\);\s*if\s*\(\s*!result\s*\)\s*return result;\s*return 1", PE):
    raise AssertionError("PE effect603 shared tail must propagate STR completion")

for label, init_source in (("PE", PE), ("Ref", REF)):
    init = function_body(init_source, r"__thiscall CRagEffect::Init\(CRagEffect \*this, CRenderObject \*master, int effectId, vector3d fDeltaPos\)", last=label == "PE")
    if not re.search(r"case\s+603:\s*\$L147740:\s*\w+\s*=\s*rand\(\);\s*sprintf\(StrName,\s*\"FireHit%d\.str\",\s*\w+\s*%\s*3\s*\+\s*1\)", init):
        raise AssertionError(f"{label} Init effect603 must choose FireHit1.str through FireHit3.str with one RNG draw")
firehit_format = ExeImage().read_cstring_at(0x71A8B8)
if firehit_format is None or firehit_format.raw != b"FireHit%d.str":
    raise AssertionError("PE bytes at VA 0x71A8B8 no longer resolve to FireHit%d.str")
if not re.search(r'\{\s*603,\s*nullptr,\s*nullptr,\s*"FireHit",\s*1,\s*3\s*\}', CATALOG):
    raise AssertionError("local effect catalog must select one of FireHit1.str through FireHit3.str")

if not re.search(r"case\s+603:\s+return ProcessTier3StrEffect\(\);", DISPATCH):
    raise AssertionError("effect603 dispatch route changed")
local_tier3 = function_body(LOCAL, r"bool CRagEffect::ProcessTier3StrEffect\(\)")
if not re.search(r"case\s+569:\s*case\s+603:\s*break;", local_tier3):
    raise AssertionError("local effect603 must have no event-frame wave, position, or side-effect branch")
if "const bool active = ProcessEZ2STR(0.0f, -80.0f, 0.0f);" not in local_tier3 or "return propagateStrResult ? active : true;" not in local_tier3:
    raise AssertionError("local effect603 must propagate the generic STR completion result")

print("effect603 FireHit source check passed: silent caller, propagated STR completion, 1-of-3 Init RNG selector")
