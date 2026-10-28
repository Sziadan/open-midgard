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
    for token in (
        "vert.specular = -16777216;",
        "v14.specular = -16777216;",
        "this->m_rp[0].tex = 0;",
        "CRenderer::AddRP(g_renderer, this->m_rp, 5);",
    ):
        assert token in ref_render, f"canonical line source omits required render state: {token}"

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
        "face->srcAlphaMode = D3DBLEND_SRCALPHA",
        "face->destAlphaMode = D3DBLEND_INVSRCALPHA",
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
    assert "top.specular = 0xFF000000u" in helper and "lower.specular = 0xFF000000u" in helper

    for token in (
        "3D line submits exactly one source alpha-operation face with flag five",
        "3D line uses top/top-plus-one/lower projected vertex order and source ten-unit world offset",
        "3D line preserves fixed source colors, reciprocal-depth values and zero UVs",
    ):
        assert token in tests, f"missing production-body render test: {token}"
    assert "opaque specular, inverse-alpha blend and source triangle state" in tests

    renderer = (ROOT / "src/render/Renderer.cpp").read_text(encoding="utf-8")
    add_triangle = function_body(renderer, "void CRenderer::AddReferenceEffectTriangle(RPFace* face, int renderFlag)")
    assert "if ((renderFlag & 4) != 0)" in add_triangle
    assert "m_rpAlphaOPList.push_back(face);" in add_triangle

    print("Render3DLine comparison passed: EXE/Ref body, type0x14 route, ordered projected face construction, and production-body checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
