#!/usr/bin/env python3
"""Check effect402 PortalWind variant1 caller and local frame-window parity."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]


def function(source: str, signature: str) -> str:
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[start:pos + 1]
    raise AssertionError(f"unterminated function: {signature}")


pe = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
ref = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
local = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")

pe_onprocess = function(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
pe_branch = pe_onprocess[pe_onprocess.index("case 402:"):pe_onprocess.index("case 403:")]
if not re.search(r"PortalWind\(this, 1\);", pe_branch):
    raise AssertionError("PE effect402 PortalWind variant1 call missing")
if not re.search(r"v16\s*>=\s*5\s*&&\s*v16\s*<=\s*65", pe_branch):
    raise AssertionError("PE effect402 inclusive frame 5..65 gate changed")
if "m_BodyLight & 1" not in pe_branch or "SetArgb(*p_m_master, -1, 250, 250, 200)" not in pe_branch:
    raise AssertionError("PE effect402 light bit or tint changed")

ref_onprocess = function(ref, "unsigned __int8 __thiscall CRagEffect::OnProcess2(CRagEffect *this)")
if re.search(r"case\s+402\s*:", ref_onprocess):
    raise AssertionError("primary Ref unexpectedly contains an effect402 OnProcess case")

if "case 402: ApplyPortalWind402(); return true;" not in dispatch:
    raise AssertionError("local effect402 generated route changed")
local_process = function(local, "void CRagEffect::ApplyPortalWind402()")
expected_order = ("ApplyPortalWind(1);", "m_stateCnt < 5 || m_stateCnt > 65", "m_BodyLight & 1",
                  "master->SetArgb(-1, 250, 250, 200);")
cursor = 0
for token in expected_order:
    pos = local_process.find(token, cursor)
    if pos < 0:
        raise AssertionError(f"local effect402 behavior/order missing {token}")
    cursor = pos + len(token)
if "ResolveCurrentMaster()" not in local_process:
    raise AssertionError("local effect402 master guard missing")

print("effect402 PortalWind parity passed: PE variant/timing/tint, Ref omission, local route and guarded behavior")
