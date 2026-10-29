#!/usr/bin/env python3
"""Compare effect392's inline Kyrie guard tint and local shared route."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
ROUTE = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")


def function_body(source, signature):
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:pos]
    raise AssertionError(f"unterminated function: {signature}")


pe_process = function_body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
pe_case = re.search(
    r"case 392:\s*if \( !this->m_stateCnt \)[\s\S]*?"
    r"if \( this->m_stateCnt <= 0x1Du \)[\s\S]*?"
    r"if \( \(\(\*p_m_master\)->m_BodyLight & 8\) != 8 \)[\s\S]*?"
    r"\(\*p_m_master\)->m_BodyLight \+= 8;[\s\S]*?"
    r"if \( this->m_stateCnt % 6 >= 3 \)[\s\S]*?"
    r"SetArgb\(\*p_m_master, -1, 250, 250, 250\);[\s\S]*?"
    r"SetArgb\(\*p_m_master, -1, 100, 100, 100\);[\s\S]*?goto LABEL_99;",
    pe_process,
)
if pe_case is None:
    raise AssertionError("canonical PE effect392 caller gate, tint sequence or common jump changed")

if "case 392: ApplyKyrieGuard(); return FinishEffectFrame();" not in ROUTE:
    raise AssertionError("local effect392 dispatcher/helper/common-finish route changed")
local_helper = function_body(LOCAL, "void CRagEffect::ApplyKyrieGuard()")
for token in (
    "m_stateCnt == 0", 'TryPlayEffectWaveAt(wavePos, "effect\\\\kyrie_guard.wav")',
    "m_stateCnt <= 29", "master->m_BodyLight += 8", "(m_stateCnt % 6) >= 3",
    "master->SetArgb(-1, 250, 250, 250)", "master->SetArgb(-1, 100, 100, 100)",
    "ProcessEZ2STR()",
):
    if token not in local_helper:
        raise AssertionError(f"local Kyrie guard helper missing {token}")

# The primary Ref's only numeric 392 case is its Init STR mapping (`aSwordStr`),
# not an OnProcess effect branch.
if REF.count("case 392:") != 1 or "case 392:\n            sprintf(StrName, aSwordStr);" not in REF:
    raise AssertionError("primary Ref effect392 provenance changed; re-audit its Init mapping")

image = ExeImage(ROOT / "HighPriest.exe")
wave = image.read_cstring_at(0x71A748)
if wave is None or wave.raw != b"effect\\kyrie_guard.wav":
    raise AssertionError("effect392 wave bytes differ at PE VA 0x71A748")

print("effect392 Kyrie guard parity passed: PE timing/tints, local helper/finish route, exact wave and primary-Ref caller provenance")
