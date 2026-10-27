#!/usr/bin/env python3
"""Compare source and current PP_BASH3D render branches mechanically."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
REF = (ROOT / "Ref/dhxj/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
CANON = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
IMPL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")


def body(text: str, marker: str) -> str:
    start = text.index(marker)
    opening = text.index("{", start)
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{": depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[opening + 1:pos]
    raise AssertionError(f"unclosed body: {marker}")


ref = body(REF, "void\tCEffectPrim :: RenderBash3D(matrix &wtm)")
canonical = body(CANON, "void __thiscall CEffectPrim::RenderBash3D(CEffectPrim *this, matrix *wtm)\n{")
ref_rect = body(REF, "void\tCEffectPrim :: RenderTeiRect(int *rpCnt, short alpha,")
canonical_rect = body(CANON, "void __thiscall CEffectPrim::RenderTeiRect(\n        CEffectPrim *this,\n        int *rpCnt,\n        __int16 alpha,\n        vector3d vec1,")
impl = body(IMPL, "void RenderBash3D(CEffectPrim& prim,")
submit = body(IMPL, "void SubmitBash3dRect(")
dispatch = body(IMPL, "void CEffectPrim::Render(matrix* viewMatrix)")

checks = {
    "four GI lanes and life/positive-alpha gates": (
        re.search(r"for\s*\(ec=0\s*;\s*ec<4", ref) and re.search(r"life == 1 && m_GI\[ec\]\.alphaB > 0", ref),
        re.search(r"for \(const EffectGi& gi : prim\.m_gi\)", impl) and re.search(r"gi\.life != 1 \|\| gi\.alphaB <= 0", impl)),
    "mode 2 narrow and wide spreads wrap at 360 with exact X/Z radius": (
        re.search(r"rise_angle - 1[\s\S]*?Angle<0[\s\S]*?rise_angle \+ 1[\s\S]*?Angle>=360", ref),
        re.search(r"gi\.riseAngle - spread[\s\S]*?lowAngle < 0[\s\S]*?gi\.riseAngle \+ spread[\s\S]*?highAngle >= 360", impl)),
    "mode 2-5 plus default narrow and wide color/PW branches": (
        all(re.search(rf"height\[4\]\s*==\s*{value}", ref) for value in (2, 3, 4, 5))
        and ref.count("RenderTeiRect(") == 10,
        all(token in impl for token in ("mode == 2.0f", "mode == 3.0f", "mode == 4.0f", "mode == 5.0f",
            "submitPass(narrowSpread, 0, 250, 250, 0)", "submitPass(wideSpread, 250, 0, 0, 0)"))
        and "D3DBLEND_INVSRCALPHA" in submit),
    "canonical executable contains four active-lane mode branches and delegated rect submissions": (
        all(token in canonical for token in ("v5 <= 0", "case 0x40400000", "case 0x40800000", "case 0x40A00000", "RenderTeiRect(")),
        all(token in impl for token in ("if (mode == 2.0f", "else if (mode == 3.0f", "else if (mode == 5.0f"))),
    "primitive-position translation, texture0, UVs, blend, flags, and render route": (
        re.search(r"m_texture\[TN\][\s\S]*?if \( F1 == 0 \)[\s\S]*?vec1 \+= m_pos[\s\S]*?destAlphaMode", REF),
        re.search(r"prim\.m_texture\[0\][\s\S]*?prim\.m_pos[\s\S]*?prim\.m_renderFlag", impl)
        and re.search(r"uvs\[4\][\s\S]*?D3DBLEND_SRCALPHA", submit)
        and re.search(r"m_type == PP_BASH3D\)\s*\{\s*RenderBash3D", dispatch)),
    "canonical helper has no readable-source texture early return": (
        "m_texture[TN] == NULL" in ref_rect and "if ( *rpCnt < 510 )" in canonical_rect,
        "if (!texture" not in submit and "if (rpCount >= 510)" in submit),
}

for label, (source_match, implementation_match) in checks.items():
    assert source_match, f"reference comparison failed: {label}"
    assert implementation_match, f"implementation comparison failed: {label}"
    print(f"PASS {label}")
print(f"{len(checks)} mechanical BASH3D branch comparisons passed")
