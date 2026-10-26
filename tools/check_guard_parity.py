#!/usr/bin/env python3
"""Mechanically compare Guard source branches and geometry/material port."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
IMPL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")


def body(text: str, marker: str) -> str:
    start = text.index(marker)
    opening = text.index("{", start)
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{": depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0: return text[opening + 1:pos]
    raise AssertionError(f"unclosed body: {marker}")


source = body(EXE, "void __thiscall CEffectPrim::RenderGuard(CEffectPrim *this, matrix *wtm)\n{")
dispatcher = body(EXE, "void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm)\n{")
impl = body(IMPL, "void RenderGuard(CEffectPrim& prim,")
geometry = body(IMPL, "void GuardRingCorners(")
renderDispatcher = body(IMPL, "void CEffectPrim::Render(matrix* viewMatrix)")
tei = body(EXE, "void __thiscall CEffectPrim::RenderTeiRect(\n        CEffectPrim *this,\n        int *rpCnt,\n        __int16 alpha,")

checks = {
    "canonical 0x3D route": (
        dispatcher, r"case 0x3D:\s*CEffectPrim::RenderGuard\(this, wtm\)",
        renderDispatcher, r"m_type == PP_GUARD\)\s*\{\s*RenderGuard\(\*this, \*viewMatrix\)"),
    "four bands and life-only active gate": (
        source, r"v158 = 4;[\s\S]*?height - 20\) == 1",
        impl, r"band < 4[\s\S]*?gi\.life != 1"),
    "three ring rows and five slices per row": (
        source, r"v3 = -45;[\s\S]*?v164 = -90;[\s\S]*?v164 \+= 45;[\s\S]*?v164 >= 135[\s\S]*?i \+ 45 < 90[\s\S]*?i \+= 45",
        impl, r"row < 3[\s\S]*?rowAngle = -45 \+ row \* 45[\s\S]*?slice < 5[\s\S]*?sliceAngle = -90 \+ slice \* 45"),
    "all three surface radius and vertical branches": (
        source, r"\*\(height - 3\) \+ 2\.0[\s\S]*?\*\(height - 3\) \+ 0\.2[\s\S]*?\*height \+ 0\.40000001[\s\S]*?\*\(height - 3\)",
        impl, r"firstRadius = gi\.maxHeight[\s\S]*?firstRadius \+ 2\.0f[\s\S]*?maxHeight \+ 0\.2f[\s\S]*?-gi\.maxHeight[\s\S]*?maxHeight \+ gi\.height\[0\] \+ 0\.4f"),
    "orientation, perpendicular angles and source wrap logic": (
        source, r"v9 = \*\(\(__int16 \*\)height - 2\) \+ v164;[\s\S]*?v18 = v9 - 90;[\s\S]*?v18 = v9 - 450;[\s\S]*?v19 = v18 \+ \(v18 < 0 \? 0x168 : 0\)",
        geometry, r"gi\.rotStart\) \+ sliceAngle;[\s\S]*?orientation >= 360[\s\S]*?orientation -= 360[\s\S]*?orientation < 0[\s\S]*?orientation \+= 360[\s\S]*?perpendicular = orientation - 90"),
    "all three source color branch pairs": (
        source, r"0xFFu",
        impl, r"gi\.flag\[0\] == 2"),
    "alpha and texture/blend selection per tier": (
        source, r"\*\(\(_WORD \*\)height - 9\)[\s\S]*?\n\s*0,\s*0,\s*0,[\s\S]*?\n\s*0,\s*0,\s*0,[\s\S]*?\n\s*1,\s*1,\s*0",
        impl, r"gi\.alphaB[\s\S]*?prim\.m_texture\[0\][\s\S]*?D3DBLEND_ONE[\s\S]*?prim\.m_texture\[1\][\s\S]*?D3DBLEND_INVSRCALPHA"),
    "distance scratch mutation follows 2,3,2 and ends at two": (
        source, r"count2 - 1[\s\S]*?\*v4 = v163 \* -1\.0[\s\S]*?1\.5 - \*v4 \+ 1\.5[\s\S]*?\*v4 = v160",
        impl, r"gi\.distance = static_cast<float>\(3 - \(std::abs\(row - 1\)\)\)[\s\S]*?innerRadius = static_cast<int>\(gi\.distance\)"),
}

for name, (src, src_re, dst, dst_re) in checks.items():
    assert re.search(src_re, src, re.S), f"reference branch mismatch: {name}"
    assert re.search(dst_re, dst, re.S), f"implementation branch mismatch: {name}"
    print(f"PASS {name}")

assert source.count("height + 84) == 2") == 3, "expected one mode switch per surface tier"
for rgb in (("0xFFu", "0x9Bu", "0"), ("0xCDu", "0xFFu", "0xCDu"),
            ("0xFFu", "0xFFu", "0x9Bu"), ("0x9Bu", "0xFFu", "0x9Bu"),
            ("0xE1u", "0xFFu", "0xE1u")):
    assert all(component in source for component in rgb), f"source color missing: {rgb}"
for expression in ("gi.flag[0] == 2 ? 255 : 205", "gi.flag[0] == 2 ? 155 : 255",
                    "gi.flag[0] == 2 ? 0 : 205", "gi.flag[0] == 2 ? 255 : 155",
                    "gi.flag[0] == 2 ? 155 : 155", "gi.flag[0] == 2 ? 255 : 225",
                    "gi.flag[0] == 2 ? 0 : 225"):
    assert expression in impl, f"implementation color branch missing: {expression}"

assert source.count("CEffectPrim::RenderTeiRect(") == 6, "expected two color alternatives at each of three tiers"
assert re.search(r"PW == 0[\s\S]*?destAlphaMode = 2[\s\S]*?destAlphaMode = 6", tei, re.S)
print("PASS six source TeiRect branches and corrected PW blend mapping")
print(f"{len(checks) + 1} mechanical RenderGuard comparisons passed")
