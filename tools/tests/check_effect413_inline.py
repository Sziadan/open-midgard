"""Compare effect413's inline PE behavior with the explicit local port."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
pe_text = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
ref_text = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
local_text = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
caller = (ROOT / "src/world/SkillSpecialEffect.cpp").read_text(encoding="utf-8")

signature = "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)"
start = pe_text.rindex(signature)
opening = pe_text.index("{", start)
depth = 0
for pos in range(opening, len(pe_text)):
    if pe_text[pos] == "{":
        depth += 1
    elif pe_text[pos] == "}":
        depth -= 1
        if depth == 0:
            pe = pe_text[start:pos + 1]
            break
else:
    raise SystemExit("unterminated canonical OnProcess")

case_start = pe.index("case 413:")
case_end = pe.index("case 414:", case_start)
case = pe[case_start:case_end]
for token in (
    "m_stateCnt == 5", "this->m_pos = (*p_m_master)->m_pos",
    "PlayWave(aEffectT_25, xbm, 0.0, zbm, 250, 40, 1.0)",
    "m_BodyLight & 0x20", "v94 >= 17", "v94 <= 50",
    "m_BodyLight += 32", "m_BodyTime = LOWORD(this->m_stateCnt) - 17",
):
    if token not in case:
        raise SystemExit(f"canonical PE effect413 case missing {token!r}")
if "char aEffectT_25[] = \"effect\\\\T_\";" not in pe_text:
    raise SystemExit("canonical effect413 wave literal changed")
if "case 413:" in ref_text:
    raise SystemExit("primary Ref unexpectedly contains an effect413 caller case")
if not re.search(r"case 415:[\s\S]{0,300}?LaunchOn\(target,\s*413\)", caller):
    raise SystemExit("skill415 target caller no longer launches effect413")

sig = "bool CRagEffect::ProcessEffect413()"
local_start = local_text.rindex(sig)
local_open = local_text.index("{", local_start)
depth = 0
for pos in range(local_open, len(local_text)):
    if local_text[pos] == "{":
        depth += 1
    elif local_text[pos] == "}":
        depth -= 1
        if depth == 0:
            local = local_text[local_start:pos + 1]
            break
else:
    raise SystemExit("unterminated local effect413 handler")
if "bool ProcessEffect413();" not in header or "case 413: return ProcessEffect413();" not in dispatch:
    raise SystemExit("local effect413 declaration or generated route missing")
ordered = (
    "if (m_stateCnt == 5)", "m_effectPos = master->m_pos", '"effect\\\\T_"',
    "master->m_BodyLight & 0x20", "m_stateCnt >= 17", "m_stateCnt <= 50",
    "master->m_BodyLight += 32", "master->m_BodyTime = static_cast<u16>(m_stateCnt - 17)",
)
cursor = 0
for token in ordered:
    found = local.find(token, cursor)
    if found < 0:
        raise SystemExit(f"local effect413 behavior missing or out of order: {token}")
    cursor = found + len(token)
if "rand(" in local or "LaunchEffectPrim" in local:
    raise SystemExit("effect413 must not consume RNG or emit a primitive")
print("effect413 parity: inline caller, state5 wave and states17-50 body-light window passed")
