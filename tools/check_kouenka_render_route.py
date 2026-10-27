#!/usr/bin/env python3
"""Mechanically compare the PP_KOUENKA Render3DParticle route and core branches."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
REF = (ROOT / "Ref/dhxj/RagEffectPrim.cpp").read_text(encoding="utf-8", errors="replace")
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


ref_dispatch = body(REF, "void\tCEffectPrim :: Render(matrix &wtm/*view matrix*/)")
canonical = body(CANON, "void __thiscall CEffectPrim::Render3DParticle(CEffectPrim *this, matrix *wtm)\n{")
impl = body(IMPL, "void RenderParticleSprites(const CEffectPrim& prim,")
predicate = body(IMPL, "bool CEffectPrim::UsesReferenceParticle() const")
render = body(IMPL, "void CEffectPrim::Render(matrix* viewMatrix)")

checks = {
    "readable dispatcher groups PP_KOUENKA with Render3DParticle":
        re.search(r"case\s+PP_KOUENKA\s*:[\s\S]*?Render3DParticle\(wtm\)", ref_dispatch) is not None,
    "canonical body retains sprite/action, segment, and clip iteration":
        all(re.search(pattern, canonical, re.S) for pattern in (r"GetSprRes", r"GetActRes", r"m_numSegment", r"numClips", r"GetClip")),
    "canonical body retains edge viewport tests and clip orientation":
        all(re.search(pattern, canonical, re.S) for pattern in (r"m_height", r"m_width", r"Clip->flags & 1", r"Clip->zoomx", r"Clip->zoomy")),
    "canonical body retains source clip-color selector, angle, and render flag":
        all(re.search(pattern, canonical, re.S) for pattern in (r"m_lastTlvertY & 0x200", r"Clip->angle", r"m_isLieOnGround")),
    "current shared particle compositor maps segment/clip loops and source color branch":
        all(re.search(pattern, impl, re.S) for pattern in (r"prim\.m_numSegments", r"motion->sprClips", r"clip\.flags & 1", r"prim\.m_pattern & 0x200", r"clip\.angle", r"prim\.m_renderFlag")),
    "current PP_KOUENKA predicate selects the shared reference renderer":
        re.search(r"m_type == PP_KOUENKA", predicate) is not None and re.search(r"UsesReferenceParticle\(\)[\s\S]*?RenderReferenceParticlePrimitive", render) is not None,
}

for label, passed in checks.items():
    assert passed, f"source-to-implementation comparison failed: {label}"
    print(f"PASS {label}")

print(f"{len(checks)} mechanical PP_KOUENKA renderer comparisons passed")
