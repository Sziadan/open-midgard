#!/usr/bin/env python3
"""Source audit for PE/Ref/current PP_STIN5 RenderSTIN5 parity."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SIGNATURE = "void __thiscall CEffectPrim::RenderSTIN5(CEffectPrim *this, matrix *wtm)"
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
    assert compact(ref_render) == compact(pe_render), "Ref RenderSTIN5 differs from canonical PE"
    assert "PP_STIN5 = 0x7E" in header
    assert re.search(r"case\s+PP_STIN5\s*:\s*RenderSTIN5\(wtm\)\s*;\s*break\s*;", ref_dispatch)

    pe_dispatch = function_body(exe, PE_RENDER)
    pe_start = pe_dispatch.index("case 0x7E:")
    pe_end = pe_dispatch.index("default:", pe_start)
    assert "CEffectPrim::RenderSTIN5(this, wtm)" in pe_dispatch[pe_start:pe_end]
    local_dispatch = function_body(current, "void CEffectPrim::Render(matrix* viewMatrix)")
    local_start = local_dispatch.index("if (m_type == PP_STIN5)")
    local_end = local_dispatch.index("if (m_type == PP_SMA)", local_start)
    assert "RenderStin5(*this, *viewMatrix);" in local_dispatch[local_start:local_end]

    # Four GI records, active only at exact life one and positive alpha.
    for token in (
        "p_z = &this->m_GI[0].vecB_now.z;", "v16 = 4;",
        "*((_BYTE *)p_z - 140) == 1", "*((__int16 *)p_z - 69) > 0",
        "p_z += 45;", "while ( v16 );",
    ):
        assert compact(token) in compact(ref_render), f"PE/Ref lane gate missing: {token}"
    local = function_body(current, "void RenderStin5(CEffectPrim& prim, const matrix& viewMatrix)")
    corners = function_body(current, "void Stin5Corners(const EffectGi& gi, vector3d corners[4])")
    assert "for (const EffectGi& gi : prim.m_gi)" in local
    assert "if (gi.life != 1 || gi.alphaB <= 0) continue;" in local

    # Source adds 90 before each corner and performs a single wrap subtraction.
    for token in ("v4 = *((__int16 *)p_z - 7);", "v4 += 90;", "if ( v4 >= 360 )", "v4 -= 360;"):
        assert compact(token) in compact(ref_render), f"PE/Ref angle step missing: {token}"
    assert "int angle = gi.riseAngle;" in corners and "angle += 90;" in corners
    assert "if (angle >= 360) angle -= 360;" in corners

    # v15 is 4*RotStart in bytes. PE dereferences float trig-table bases plus
    # that displacement; CreateTrigonometricTable initializes degree-indexed
    # float elements, so local TableAngle/array indexing uses the RotStart
    # element itself rather than a 4x angle.
    for token in (
        "v15 = 4 * *((__int16 *)p_z - 62);",
        "v5 = (float *)(4 * v4 + 7522556);",
        "v6 = (float *)(4 * v4 + 7525444);",
        "*(float *)(v15 + 7525444)", "*(float *)(v8 + 7522556)",
    ):
        assert compact(token) in compact(ref_render), f"PE/Ref table path missing: {token}"
    trig_init = function_body(exe, "void __cdecl CreateTrigonometricTable()")
    assert compact("v1 = (double)i * 3.141592 * 0.0055555557;") in compact(trig_init)
    assert "g_sinTable[v0] = ib;" in trig_init and "g_cosTable[v0] = v3;" in trig_init
    for token in (
        "g_cosTable[angle]", "g_sinTable[angle]", "g_cosTable[gi.rotStart]",
        "g_sinTable[gi.rotStart]", "gi.distance",
        "corners[index].x", "corners[index].y", "corners[index].z",
        "gi.sparkX", "gi.sparkY", "gi.sparkZ",
    ):
        assert token in corners, f"local distance/rotation/origin mapping missing: {token}"

    # Source coordinates: radial cos(angle)*distance is rotated by RotStart;
    # sin(angle)*distance supplies Y. Fixed green tint, alphaB, PW/TN 0,
    # F1=1, full UV and primitive render flags are preserved by shared submit.
    for token in (
        "v9 = *v6 * v3;", "*(v7 - 5) = v9 * *(float *)(v15 + 7525444);",
        "v10 = v9 * *(float *)(v8 + 7522556);", "*(v7 - 4) = *v5 * v3;",
        "0x50u, 0xFFu, 0x50u", "0.0, 1.0, 0.0, 1.0",
    ):
        assert compact(token) in compact(ref_render), f"PE/Ref geometry/submission missing: {token}"
    assert "(80u << 16) | (255u << 8) | 80u" in local
    assert "static_cast<unsigned short>(gi.alphaB)" in local
    assert "SubmitStinRect(rpCount, corners, viewMatrix, texture, color, static_cast<int>(prim.m_renderFlag))" in local
    submit = function_body(current, "void SubmitStinRect(int& rpCount,")
    for token in (
        "const int triangles[2][3] = {{0,1,2},{3,2,0}};",
        "const float uvs[4][2] = {{0,1},{1,1},{1,0},{0,0}};",
        "D3DBLEND_SRCALPHA", "D3DBLEND_ONE", "g_renderer.AddRP(face, renderFlags);",
    ):
        assert token in submit, f"local source submission mapping missing: {token}"

    fixture_start = render_tests.index("CEffectPrim stin5;")
    fixture_end = render_tests.index("CEffectPrim sma2;", fixture_start)
    fixture = render_tests[fixture_start:fixture_end]
    for token in (
        "RenderStin5(stin5,identity);", "for(int i=0;i<4;++i)",
        "GetCos(active.rotStart)*radial", "GetSin(active.rotStart))*radial",
        "GetSin(angle))*active.distance", "0x9650FF50u", "0x1450FF50u",
        "D3DBLEND_SRCALPHA", "D3DBLEND_ONE", "g_renderer.flags[0]==0x205",
        "active.riseAngle==359&&active.rotStart==90",
    ):
        assert token in fixture, f"existing ParticleRenderTests fixture lacks: {token}"

    print("PASS RenderSTIN5 primary Ref/PE normalized body identity and dispatch routes")
    print("PASS four GI gates, riseAngle +90 wrap, radius/RotStart element lookup and sparkXYZ")
    print("PASS RotStart 4x byte offset selects the degree-table element; trig initialization confirms table indexing")
    print("PASS fixed green tint/alpha, PW/TN/F1, full UV, shared triangles/blend/texture/render flags")
    print("PASS existing ParticleRenderTests fixture covers rotated geometry, wrap, tint and submission")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
