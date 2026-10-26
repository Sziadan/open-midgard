#!/usr/bin/env python3
"""Mechanically compare DoubleGumGang source branches with its renderer port."""
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
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[opening + 1:pos]
    raise AssertionError(f"unclosed function: {marker}")


source = body(EXE, "void __thiscall CEffectPrim::RenderDoubleGumGang(CEffectPrim *this, matrix *wtm)\n{")
dispatch = body(EXE, "void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm)\n{")
impl = body(IMPL, "void RenderDoubleGumGang(CEffectPrim& prim,")
layer = body(IMPL, "void RenderGumGangLayer(")
front = body(IMPL, "bool GumGangFrontFacing(")

checks = {
    "canonical executable route to the dedicated renderer": (
        dispatch, r"case 0x63:\s*CEffectPrim::RenderDoubleGumGang\(this, wtm\)",
        IMPL, r"m_type == PP_DOUBLEGUMGANG\)\s*\{\s*RenderDoubleGumGang\(\*this, \*viewMatrix\)"),
    "four GI bands gated by life equal to one": (
        source, r"v170 = 4;[\s\S]*?p_z - 164\) == 1",
        impl, r"band < 4[\s\S]*?gi\.life != 1"),
    "three nested visual layers": (
        source, r"if \( \+\+i >= 3 \)",
        impl, r"layer < 3"),
    "blue layer colors and 1, 1.2, 2 radial scales": (
        source, r"(?=[\s\S]*R = 100;\s*G = 100;\s*B = -1)(?=[\s\S]*R = 0;\s*G = 0;\s*B = 120)(?=[\s\S]*Rdist = 1\.2)(?=[\s\S]*Rdist = 2\.0)",
        IMPL, r"(?=[\s\S]*red = 100; green = 100; blue = 255;)(?=[\s\S]*red = 0; green = 0; blue = 120;)(?=[\s\S]*radialScales\[3\] = \{ 1\.0f, 1\.2f, 2\.0f \})"),
    "white mode has three distinct RGB layers": (
        source, r"(?=[\s\S]*R = 90;\s*G = 90;\s*B = 80)(?=[\s\S]*R = 60;\s*G = 60;\s*B = 50)(?=[\s\S]*R = 30;\s*G = 30;\s*B = 20)",
        impl, r"gi\.flag\[4\] == 1[\s\S]*?red = 90; green = 90; blue = 80;[\s\S]*?red = 60; green = 60; blue = 50;[\s\S]*?red = 30; green = 30; blue = 20"),
    "red mode has its source layer colors": (
        source, r"(?=[\s\S]*R = -1;\s*G = 100;\s*B = 100)(?=[\s\S]*R = 120;\s*G = 0;\s*B = 0)(?=[\s\S]*Rdist = 1\.2)",
        impl, r"else if \(layer == 0\)[\s\S]*?red = 255; green = 100; blue = 100;[\s\S]*?red = 120; green = 0; blue = 0"),
    "four source angle, alpha, texture/orientation slots map to helper": (
        source, r"(?=[\s\S]*p_z - 33)(?=[\s\S]*p_z - 29)(?=[\s\S]*p_z - 25)(?=[\s\S]*p_z - 21)(?=[\s\S]*p_z - 35)(?=[\s\S]*p_z - 31)(?=[\s\S]*p_z - 27)(?=[\s\S]*p_z - 23)(?=[\s\S]*p_z - 34)(?=[\s\S]*p_z - 30)(?=[\s\S]*p_z - 26)(?=[\s\S]*p_z - 22)",
        layer, r"angleSlots\[4\] = \{ 3, 7, 11, 15 \};[\s\S]*?alphaSlots\[4\] = \{ 1, 5, 9, 13 \};[\s\S]*?orientationSlots\[4\] = \{ 2, 6, 10, 14 \};[\s\S]*?textureSlots\[4\] = \{ 2, 6, 10, 14 \}"),
    "all four source strips use their own strict facing branch": (
        source, r"v10 < 70 && v10 > -80 \|\| v10 < -270 \|\| v10 > 270[\s\S]*?v23 < 70 && v23 > -80 \|\| v23 < -270 \|\| v23 > 270[\s\S]*?v58 < 70 && v58 > -80 \|\| v58 < -270 \|\| v58 > 270[\s\S]*?v71 < 70 && v71 > -80 \|\| v71 < -270 \|\| v71 > 270",
        front, r"difference < 70 && difference > -80[\s\S]*?difference < -270 \|\| difference > 270"),
    "source caps each strip alpha at the current GI process value": (
        source, r"if \( v22 > \*\(\(\_DWORD \*\)p_z - 38\) \)\s*v22 = \*\(\(\_DWORD \*\)p_z - 38\);[\s\S]*?if \( v57 > \*\(\(\_DWORD \*\)p_z - 38\) \)",
        layer, r"if \(capAlpha && alpha > alphaCap\)\s*\{\s*alpha = alphaCap"),
}

for name, (source_text, source_pattern, impl_text, impl_pattern) in checks.items():
    assert re.search(source_pattern, source_text, re.S), f"reference branch missing: {name}"
    assert re.search(impl_pattern, impl_text, re.S), f"implementation branch missing: {name}"
    print(f"PASS {name}")

assert source.count("CEffectPrim::RenderTeiRect(") == 4, "expected four source strip submissions in loop"
print("PASS source has four strip submissions inside its three-layer loop")
print(f"{len(checks) + 1} mechanical RenderDoubleGumGang comparisons passed")
