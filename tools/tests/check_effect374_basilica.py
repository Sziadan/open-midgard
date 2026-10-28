#!/usr/bin/env python3
"""Compare Basilica's two PE caller variants with Ref and local routing."""
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

caller = re.search(r"case 374:\s*CRagEffect::Basilica\(this, aEffectAlphaDow, 0\);\s*"
                   r"CRagEffect::Basilica\(this, aEffectAlphaDow, 1\);", PE)
require(caller is not None, "PE Basilica caller order or variants changed")
resource = symbols.resolve_name("aEffectAlphaDow")
require(resource is not None and (resource.va, resource.raw) ==
        (0x719498, b"effect\\alpha_down.tga"),
        "Basilica texture no longer resolves to effect\\alpha_down.tga")
require("case 374: return ProcessBasilica();" in DISPATCH
        and re.search(r"374:\s*\"return ProcessBasilica\(\);\"", GENERATOR),
        "effect374 local/generated route mismatch")

pe_helper = re.search(r"void __thiscall CRagEffect::Basilica\([^)]*\)\s*\{(.*?)\n\}", PE, re.S)
ref_helper = re.search(r"void CRagEffect\s*::\s*Basilica\([^)]*\)\s*\{(.*?)\n\}", REF2, re.S)
require(pe_helper is not None and ref_helper is not None, "PE or secondary Ref Basilica helper missing")
for label, body, tokens in (
    ("PE", pe_helper.group(1), ("PP_RECT_UP2", "m_stateCnt", "m_renderFlag = 1",
        "m_duration", "m_size = 0.0", "max_height", "0.40000001", "53.200001", "alphaB")),
    ("secondary Ref", ref_helper.group(1), ("PP_RECT_UP2", "m_stateCnt", "RF_ALPHA", "RF_EFFECT",
        "height[4] = F1", "53.2f", "alphaT = 2415")),
):
    for token in tokens:
        require(token in body, f"{label} Basilica helper lost {token}")
require(re.search(r"CRagEffect\s*::\s*Basilica\s*\(", REF1) is None,
        "primary Ref now has Basilica; re-audit source-version provenance")
for token in ("ProcessBasilica()", "PP_RECT_UP2", "effect\\\\alpha_down.tga",
              "0.40000001", "53.200001", "gi.height[4] = static_cast<float>(variant)"):
    require(token in LOCAL, f"local Basilica implementation missing {token}")
print("effect374 Basilica source parity passed; primary Ref helper omission recorded")
