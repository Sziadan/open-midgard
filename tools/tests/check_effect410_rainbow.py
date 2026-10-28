#!/usr/bin/env python3
"""Compare effect410 Rainbow caller, texture, and state-zero payload."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage, SymbolTable  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF2 = (ROOT / "Ref/dhxj/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
REF1 = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
GENERATOR = (ROOT / "tools/generate_effect_dispatch.py").read_text(encoding="utf-8")
symbols = SymbolTable(ExeImage())

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

require(re.search(r"case 410:\s*CRagEffect::Rainbow\(this, aEffectAlphaCen, 0\);", PE) is not None,
        "PE effect410 caller mapping changed")
resolved = symbols.resolve_name("aEffectAlphaCen")
require(resolved is not None and (resolved.va, resolved.raw) ==
        (0x7192FC, b"effect\\alpha_center.tga"),
        "Rainbow texture no longer resolves to effect\\alpha_center.tga")
require("case 410: return ProcessRainbow410();" in DISPATCH
        and re.search(r"410:\s*\"return ProcessRainbow410\(\);\"", GENERATOR),
        "effect410 local/generated route mismatch")

pe_body = function_body(PE, r"void __thiscall CRagEffect::Rainbow\([^)]*\)(?=\s*\{)")
ref_body = function_body(REF2, r"void\s+CRagEffect\s*::\s*Rainbow\([^)]*\)")
pe_helper = pe_body
ref_helper = ref_body
require(pe_helper is not None and ref_helper is not None, "PE or secondary Ref Rainbow helper missing")
for label, body in (("PE", pe_helper), ("secondary Ref", ref_helper)):
    for token in ("m_stateCnt", "PP_RAINBOW", "m_renderFlag", "m_duration", "alphaT", "360",
                  "distance", "50.0", "full_display_angle", "rise_angle"):
        require(token in body, f"{label} Rainbow helper lost {token}")
require(re.search(r"CRagEffect\s*::\s*Rainbow\s*\(", REF1) is None,
        "primary Ref now has Rainbow; re-audit source-version provenance")
for token in ("ProcessRainbow410()", "PP_RAINBOW", "effect\\\\alpha_center.tga",
              "GetCameraLongitude()", "m_stateCnt != 0", "prim->m_totalTexture = 1"):
    require(token in LOCAL, f"local Rainbow implementation missing {token}")
print("effect410 Rainbow source parity passed; primary Ref helper omission recorded")
