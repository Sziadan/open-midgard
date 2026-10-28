#!/usr/bin/env python3
"""Source audit for PE/Ref/current PP_STIN-family RenderSTIN parity."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SIGNATURE = "void __thiscall CEffectPrim::RenderSTIN(CEffectPrim *this, matrix *wtm)"
PE_RENDER = "void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm)"


def function_body(source: str, signature: str) -> str:
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 0
    for cursor in range(opening, len(source)):
        if source[cursor] == "{":
            depth += 1
        elif source[cursor] == "}":
            depth -= 1
            if depth == 0:
                return source[start:cursor + 1]
    raise AssertionError(f"unterminated function: {signature}")


def compact(source: str) -> str:
    return re.sub(r"\s+", "", source)


def main() -> int:
    ref = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="utf-8")
    ref_dispatch = (ROOT / "Ref/dhxj/RagEffectPrim.cpp").read_text(encoding="cp1252")
    exe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8")
    current = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
    header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
    render_tests = (ROOT / "tools/tests/ParticleRenderTests.cpp").read_text(encoding="utf-8")

    ref_render = function_body(ref, SIGNATURE)
    pe_render = function_body(exe, SIGNATURE)
    assert compact(ref_render) == compact(pe_render), "Ref RenderSTIN differs from canonical PE"
    for token in ("PP_STIN = 0x77", "PP_STIN2 = 0x7A", "PP_STIN4 = 0x7D"):
        assert token in header, f"primitive enum missing {token}"

    assert re.search(
        r"case\s+PP_STIN\s*:\s*case\s+PP_STIN2\s*:\s*case\s+PP_STIN4\s*:\s*RenderSTIN\(wtm\)\s*;\s*break\s*;",
        ref_dispatch,
    ), "Ref dispatcher no longer groups the STIN primitive family"
    pe_dispatch = function_body(exe, PE_RENDER)
    route = compact(pe_dispatch)
    assert compact("case 0x77: case 0x7A: case 0x7D: CEffectPrim::RenderSTIN(this, wtm); break;") in route

    local_dispatch = function_body(current, "void CEffectPrim::Render(matrix* viewMatrix)")
    assert "if (m_type == PP_STIN || m_type == PP_STIN2 || m_type == PP_STIN4)" in local_dispatch
    local_route_start = local_dispatch.index("if (m_type == PP_STIN || m_type == PP_STIN2 || m_type == PP_STIN4)")
    local_route_end = local_dispatch.index("if (m_type == PP_STIN5)", local_route_start)
    assert "RenderStin(*this, *viewMatrix);" in local_dispatch[local_route_start:local_route_end]

    # PE and Ref walk all four 180-byte GI records, gated by life==1 and
    # alphaB>0. riseAngle advances by 90 before each corner with one subtraction.
    for token in (
        "p_z = &this->m_GI[0].vecB_now.z;", "v12 = 4;", "*((_BYTE *)p_z - 140) == 1",
        "*((__int16 *)p_z - 69) > 0", "v3 = *((__int16 *)p_z - 7);",
        "v3 += 90;", "if ( v3 >= 360 )", "v3 -= 360;", "p_z += 45;", "while ( v12 );",
    ):
        assert compact(token) in compact(ref_render), f"PE/Ref lane/geometry branch missing: {token}"

    local = function_body(current, "void RenderStin(CEffectPrim& prim, const matrix& viewMatrix)")
    corners = function_body(current, "void StinCorners(const EffectGi& gi, vector3d corners[4])")
    for token in (
        "for (const EffectGi& gi : prim.m_gi)", "if (gi.life != 1 || gi.alphaB <= 0) continue;",
        "for (int index = 0; index < 4; ++index)", "int angle = gi.riseAngle;", "angle += 90;",
        "if (angle >= 360) angle -= 360;", "g_cosTable[angle]", "g_sinTable[angle]",
        "gi.distance", "gi.sparkX", "gi.sparkY", "gi.sparkZ",
    ):
        assert token in local + corners, f"local lane/geometry mapping missing: {token}"

    # Flag3 selector zero is blue; one is green; any other selector skips.
    for token in (
        "v10 = *((_BYTE *)p_z - 33);", "if ( v10 )", "if ( v10 == 1 )",
        "0x50u, 0xFFu, 0x50u", "0x50u, 0x50u, 0xFFu",
    ):
        assert compact(token) in compact(ref_render), f"PE/Ref color-selector path missing: {token}"
    for token in (
        "if (gi.flag[3] == 0)", "red = 80; green = 80; blue = 255;",
        "} else if (gi.flag[3] == 1)", "red = 80; green = 255; blue = 80;",
        "} else {", "continue;", "gi.flag[3]",
    ):
        assert token in local, f"local selector mapping missing: {token}"

    # The source RenderTeiRect calls preserve alphaB, fixed tint, PW/TN/F1,
    # full UVs and default primitive render flags. Local packs alpha/RGB and
    # submits the two source triangle orders through SubmitStinRect.
    source = compact(ref_render)
    assert source.count("CEffectPrim::RenderTeiRect(") == 2
    assert source.count(compact("*((_WORD *)p_z - 69)")) == 2
    for token in ("wtm, 0, 0, 1, 0x50u, 0xFFu, 0x50u, 0.0, 1.0, 0.0, 1.0", "wtm, 0, 0, 1, 0x50u, 0x50u, 0xFFu, 0.0, 1.0, 0.0, 1.0"):
        assert compact(token) in source, f"PE/Ref RenderTeiRect arguments missing: {token}"
    for token in (
        "static_cast<unsigned short>(gi.alphaB)", "(red << 16) | (green << 8) | blue",
        "SubmitStinRect(rpCount, corners, viewMatrix, texture, color, static_cast<int>(prim.m_renderFlag))",
    ):
        assert token in local, f"local alpha/tint/render-flag mapping missing: {token}"
    submit = function_body(current, "void SubmitStinRect(int& rpCount,")
    for token in (
        "const int triangles[2][3] = {{0,1,2},{3,2,0}};",
        "const float uvs[4][2] = {{0,1},{1,1},{1,0},{0,0}};",
        "D3DBLEND_SRCALPHA", "D3DBLEND_ONE", "ProjectReferenceEffectVertex",
        "g_renderer.AddRP(face, renderFlags);",
    ):
        assert token in submit, f"local submission mapping missing: {token}"

    fixture_start = render_tests.index("CEffectPrim stin;stin.m_pos={100,200,300};")
    fixture_end = render_tests.index("CEffectPrim stin5;", fixture_start)
    fixture = render_tests[fixture_start:fixture_end]
    for token in (
        "RenderStin(stin,identity);", "first.m_verts[0].color==0x405050FFu",
        "g_renderer.submissions[2].m_verts[0].color==0x7F50FF50u",
        "stin.m_texture.clear();", "s_dummy_texture", "life=2", "flag[3]=2",
        "near(g_renderer.submissions[2].m_verts[0].x",
    ):
        assert token in fixture, f"existing ParticleRenderTests fixture omits: {token}"

    print("PASS RenderSTIN primary Ref/PE body identity and three-ID dispatch parity")
    print("PASS four GI lanes, life/alpha gates, +90 one-step angle wrap, distance and sparkXYZ geometry")
    print("PASS selector 0/1 RGB branches and unsupported-selector skip")
    print("PASS alphaB, PW/TN/F1, full UV, triangle order, additive blend and render flags map to local helper")
    print("PASS existing ParticleRenderTests fixture covers geometry, tints, gates and missing/dummy textures")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
