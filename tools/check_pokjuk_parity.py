#!/usr/bin/env python3
"""Compare source branches and state/geometry mappings for RenderPokJuk."""
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


source = body(EXE, "void __thiscall CEffectPrim::RenderPokJuk(CEffectPrim *this, matrix *wtm)\n{")
dispatch = body(EXE, "void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm)\n{")
impl = body(IMPL, "void RenderPokJuk(CEffectPrim& prim,")
render_dispatch = body(IMPL, "void CEffectPrim::Render(matrix* viewMatrix)")

checks = {
    "canonical 0x34 route and implementation dispatch": (
        dispatch, r"case 0x34:\s*CEffectPrim::RenderPokJuk\(this, wtm\)",
        render_dispatch, r"m_type == PP_POKJUK\)\s*\{\s*RenderPokJuk\(\*this, \*viewMatrix\)"),
    "four bands and active life/positive-alpha gates": (
        source, r"v29 = 4;[\s\S]*?p_z - 140\) == 1 && \*\(\(__int16 \*\)p_z - 69\) > 0",
        impl, r"band < 4[\s\S]*?gi\.life != 1 \|\| gi\.alphaB <= 0"),
    "all five source color modes map to style flag cases": (
        source, r"(?=[\s\S]*v4 = \*\(\(_BYTE \*\)p_z - 36\))(?=[\s\S]*r = -1;\s*g = 70;\s*b = 70)(?=[\s\S]*r = 70;\s*g = -1;\s*b = 70)(?=[\s\S]*g = -1;\s*b = 90)(?=[\s\S]*g = 70;\s*b = -1)(?=[\s\S]*b = -1;\s*r = 70;\s*g = 70)",
        impl, r"switch \(gi\.flag\[0\]\)[\s\S]*?case 0: red = 70; green = 70; blue = 255;[\s\S]*?case 1: red = 255; green = 70; blue = 70;[\s\S]*?case 2: red = 70; green = 255; blue = 70;[\s\S]*?case 3: red = 255; green = 255; blue = 90;[\s\S]*?default: break"),
    "two passes with alpha divided by pass number": (
        source, r"v20 = v5 \+ 1;[\s\S]*?v21 = v18 / v20;[\s\S]*?if \( v20 >= 2 \)",
        impl, r"pass < 2[\s\S]*?gi\.alphaB\) / \(pass \+ 1\)"),
    "radius, four quarter-turn corners, and toward-screen rotation": (
        source, r"v7 = \*\(p_z - 33\) - minus;[\s\S]*?v9 = v6 \+ 90;[\s\S]*?v11 = v9 \+ 90;[\s\S]*?v13 = v11 \+ 90;[\s\S]*?Tei_TowardScreen",
        impl, r"gi\.maxHeight - minus[\s\S]*?90 \* corner[\s\S]*?IntimidateTowardScreen\(corners, prim\.m_gi\[0\]\.alphaT, prim\.m_gi\[1\]\.alphaT\)"),
    "source's next-angle Z lookup and final-corner exception are retained": (
        source, r"vec1\.z = \*\(\(float \*\)&g_3dDevice[\s\S]*?\+ v9\) \* v7;[\s\S]*?vec2\.z = \*\(\(float \*\)&g_3dDevice[\s\S]*?\+ v11\) \* v7;[\s\S]*?vec3\.z = \*\(\(float \*\)&g_3dDevice[\s\S]*?\+ v13\) \* v7;[\s\S]*?vec4\.z = g_sinTable\[v13\] \* v7",
        impl, r"zAngle = corner == 3 \? angle : static_cast<int>\(gi\.rotStart\) \+ 90 \* \(corner \+ 1\);[\s\S]*?zAngle %= 360;[\s\S]*?g_sinTable\[zAngle\]"),
    "source offset choices preserve all four branches": (
        source, r"p_z\[4\][\s\S]*?p_z\[1\][\s\S]*?p_z\[7\][\s\S]*?p_z - 2",
        impl, r"pass == 0\) offset = \{ gi\.sparkX, gi\.sparkY, gi\.sparkZ \}[\s\S]*?pass == 1\) offset = \{ gi\.vecBPreX, gi\.arcBaselineY, gi\.vecBPreZ \}[\s\S]*?pass == 2\) offset = \{ gi\.vecTNowX, gi\.vecTNowY, gi\.vecTNowZ \}[\s\S]*?else offset = \{ gi\.vecTPreX, gi\.vecTPreY, gi\.vecTPreZ \}"),
    "textured and style branches preserve selector, tint and alpha differences": (
        source, r"v19 = \*\(\(_BYTE \*\)p_z - 33\);[\s\S]*?v21 / 4[\s\S]*?0xFFu,[\s\S]*?else\s*CEffectPrim::RenderTeiRect[\s\S]*?v21,[\s\S]*?r,\s*g,\s*b",
        impl, r"(?=[\s\S]*gi\.flag\[3\] != 0)(?=[\s\S]*if \(textured\) alpha /= 4;)(?=[\s\S]*textured \? 255 : red)(?=[\s\S]*textureIndex)"),
    "source radius inset sequence 0.5 then 0.25 maps to the two passes": (
        source, r"four \? v22 \+ 0\.25 : v22 \+ 0\.5",
        impl, r"minus \+= pass == 0 \? 0\.5f : 0\.25f"),
}

for name, (src, src_re, dst, dst_re) in checks.items():
    assert re.search(src_re, src, re.S), f"reference mismatch: {name}"
    assert re.search(dst_re, dst, re.S), f"implementation mismatch: {name}"
    print(f"PASS {name}")

assert source.count("CEffectPrim::RenderTeiRect(") == 2, "expected texture/style RenderTeiRect branch pair"
assert source.count("p_z += 45;") == 1, "expected four 180-byte GI bands"
print("PASS source has exactly two RenderTeiRect branches and advances four GI bands")
print(f"{len(checks) + 1} mechanical RenderPokJuk branch comparisons passed")
