#!/usr/bin/env python3
"""Compare TANJI/TANJI2 PE caller resources and local routes/payload."""
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
CALLSITES = (ROOT / "docs/skill/generated/playwave_callsites.tsv").read_text(encoding="utf-8")
symbols = SymbolTable(ExeImage())

def require(ok: bool, message: str) -> None:
    if not ok:
        raise SystemExit(message)

for effect_id, method, route in (
    (265, "TANJI", "ProcessTanji(false)"),
    (412, "TANJI2", "ProcessTanji(true)"),
):
    require(re.search(rf"case {effect_id}:\s*CRagEffect::{method}\(this\)", PE) is not None,
            f"PE caller for effect {effect_id} changed")
    require(f"case {effect_id}: return {route};" in DISPATCH,
            f"effect {effect_id} dispatcher route mismatch")
    require(re.search(rf"{effect_id}:\s*\"return {re.escape(route)};\"", GENERATOR) is not None,
            f"effect {effect_id} generator route missing")

resolved = symbols.resolve_name("aEffectBlueIvyB")
require(resolved is not None and (resolved.va, resolved.raw) == (0x7193A4, b"effect\\blue_ivy.bmp"),
        "TANJI primitive texture no longer resolves to effect\\blue_ivy.bmp")
require("0x63748F\tCRagEffect::TANJI\t0x71CB1C\teffect\\mon_" in CALLSITES
        and "TANJI" in CALLSITES and "0x71CB1C" in CALLSITES,
        "TANJI wave callsite is no longer resolved by the WP0 inventory")
for resource in ("PP_SHIELDBOOMERANG", "PP_TANJI2", "height[8]", "0.40000001", "maxHeight"):
    require(resource in PE, f"PE TANJI payload missing {resource}")
for resource in ("ProcessTanji(bool tanji2)", "PP_TANJI2 : PP_SHIELDBOOMERANG",
                 "effect\\\\blue_ivy.bmp", "0.40000001 + 2.0", "TryPlaySingleEffectWave"):
    require(resource in LOCAL, f"local TANJI port missing {resource}")
require(re.search(r"CRagEffect\s*::\s*TANJI2\s*\(\)", REF2) is not None,
        "secondary Ref TANJI2 helper is missing")
require(re.search(r"CRagEffect\s*::\s*TANJI\s*\(\).*?effect\\\\mon_tanji\.wav", REF2, re.S) is not None,
        "secondary Ref TANJI wave spelling changed; re-audit the PE/Ref mismatch")
require(re.search(r"CRagEffect\s*::\s*TANJI2?\s*\(\)", REF1) is None,
        "the primary Ref caller omission changed; re-audit caller mappings")
print("effects265/412 TANJI source parity passed; primary Ref helper omission recorded")
