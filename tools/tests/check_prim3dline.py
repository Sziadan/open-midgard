#!/usr/bin/env python3
"""Branch-level comparison for canonical PP_3DLINE rendering."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def function_body(source: str, signature: str) -> str:
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 1
    cursor = opening + 1
    while depth:
        depth += (source[cursor] == "{") - (source[cursor] == "}")
        cursor += 1
    return source[start:cursor]


def compact(source: str) -> str:
    return re.sub(r"\s+", "", source)


def main() -> int:
    ref = (ROOT / "Ref/RagEffectPrim.cpp").read_text(encoding="utf-8")
    exe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8")
    current = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
    tests = (ROOT / "tools/tests/ParticleRenderTests.cpp").read_text(encoding="utf-8")

    signature = "void __thiscall CEffectPrim::Render3DLine(CEffectPrim *this, matrix *wtm)"
    ref_render = function_body(ref, signature)
    exe_render = function_body(exe, signature)
    assert compact(ref_render) == compact(exe_render), "Ref Render3DLine body differs from canonical executable"

    public_signature = "void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm)"
    exe_dispatch = function_body(exe, public_signature)
    line_case_start = exe_dispatch.index("case 0x14:")
    line_case_end = exe_dispatch.index("case 0x16:", line_case_start)
    assert "Render3DLine(this, wtm)" in exe_dispatch[line_case_start:line_case_end]

    current_dispatch = function_body(current, "void CEffectPrim::Render(matrix* viewMatrix)")
    route = "if (m_type == PP_3DLINE)"
    assert route in current_dispatch, "PP_3DLINE is missing from current render dispatch"
    route_start = current_dispatch.index(route)
    route_end = current_dispatch.index("if (m_type == PP_STIN", route_start)
    assert "RenderReference3DLine(*this, *viewMatrix);" in current_dispatch[route_start:route_end]
    assert route_start < current_dispatch.index("switch (m_type)")

    helper = function_body(current, "void RenderReference3DLine(const CEffectPrim& prim, const matrix& view)")
    ordered = (
        "ProjectReferenceEffectVertex(prim.m_pos, view, top);",
        "lowerPosition.y -= 10.0f;",
        "ProjectReferenceEffectVertex(lowerPosition, view, lower);",
        "g_renderer.BorrowNullRP()",
        "face->numVerts = 3",
        "face->tex = nullptr",
        "face->m_verts[0] = top;",
        "face->m_verts[1] = top;",
        "face->m_verts[2] = lower;",
        "static_cast<double>(top.x) + 1.0",
        "face->m_verts[index].oow = 0.99999899f;",
        "face->m_verts[index].tu = 0.0f;",
        "face->m_verts[index].tv = 0.0f;",
        "face->m_verts[0].color = 0x9DFFFFFFu;",
        "face->m_verts[1].color = 0x9BFFFFFFu;",
        "face->m_verts[2].color = 0x9BFFFFFFu;",
        "AddReferenceEffectTriangle(face,5)",
    )
    cursor = -1
    for token in ordered:
        cursor = helper.find(token, cursor + 1)
        assert cursor >= 0, f"current line renderer omits or reorders source operation: {token}"

    for token in (
        "3D line submits exactly one source alpha-operation face with flag five",
        "3D line uses top/top-plus-one/lower projected vertex order and source ten-unit world offset",
        "3D line preserves fixed source colors, reciprocal-depth values and zero UVs",
    ):
        assert token in tests, f"missing production-body render test: {token}"

    print("Render3DLine comparison passed: EXE/Ref body, type0x14 route, ordered projected face construction, and production-body checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
