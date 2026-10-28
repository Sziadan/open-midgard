#!/usr/bin/env python3
"""Compare effect428's PE HitLine3(F1=0) no-emission caller path."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF2 = (ROOT / "Ref/dhxj/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
REF1 = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
GENERATOR = (ROOT / "tools/generate_effect_dispatch.py").read_text(encoding="utf-8")

def require(ok: bool, message: str) -> None:
    if not ok:
        raise SystemExit(message)

def function_body(source: str, signature: str) -> str | None:
    match = re.search(signature, source)
    if match is None:
        return None
    opening = source.find("{", match.end())
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:index]
    return None

require(re.search(r"case 428:\s*CRagEffect::HitLine3\(this, 0\);", PE) is not None,
        "PE effect428 caller mapping changed")
pe = function_body(PE, r"void __thiscall CRagEffect::HitLine3\([^)]*\)(?=\s*\{)")
require(pe is not None, "PE HitLine3 helper missing")
for token in ("this->m_pos = this->m_master->m_pos", "v3 = F1", "v4 = F1",
              "m_stateCnt >= v3", "m_stateCnt < v4"):
    require(token in pe, f"PE HitLine3 helper missing {token}")
ref = function_body(REF2, r"void\s+CRagEffect\s*::\s*HitLine3\([^)]*\)")
require(ref is not None and "int uNum;" in ref and "int start;" in ref
        and "if (F1 == 1)" in ref and "else if (F1 == 3)" in ref,
        "secondary Ref F1=0 undefined range discrepancy changed; re-audit")
require("case 428: return ProcessHitLine3Zero428();" in DISPATCH
        and re.search(r"428:\s*\"return ProcessHitLine3Zero428\(\);\"", GENERATOR),
        "effect428 local/generated route mismatch")
require("ProcessHitLine3Zero428()" in LOCAL and "m_effectPos = m_master->m_pos" in LOCAL,
        "local F1=0 position-only route missing")
require(re.search(r"CRagEffect\s*::\s*HitLine3\s*\(", REF1) is None,
        "primary Ref now contains HitLine3; re-audit source-version provenance")
print("effect428 HitLine3(F1=0) source parity passed; secondary Ref undefined range documented")
