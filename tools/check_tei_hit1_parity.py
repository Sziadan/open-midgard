#!/usr/bin/env python3
"""Mechanical source/implementation branch checks for RenderTeiHit1."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
REF = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
REF_PRIM = (ROOT / "Ref/RagEffectPrim.cpp").read_text(encoding="utf-8", errors="replace")
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
    raise AssertionError(f"unclosed function: {marker}")


ref = body(REF, "void __thiscall CEffectPrim::RenderTeiHit1(CEffectPrim *this, matrix *wtm)")
impl = "\n".join(body(IMPL, marker) for marker in (
    "void TeiHit1Corners(", "void RenderTeiHit1(", "void SubmitTeiHit1Rect("))
checks = {
    "four GI iterations": (r"v2 \+= 180;\s*--v33;\s*\}\s*while \( v33 \)", r"band < 4"),
    "life and positive alpha gates": (r"\*\(v2 - 105\) == 1.*?v3 > 0", r"gi\.life != 1 \|\| gi\.alphaB <= 0"),
    "alphaB retained": (r"v3 = \*\(_WORD \*\)\(v2 - 103\)", r"static_cast<unsigned short>\(gi\.alphaB\)"),
    "full-display and rise table angles": (r"v4 = 4 \* \*\(__int16 \*\)\(v2 - 99\).*?v6 = 4 \* \*\(__int16 \*\)\(v2 \+ 21\)", r"gi\.fullDisplayAngle.*?gi\.riseAngle"),
    "flag-one signed distance offsets": (r"double\)v31 \* \*\(float \*\)\(v2 - 81\) \+ \*\(float \*\)\(v2 \+ 23\).*?double\)-v31 \* \*\(float \*\)\(v2 - 81\) \+ \*\(float \*\)\(v2 \+ 23\)", r"static_cast<double>\(gi\.flag\[1\]\).*?static_cast<double>\(-static_cast<int>\(gi\.flag\[1\]\)\)"),
    "perpendicular height offsets and four corners": (r"vec1a = .*? - .*?\* v7;.*?vec2a = .*? - .*?\* v13;.*?vec3a = .*? - .*?\* \(v13 \* -1\.0\).*?vec4a = .*?\* v5 - .*?\* v32", r"leftX = cosineFull \* offset - sineFull \* height;.*?rightX = cosineFull \* rightOffset - sineFull \* height;.*?rightLowerX = cosineFull \* rightOffset \+ sineFull \* height;.*?leftLowerX = cosineFull \* offset \+ sineFull \* height"),
    "source vertex order vec4, vec1, vec2, vec3": (r"vec4,\s*vec1,\s*vec2,\s*vec3,\s*wtm", r"xValues\[4\]\{leftLowerX, leftX, rightX, rightLowerX\}.*?positions\[2\]\[3\].*?corners\[0\].*?corners\[1\].*?corners\[2\].*?corners\[3\]"),
    "flag-zero tint and frame": (r"0xE1u,\s*0xE1u,\s*0x50u", r"0x00E1E150u"),
    "flag-one white tint/frame zero": (r"v18 == 1.*?v22 = -1;\s*v21 = -1;\s*v20 = 0", r"case 1:\s*tint = 0x00FFFFFFu"),
    "other nonzero flag tint/frame one": (r"v22 = -56;\s*v21 = -56;\s*v20 = 1", r"texturePart = 1;\s*tint = 0x00C8C8FFu"),
    "primitive origin translation and dedicated overload": (r"wtm,\s*v20,\s*0,\s*0,.*?0\.0,\s*1\.0,\s*0\.0,\s*1\.0", r"SubmitTeiHit1Rect\(corners, prim\.m_pos"),
    "source V frame coefficient": (r"0\.047619049", r"0\.047619049"),
    "additive destination mode": (r"if \( PW \).*?destAlphaMode = 6;.*?destAlphaMode = 2", r"face->destAlphaMode = D3DBLEND_ONE"),
}
for name, (rp, ip) in checks.items():
    source_body = REF_PRIM if name in ("source V frame coefficient", "additive destination mode") else ref
    assert re.search(rp, source_body, re.S), f"reference mismatch: {name}"
    assert re.search(ip, impl, re.S), f"implementation mismatch: {name}"
    print(f"PASS {name}")

assert re.search(r"verts\[0\]\.tu = 1\.0;.*?verts\[0\]\.tv = v19", REF_PRIM, re.S)
assert re.search(r"verts\[1\]\.tu = 1\.0;.*?verts\[1\]\.tv = Tx2", REF_PRIM, re.S)
assert re.search(r"verts\[2\]\.tu = 0\.0;.*?verts\[2\]\.tv = Tx2", REF_PRIM, re.S)
assert "{ { 1.0f, frame0 }, { 1.0f, frame1 }, { 0.0f, frame1 } }" in impl
assert "{ { 0.0f, frame0 }, { 0.0f, frame1 }, { 1.0f, frame0 } }" in impl
print("PASS exact source frame V and fixed-U triangle UV layout")

dispatch = body(EXE, "void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm)\n")
assert re.search(r"case 0x2C:\s*CEffectPrim::RenderTeiHit1", dispatch)
assert re.search(r"m_type == PP_TEIHIT1\)\s*\{\s*RenderTeiHit1", IMPL)
print("PASS source case 0x2C maps to PP_TEIHIT1")
print(f"{len(checks) + 2} mechanical source/implementation branch comparisons passed")
