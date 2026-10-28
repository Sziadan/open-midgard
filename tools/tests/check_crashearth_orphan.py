"""Verify CrashEarth helper identity and that it has no canonical effect caller."""
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


def compact(text: str) -> str:
    return re.sub(r"\s+", "", text)


signature = "bool __thiscall CRagEffect::CrashEarth(CRagEffect *this)"
pe_body = fn(pe, signature)
ref_body = fn(ref, signature)
if compact(pe_body) != compact(ref_body):
    raise SystemExit("CrashEarth PE/primary Ref helper mismatch")
for source, label in ((pe, "PE"), (ref, "Ref")):
    if "CRagEffect::CrashEarth(this" in source:
        raise SystemExit(f"{label} unexpectedly invokes orphaned CrashEarth")
for effect_id in (70, 75, 76, 77, 78):
    if f"case {effect_id}:" in dispatch and "CrashEarth" in dispatch:
        raise SystemExit("CrashEarth was incorrectly routed to a neighboring effect id")
for token in ("m_stateCnt == 30", "aEffectWizardFi", "m_stateCnt == 35",
              "135, 0, 0, 0", "CRagEffect::ProcessEZ2STR(this, 0.0, -80.0, 0.0)"):
    if token not in pe_body:
        raise SystemExit(f"CrashEarth helper behavior missing {token}")
wave = SymbolTable(ExeImage()).resolve_name("aEffectWizardFi")
if wave is None or not wave.raw.startswith(b"effect\\") or not wave.raw.lower().endswith(b".wav"):
    raise SystemExit(f"canonical wizard wave bytes unavailable: {wave!r}")
if "CrashEarth" in local or re.search(r"case\s+\d+\s*:\s*return\s+[^;]*CrashEarth", dispatch):
    raise SystemExit("local implementation unexpectedly routes orphaned CrashEarth")
print("CrashEarth orphan parity: PE/Ref body identity, no caller/route, state30 wave/state35 message and exact PE wave bytes passed")
