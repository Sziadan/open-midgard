#!/usr/bin/env python3
"""Mechanical source/implementation branch checks for RenderLineLink."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
IMPL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")


def body(text: str, marker: str) -> str:
    start = text.index(marker)
    opening = text.index("{", start)
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[opening + 1 : pos]
    raise AssertionError(f"unclosed function: {marker}")


ref = body(SOURCE, "void __thiscall CEffectPrim::RenderLineLink(CEffectPrim *this, float wtm)\n")
impl = "\n".join(body(IMPL, marker) for marker in (
    "int LineLinkHeading(", "void LineLinkInterpolatedEnd(",
    "void LineLinkRingPoint(", "void RenderLineLink("))
checks = {
    "GI0 live and alpha gate": (r"m_GI\[0\]\.life == 1 && this->m_GI\[0\]\.alphaB > 0", r"gi\.life != 1 \|\| gi\.alphaB <= 0"),
    "side heading from now minus previous, then 90": (r"vecT_now\.x - this->m_GI\[0\]\.vecT_pre\.x", r"LineLinkHeading\(current\.x - previous\.x, current\.z - previous\.z\) \+ 90"),
    "heading normalized by delta length": (r"jb / v4", r"zDelta / length"),
    "process 15 endpoint cutoff": (r"process >= 15", r"process >= 15"),
    "six-times-process sine interpolation": (r"g_sinTable\[6 \* process\]", r"g_sinTable\[6 \* process\]"),
    "two radial layers": (r"v11 >= 2", r"layer < 2"),
    "layer radii maxheight and twice maxheight": (r"max_height \+ this->m_GI\[0\]\.max_height", r"gi\.maxHeight \* \(layer == 0 \? 1\.0f : 2\.0f\)"),
    "cross-section byte step and endpoint": (r"v13 \+= 144", r"around \+= 144"),
    "cross-section table byte-offset conversion": (r"v13 \+ 7522556", r"around / 4"),
    "mode 2 inner and outer colors": (r"0xFFu,\s*0x32u,\s*0", r"0x00FF3200u"),
    "mode 2 outer color": (r"v26 = -106;\s*v25 = 100;\s*v24 = 0", r"0x00006496u"),
    "mode 2 selector": (r"LODWORD\(this->m_GI\[0\]\.height\[4\]\) == 0x40000000", r"gi\.height\[4\] == 2\.0f"),
    "mode 3 shared pink color": (r"0xFFu,\s*0x59u,\s*0xB6u", r"0x00FF59B6u"),
    "mode 3 selector": (r"LODWORD\(this->m_GI\[0\]\.height\[4\]\) == 1077936128", r"gi\.height\[4\] == 3\.0f"),
    "default inner red and outer yellow": (r"v26 = 0;\s*v25 = -1;\s*\}\s*v24 = -1", r"0x00FFFF00u"),
    "default selector": (r"if \( LODWORD\(this->m_GI\[0\]\.height\[4\]\) != 1077936128 \)", r"else \{\s*tint = 0x00FFFF00u;"),
    "outer alpha doubles": (r"2 \* this->m_GI\[0\]\.alphaB", r"alpha \*= 2"),
    "additive world-space custom rect and UVs": (r"\n\s*0,\s*0,\s*1,", r"D3DBLEND_ONE"),
}
for name, (source_pattern, impl_pattern) in checks.items():
    assert re.search(source_pattern, ref, re.S), f"reference missing: {name}"
    assert re.search(impl_pattern, impl, re.S), f"implementation missing: {name}"
    print(f"PASS {name}")

dispatch = body(SOURCE, "void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm)\n")
assert re.search(r"case 0x26:\s*CEffectPrim::RenderLineLink", dispatch), "source dispatch case 0x26 missing"
assert re.search(r"m_type == PP_LINELINK\)\s*\{\s*RenderLineLink", IMPL), "PP_LINELINK implementation route missing"
print("PASS source dispatch case 0x26 maps to PP_LINELINK")
print(f"{len(checks) + 1} mechanical source/implementation comparisons passed")
