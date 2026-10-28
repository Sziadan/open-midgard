"""Check Cure's orphan helper against canonical inline PE effect66 behavior."""
from __future__ import annotations
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage, SymbolTable

pe = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
ref = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
local = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")


def fn(text: str, sig: str) -> str:
    start = text.rindex(sig)
    opening = text.index("{", start)
    depth = 0
    for i in range(opening, len(text)):
        if text[i] == "{": depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0: return text[start:i + 1]
    raise AssertionError(sig)


helper = "bool __thiscall CRagEffect::Cure(CRagEffect *this)"
if re.sub(r"\s+", "", fn(pe, helper)) != re.sub(r"\s+", "", fn(ref, helper)):
    raise SystemExit("Cure PE/primary Ref helper mismatch")
for source, label in ((pe, "PE"), (ref, "Ref")):
    if "CRagEffect::Cure(this" in source:
        raise SystemExit(f"{label} unexpectedly calls orphaned Cure helper")
main = fn(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
case = main[main.index("case 66:"):main.index("case 67:")]
for token in ("if ( !this->m_stateCnt )", "this->m_pos.z - v113->z",
              "this->m_pos.x - v113->x", "PlayWave(aEffectAcolyteC, xbu, 0.0, zbu, 250, 40, 1.0)",
              "goto LABEL_424"):
    if token not in case:
        raise SystemExit(f"inline PE effect66 behavior missing {token}")
if "case 66: return ProcessClipWaveStrEffect();" not in dispatch:
    raise SystemExit("local effect66 clip-wave/STR route missing")
legacy = fn(local, "bool CRagEffect::ProcessClipWaveStrEffect()")
if 'case 66: wave = "effect\\\\Acolyte_cure.wav";' not in legacy:
    raise SystemExit("local effect66 wave mapping missing")
wave = SymbolTable(ExeImage()).resolve_name("aEffectAcolyteC")
if wave is None or wave.raw != b"effect\\Acolyte_cure.wav":
    raise SystemExit(f"canonical Cure wave bytes changed: {wave!r}")
print("Cure66 parity: orphan helper identity, inline PE behavior, clip-wave/STR route and exact PE wave bytes passed")
