#!/usr/bin/env python3
"""Compare Bottom_Vertical effects 277/287/290/292/294 to PE and Ref helper."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage, SymbolTable  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/dhxj/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
PRIMARY = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
GENERATOR = (ROOT / "tools/generate_effect_dispatch.py").read_text(encoding="utf-8")
symbols = SymbolTable(ExeImage())

def require(ok: bool, message: str) -> None:
    if not ok:
        raise SystemExit(message)

expected = {
    277: ("aEffectRingBlue", 0, b"effect\\ring_blue.tga", 0x71943C),
    287: ("aEffectRingRedT", 1, b"effect\\ring_red.tga", 0x719454),
    290: ("aEffectRingRedT", 0, b"effect\\ring_red.tga", 0x719454),
    292: ("aEffectMagicGre", 2, b"effect\\magic_green.tga", 0x71A730),
    294: ("aEffectSafeline", 4, b"effect\\safeline.bmp", 0x71A560),
}
for effect_id, (symbol, profile, raw, va) in expected.items():
    require(re.search(rf"case {effect_id}:\s*CRagEffect::Bottom_Vertical\(this,\s*{symbol},\s*{profile}\);", PE),
            f"PE caller changed for effect {effect_id}")
    require(f"case {effect_id}: return ProcessBottomVertical({effect_id});" in DISPATCH,
            f"local dispatch missing effect {effect_id}")
    require(re.search(rf"{effect_id}:\s*\"return ProcessBottomVertical\({effect_id}\);\"", GENERATOR),
            f"generator route missing effect {effect_id}")
    resolved = symbols.resolve_name(symbol)
    require(resolved is not None and (resolved.va, resolved.raw) == (va, raw),
            f"{symbol} executable resource mismatch")

pe_helper = re.search(r"void __thiscall CRagEffect::Bottom_Vertical\([^)]*\)\s*\{(.*?)\n\}", PE, re.S)
ref_helper = re.search(r"void CRagEffect\s*::\s*Bottom_Vertical\([^)]*\)\s*\{(.*?)\n\}", REF, re.S)
require(pe_helper is not None and ref_helper is not None, "PE/secondary Ref helper missing")
for source_name, body in (("PE", pe_helper.group(1)), ("secondary Ref", ref_helper.group(1))):
    for token in ("PP_BOTTOM2", "m_stateCnt", "m_duration", "m_size", "full_display_angle", "vecB_pre", "vecB_now", "alphaB", "height[0]", "360"):
        require(token in body, f"{source_name} helper lacks {token}")
    require("rand()" in body or "random(" in body, f"{source_name} helper RNG calls missing")
require("ProcessBottomVertical(int effectId)" in LOCAL
        and "rand() % 40 - 20" in LOCAL and "rand() % 7" in LOCAL
        and "gi.life = index < 2 ? 1 : 0" in LOCAL,
        "local handler profile behavior changed")
require("Bottom_Vertical" not in PRIMARY,
        "primary Ref should continue to record caller omission for this PE-only group")
print("effects277/287/290/292/294 Bottom_Vertical source parity passed")
