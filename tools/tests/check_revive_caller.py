"""Compare effect140 Revive with canonical PE and primary Ref helper evidence."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage, SymbolTable


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
    raise AssertionError(f"unterminated function {signature}")


def compact(text: str) -> str:
    return re.sub(r"\s+", "", text)


pe_text = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
ref_text = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
local_text = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")

signature = "void __thiscall CRagEffect::Revive(CRagEffect *this)"
pe = function(pe_text, signature)
ref = function(ref_text, signature)
if compact(pe) != compact(ref):
    raise SystemExit("canonical PE and primary Ref Revive helper bodies differ")
pe_main = function(pe_text, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
case140 = pe_main[pe_main.index("case 140:"):pe_main.index("case 142:")]
if "Revive(this)" not in case140:
    raise SystemExit("canonical PE effect140 caller no longer invokes Revive")
if "CRagEffect::Revive(this)" in ref_text:
    raise SystemExit("primary Ref caller now invokes Revive; review caller parity")

local = function(local_text, "bool CRagEffect::ProcessRevive140()")
if "bool ProcessRevive140();" not in header or "case 140: return ProcessRevive140();" not in dispatch:
    raise SystemExit("local effect140 declaration or generated route is missing")
ordered = (
    "(m_stateCnt % 25) == 0", "LaunchEffectPrim(PP_3DCYLINDER", "m_duration = 60",
    "m_longSpeed = 1.5f", "m_longAccel = 0.029999999f", "m_outerSize = 10.5f",
    "m_innerSpeed = -0.12f", "m_innerAccel = 0.003f", "m_outerSpeed = -0.12f",
    "m_outerAccel = 0.003f", "m_innerSize = 7.5f", "m_heightSize = 7.5f", "m_alpha = 240.0f",
    "m_duration - prim->m_duration / 2", "if (m_stateCnt == 0)", "m_duration = m_duration",
    "m_innerSpeed = 0.0f", "m_innerAccel = 0.0f", "m_outerSize = 11.0f",
    "m_outerSpeed = 0.0f", "m_outerAccel = 0.0f", "m_duration / 10",
    '"effect\\\\ring_yellow.tga"',
)
cursor = 0
for token in ordered:
    pos = local.find(token, cursor)
    if pos < 0:
        raise SystemExit(f"local Revive payload missing or out of order: {token}")
    cursor = pos + len(token)
if re.search(r"\brand\s*\(|\bPlayWave\s*\(", local):
    raise SystemExit("Revive canonical source has no RNG or audio")

family = function(local_text, "bool CEffectPrim::UsesReferenceCylinder() const")
if "case 140: return true;" not in family:
    raise SystemExit("effect140 PP_3DCYLINDER reference renderer family binding is missing")

resolved = SymbolTable(ExeImage(ROOT / "HighPriest.exe")).resolve_name("aEffectRingYell")
if resolved is None or resolved.va != 0x7193E8 or resolved.raw != b"effect\\ring_yellow.tga":
    raise SystemExit(f"Revive PE texture bytes changed: {resolved!r}")
if '"effect\\\\ring_yellow.tga"' not in local_text:
    raise SystemExit("local Revive texture literal differs from the executable")

print("Revive140 parity: PE/Ref helper, PE caller, route, pulse payload, exact texture and cylinder family passed")
