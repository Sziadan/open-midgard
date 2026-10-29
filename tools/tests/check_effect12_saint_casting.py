"""Compare effect12 inline SaintCasting caller and local paired route."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
SRC = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
FIXTURE = (ROOT / "tools/tests/EffectLifecycleTests.cpp").read_text(encoding="utf-8")


def body(text: str, signature: str, offset: int = 0) -> str:
    start = text.index(signature, offset)
    opening = text.index("{", start)
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[start : pos + 1]
    raise AssertionError(f"unterminated body: {signature}")


pe_signature = "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)"
pe_process = body(PE, pe_signature, PE.rindex(pe_signature))
start = pe_process.index("case 12:")
end = pe_process.index("case 13:", start)
pe_case = pe_process[start:end]
frame = body(SRC, "bool CRagEffect::ProcessEffectFrame()")
paired = body(SRC, "void CRagEffect::ApplyPairedSaintCasting(")

if not re.search(r"this->m_pos\s*=\s*\(\*p_m_master\)->m_pos", pe_case):
    raise SystemExit("canonical effect12 parent-position refresh is missing")
if not re.search(r"if\s*\(\s*!this->m_stateCnt\s*\)", pe_case):
    raise SystemExit("canonical effect12 state-zero gate is missing")
ordered = (
    "PlayWave(aEffectEfBegins", "CRagEffect::SAINTCASTING(this, 45, aEffectRingYell, 1)",
    "CRagEffect::SAINTCASTING(this, 25, aEffectRingYell, 1)",
)
cursor = 0
for token in ordered:
    found = pe_case.find(token, cursor)
    if found < 0:
        raise SystemExit(f"canonical effect12 inline operation order missing: {token}")
    cursor = found + len(token)
if "case 12: ApplyPairedSaintCasting(\"effect\\\\ring_yellow.tga\", 1, true, 0.0f); return true;" not in DISPATCH:
    raise SystemExit("local generated effect12 paired-casting route is missing")
if '#include "RagEffectOnProcessCases.generated.inc"' not in frame:
    raise SystemExit("local effect dispatcher no longer includes generated routes")
local_order = (
    "if (playWave && m_stateCnt == 0)", '"effect\\\\EF_BeginSpell.wav"',
    "ApplySaintCasting(45, textureName, variant)", "ApplySaintCasting(25, textureName, variant)",
)
cursor = 0
for token in local_order:
    found = paired.find(token, cursor)
    if found < 0:
        raise SystemExit(f"local paired SaintCasting operation order missing: {token}")
    cursor = found + len(token)

for phrase in (
    "BeginSpell loads exact variant texture once per primitive including both SaintCasting rings",
    "Shared SaintCasting preserves all original variant size height alpha and texture branches",
):
    if phrase not in FIXTURE:
        raise SystemExit(f"SaintCasting lifecycle fixture missing {phrase!r}")

print("effect12 inline PE and generated local SaintCasting routes, wave/order and helper coverage passed")
