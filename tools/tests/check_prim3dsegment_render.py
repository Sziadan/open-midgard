#!/usr/bin/env python3
"""Source map for the PP_3DSPLINE/PP_3DSPIRAL segment ribbon renderer."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffectPrim.cpp").read_text(encoding="cp1252", errors="replace")
REF_DISPATCH = (ROOT / "Ref/dhxj/RagEffectPrim.cpp").read_text(encoding="cp1252", errors="replace")
CURRENT = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
FIXTURE = (ROOT / "tools/tests/ParticleRenderTests.cpp").read_text(encoding="utf-8")


def body(text: str, marker: str, start: int = 0) -> str:
    index = text.index(marker, start)
    opening = text.index("{", index)
    depth = 0
    for cursor in range(opening, len(text)):
        if text[cursor] == "{":
            depth += 1
        elif text[cursor] == "}":
            depth -= 1
            if depth == 0:
                return text[opening + 1:cursor]
    raise AssertionError(f"unclosed body: {marker}")


def check(label: str, condition: bool) -> None:
    assert condition, f"FAIL {label}"
    print(f"PASS {label}")


pe_sig = "void __thiscall CEffectPrim::Render3DSegment(CEffectPrim *this, matrix *wtm)"
pe_prototype = PE.index(pe_sig + "; // idb")
PE_RENDER = body(PE, pe_sig, pe_prototype + 1)
REF_RENDER = body(REF, "void __thiscall CEffectPrim::Render3DSegment(CEffectPrim *this, matrix *wtm)")
PE_RENDER_DISPATCH = body(PE, "void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm)",
                          PE.index("void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm); // idb") + 1)
REF_RENDER_DISPATCH = body(REF_DISPATCH, "void\tCEffectPrim :: Render(matrix &wtm/*view matrix*/)")
LOCAL_RENDER = body(CURRENT, "void CEffectPrim::Render(matrix* viewMatrix)")
LOCAL_SEGMENT = body(CURRENT, "void RenderReferenceSegment(")
LOCAL_SHIFT = body(CURRENT, "void ShiftReferenceSegments(")
LOCAL_INIT = body(CURRENT, "void InitReferenceSegments(")

check("canonical PE and primary Ref renderer bodies normalize identically",
      re.sub(r"\s+", " ", PE_RENDER).strip() == re.sub(r"\s+", " ", REF_RENDER).strip())
check("PE routes canonical primitive ids 0x0A and 0x0B to Render3DSegment",
      re.search(r"case\s+0xA\s*:\s*case\s+0xB\s*:\s*CEffectPrim::Render3DSegment\(this, wtm\)",
                PE_RENDER_DISPATCH) is not None)
check("primary Ref routes PP_3DSPLINE and PP_3DSPIRAL to Render3DSegment",
      re.search(r"case\s+PP_3DSPLINE\s*:\s*case\s+PP_3DSPIRAL\s*:\s*Render3DSegment\(wtm\)",
                REF_RENDER_DISPATCH) is not None)
check("local enum preserves primitive ids and Render dispatches both into segment renderer",
      re.search(r"PP_3DSPLINE\s*=\s*0xA", HEADER) is not None
      and re.search(r"PP_3DSPIRAL\s*=\s*0xB", HEADER) is not None
      and "if (m_type == PP_3DSPLINE || m_type == PP_3DSPIRAL)" in LOCAL_RENDER
      and "RenderReferenceSegment(*this, *viewMatrix);" in LOCAL_RENDER)

# p_argb-relative positions in PE are segment.segPos[0..3]; the primary
# Ref's Segments are populated from m_segPos in ShiftSegment/InitSegment.
pe_tokens = [
    "p_argb = &this->m_segment[0].argb", "src = *(vector3d *)(p_argb - 31)",
    "v46 = *(vector3d *)(p_argb - 28)", "v44 = *(vector3d *)(p_argb - 25)",
    "LODWORD(v47.x) = *(p_argb - 22)", "v45.z = v21", "v48.z = z",
    "*v13 = *v50->m_texture", "*p_tex = *v50->m_texture",
    "*p_tu = 0", "*v7 = 1065353216", "v52->tu = 0.0",
    "v17->color = *p_argb", "v22->color = *p_argb", "CEffectPrim::AddRP",
]
for token in pe_tokens[:-1]:
    check(f"PE source anchor: {token}", token in PE_RENDER)
check("PE renderer emits two AddRP triangle submissions per segment", PE_RENDER.count("CRenderer::AddRP") == 2)
check("primary Ref confirms the same face winding, UV progression and per-segment ARGB",
      all(token in REF_RENDER for token in ("m_rp[0].verts", "m_segment[0].argb",
          "*v13 = *v50->m_texture", "*p_tex = *v50->m_texture",
          "CRenderer::AddRP(g_renderer, v35, 1)", "CRenderer::AddRP(g_renderer, v36, 1)")))
check("segment history writes four ribbon points on init and every shift",
      "prim->m_segmentQuadPositions[static_cast<size_t>(index)]" in LOCAL_SHIFT
      and "= prim->m_segmentQuadPositions[static_cast<size_t>(index - 1)]" in LOCAL_SHIFT
      and "prim->m_segmentQuadPositions[0] = prim->m_segPos" in LOCAL_SHIFT
      and "prim->m_segmentQuadPositions[static_cast<size_t>(i)] = prim->m_segPos" in LOCAL_INIT)
check("local segment renderer selects current texture and source two-face vertex/UV order",
      "prim.m_texture[0]" in LOCAL_SEGMENT and re.search(r"prim\.m_curMotion", LOCAL_SEGMENT) is None
      and "corners[0], corners[1], corners[2]" in LOCAL_SEGMENT
      and "corners[3], corners[2], corners[0]" in LOCAL_SEGMENT
      and "{{0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f}}" in LOCAL_SEGMENT
      and "{{1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 1.0f}}" in LOCAL_SEGMENT
      and "SubmitReferenceEffectTriangle(firstFace" in LOCAL_SEGMENT
      and "SubmitReferenceEffectTriangle(secondFace" in LOCAL_SEGMENT)
check("focused fixture asserts two faces per segment, slot-zero texture, colors and UVs",
      all(token in FIXTURE for token in (
          "3D spline/spiral emits two ordered texture triangles for every history segment",
          "first segment face preserves source corners, color, slot-zero texture and UV order",
          "opposing segment face preserves source reversed corners and UV order",
          "each segment keeps its own packed color and legacy reference triangle material",
          "missing slot-zero segment texture emits no substitute geometry")))
print("PP_3DSPLINE/PP_3DSPIRAL segment ribbon comparator passed.")
