#!/usr/bin/env python3
"""Mechanical branch comparison for the Intimidate renderer port."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
REF = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
IMPL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")


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
    raise AssertionError(f"unclosed body: {marker}")


ref = body(REF, "void __thiscall CEffectPrim::RenderIntimidate(CEffectPrim *this, matrix *wtm)")
impl = "\n".join(body(IMPL, marker) for marker in (
    "void IntimidateTowardScreen(", "void IntimidateCorners(",
    "vector3d IntimidateCenter(", "void RenderIntimidate("))
checks = {
    "all four source GI bands": (r"p_z \+= 45;\s*--v39;\s*\}\s*while \( v39 \)", r"band < 4"),
    "live and positive-alpha gate": (r"p_z - 140\) == 1 && .*p_z - 69\) > 0", r"gi\.life != 1 \|\| gi\.alphaB <= 0"),
    "distance/full-display center X": (r"g_cosTable\[\*\(\(__int16 \*\)p_z - 67\)\] \* v3", r"g_cosTable\[gi\.fullDisplayAngle\]\) \* gi\.distance"),
    "full-display center Y": (r"g_sinTable\[\*\(\(__int16 \*\)p_z - 67\)\].*?\* v3", r"g_sinTable\[gi\.fullDisplayAngle\]\) \* gi\.distance"),
    "rise rotation for center X/Z": (r"g_cosTable\[\*\(\(__int16 \*\)p_z - 7\)\].*?g_sinTable\[\*\(\(__int16 \*\)p_z - 7\)\]", r"g_cosTable\[gi\.riseAngle\].*?g_sinTable\[gi\.riseAngle\]"),
    "radius from height[1] and angle from height[2]": (r"v8 = \(__int64\)\*\(p_z - 28\).*?g_cosTable\[v7\] \* \*\(p_z - 29\)", r"static_cast<int>\(gi\.height\[2\]\).*?g_cosTable\[angle\].*?gi\.height\[1\]"),
    "four quarter-turn corners": (r"v7 \+ 90", r"angle \+= 90"),
    "source single-subtract angle wrap": (r"if \( v7 \+ 90 >= 360 \).*?v12 = v7 - 270", r"if \(angle >= 360\)\s*\{\s*angle -= 360;"),
    "flag-2 screen-facing branch": (r"if \( \(_BYTE\)v8 == 2 \)", r"if \(gi\.flag\[0\] == 2\)"),
    "flat flag-2 ring": (r"vec1\.y = 0\.0;.*?vec4\.y = 0\.0;", r"corners\[i\]\.y = 0\.0f"),
    "screen-facing uses GI0/GI1 alphaT": (r"alphaT = v40->m_GI\[1\]\.alphaT;.*?v30 = v40->m_GI\[0\]\.alphaT;.*?Tei_TowardScreen\(v40,.*?v30, alphaT", r"IntimidateTowardScreen\(corners, prim\.m_gi\[0\]\.alphaT, prim\.m_gi\[1\]\.alphaT\)"),
    "add transformed center and source origin": (r"vec1\.x = vec1\.x \+ vecM \+ \*\(p_z - 2\).*?vec4\.z = vec4\.z \+ \*p_z", r"corner\.x = static_cast<float>\(static_cast<double>\(corner\.x\) \+ center\.x \+ gi\.sparkX\);.*?corner\.y = static_cast<float>\(static_cast<double>\(corner\.y\) \+ center\.y\);.*?corner\.y = static_cast<float>\(static_cast<double>\(corner\.y\) \+ gi\.sparkY\);.*?corner\.z = static_cast<float>\(static_cast<double>\(corner\.z\) \+ center\.z\);.*?corner\.z = static_cast<float>\(static_cast<double>\(corner\.z\) \+ gi\.sparkZ\);"),
    "flag-zero cyan tint": (r"0x64u,\s*0x96u,\s*0xFFu", r"case 0: tint = 0x006496FFu"),
    "flag-one yellow tint": (r"0xFAu,\s*0xFAu,\s*0x9Bu", r"case 1: tint = 0x00FAFA9Bu"),
    "flag-two red tint": (r"0xFAu,\s*0x64u,\s*0x64u", r"case 2: tint = 0x00FA6464u"),
    "band alpha preserved": (r"\*\(\(_WORD \*\)p_z - 69\)", r"static_cast<unsigned short>\(gi\.alphaB\)"),
    "four-corner rect and 0..1 UVs": (r"vec1,\s*vec2,\s*vec3,\s*vec4,\s*wtm,\s*0,\s*0,\s*1,", r"corners\[0\], corners\[1\], corners\[2\], corners\[3\].*?0\.0f, 1\.0f, 1\.0f, 0\.0f"),
}
for name, (rp, ip) in checks.items():
    assert re.search(rp, ref, re.S), f"reference pattern missing: {name}"
    assert re.search(ip, impl, re.S), f"implementation pattern missing: {name}"
    print(f"PASS {name}")

toward_ref = body(REF, "void __thiscall CEffectPrim::Tei_TowardScreen(")
toward_impl = body(IMPL, "void IntimidateTowardScreen(")
for name, pattern in (
    ("Y/Z rotation formulas", r"z \* g_cosTable\[Rot1\] - y \* g_sinTable\[Rot1\]"),
    ("X/Z rotation formulas", r"x \* g_cosTable\[Rot0\] - v16 \* g_sinTable\[Rot0\]"),
):
    assert re.search(pattern, toward_ref, re.S), f"reference TowardScreen missing {name}"
assert "corners[i].z = static_cast<float>(z * g_cosTable[rotationYZ] - y * g_sinTable[rotationYZ])" in toward_impl
assert "corners[i].x = static_cast<float>(x * g_cosTable[rotationX] - z * g_sinTable[rotationX])" in toward_impl
print("PASS TowardScreen Y/Z then X/Z transform order")

dispatch = body(EXE, "void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm)\n")
assert re.search(r"case 0x2B:\s*CEffectPrim::RenderIntimidate", dispatch)
assert re.search(r"m_type == PP_INTIMIDATE\)\s*\{\s*RenderIntimidate", IMPL)
print("PASS source case 0x2B maps to PP_INTIMIDATE")
print(f"{len(checks) + 2} mechanical source/implementation branch comparisons passed")
