#!/usr/bin/env python3
"""Branch-level comparison for canonical PP_3DTEXTURE_NOMASTER rendering."""
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
    header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
    tests = (ROOT / "tools/tests/ParticleRenderTests.cpp").read_text(encoding="utf-8")

    signature = "void __thiscall CEffectPrim::Render3DTextureBillboard(CEffectPrim *this, matrix *wtm)"
    ref_render = function_body(ref, signature)
    exe_render = function_body(exe, signature)
    assert compact(ref_render) == compact(exe_render), "Ref billboard renderer differs from canonical executable"
    assert "PP_3DTEXTURE_NOMASTER = 0x16" in header

    public = function_body(exe, "void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm)")
    start = public.index("case 0x16:")
    end = public.index("case 0x17:", start)
    assert "Render3DTextureBillboard(this, wtm)" in public[start:end]

    current_render = function_body(current, "void CEffectPrim::Render(matrix* viewMatrix)")
    master_guard = current_render.split("if ((!m_master", 1)[1].split("|| !viewMatrix", 1)[0]
    assert "m_type != PP_3DTEXTURE_NOMASTER" in master_guard and "m_type != PP_MAPPARTICLE" in master_guard, \
        "masterless renderer types are rejected before their specialized routes"
    route = "if (m_type == PP_3DTEXTURE_NOMASTER)"
    assert route in current_render, "type0x16 billboard route is absent"
    start = current_render.index(route)
    end = current_render.index("if (m_type == PP_STIN", start)
    assert "RenderReference3DTextureBillboard(*this, *viewMatrix);" in current_render[start:end]
    assert start < current_render.index("switch (m_type)")

    helper = function_body(current, "void RenderReference3DTextureBillboard(const CEffectPrim& prim, const matrix& view)")
    ordered = (
        "const vector3d& point = prim.m_pos;",
        "const double reciprocal = 1.0 /",
        "const float oow = static_cast<float>(reciprocal);",
        "* reciprocal * g_renderer.m_hpc + g_renderer.m_xoffset",
        "* g_renderer.m_vpc * oow + g_renderer.m_yoffset",
        "static_cast<double>(oow) * g_renderer.m_hpc * 0.14285715",
        "pixelRatio * prim.m_widthSize",
        "pixelRatio * prim.m_heightSize",
        "prim.m_texture[static_cast<size_t>(prim.m_curMotion)]",
        "{{x,y},{right,y},{x,bottom},{right,bottom}}",
        "{{0,0},{1,0},{0,1},{1,1}}",
        "{{0,1,2},{1,2,3}}",
        "output.z = 0.80000001f; output.oow = 1.0f;",
        "output.color = prim.m_argb",
        "g_renderer.AddRP(face, static_cast<int>(prim.m_renderFlag))",
    )
    cursor = -1
    for token in ordered:
        cursor = helper.find(token, cursor + 1)
        assert cursor >= 0, f"current billboard helper omits or reorders source operation: {token}"

    for token in (
        "3D texture billboard submits two ordered faces through legacy AddRP with raw render flag five",
        "3D texture billboard projects source anchor and preserves the two-triangle top-left quad order",
        "3D texture billboard retains fixed source depth, reciprocal depth, color and complete quad UVs",
    ):
        assert token in tests, f"missing production-body billboard test: {token}"

    print("Render3DTextureBillboard comparison passed: EXE/Ref body, type0x16 route, masterless dispatch, ordered two-face geometry, and production-body checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
