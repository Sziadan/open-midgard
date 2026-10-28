#!/usr/bin/env python3
"""Compare Land Protector effects 242/404 callers and shared payload."""
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

for case_id, resource, variant, filename, va in (
    (242, "aEffectAaaCopyB", 1, b"effect\\aaa copy.bmp", 0x71A584),
    (404, "aEffectSpiderwe", 2, b"effect\\spiderweb.tga", 0x71A4D0),
):
    caller = re.search(rf"case {case_id}:\s*CRagEffect::Bottom_LandProtector\(this,\s*{resource},\s*{variant}\);", PE)
    require(caller is not None,
            f"PE case {case_id} caller mapping changed")
    require(f"case {case_id}: return ProcessLandProtector({variant});" in DISPATCH,
            f"local case {case_id} route mismatch")
    require(re.search(rf"{case_id}:\s*\"return ProcessLandProtector\({variant}\);\"", GENERATOR) is not None,
            f"generator route for effect {case_id} is missing")
    resolved = symbols.resolve_name(resource)
    require(resolved is not None and (resolved.va, resolved.raw) == (va, filename),
            f"{resource} does not resolve to {filename!r} at 0x{va:X}")

helper = re.search(r"void __thiscall CRagEffect::Bottom_LandProtector\([^)]*\)\s*\{(.*?)\n\}", PE, re.S)
ref_helper = re.search(r"void CRagEffect\s*::\s*Bottom_LandProtector\([^)]*\)\s*\{(.*?)\n\}", REF2, re.S)
require(helper is not None and ref_helper is not None, "PE/secondary Ref helper missing")
for name, body in (("PE", helper.group(1)), ("secondary Ref", ref_helper.group(1))):
    for token in ("PP_LANDPROTECTOR", "m_stateCnt", "360", "height[0]", "height[4]", "alphaB", "RotStart"):
        require(token in body, f"{name} helper lacks {token}")
    require("distance = 12.0" in body and "distance = 7.0" in body,
            f"{name} helper variant distances changed")
require("ProcessLandProtector(int variant)" in LOCAL
        and '"effect\\\\aaa copy.bmp"' in LOCAL and '"effect\\\\spiderweb.tga"' in LOCAL,
        "local handler/resource mapping missing")
require("gi.distance = variant == 1 ? 7.0f : 12.0f" in LOCAL
        and "gi.height[4] = static_cast<float>(variant)" in LOCAL
        and "gi.riseAngle = static_cast<short>(rand() % 360)" in LOCAL,
        "local variant payload or one-draw random angle changed")
require("Bottom_LandProtector(this, aEffectHanmoon1, 3)" in REF1
        and "Bottom_LandProtector(this, aEffectHanmoon2, 3)" in REF1,
        "primary Ref caller omission should remain documented")
print("effect242_404_land_protector source parity passed; primary Ref caller omission recorded")
