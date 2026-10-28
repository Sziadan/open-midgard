#!/usr/bin/env python3
"""Source audit for PE/Ref/current PP_AIRTEXTURE rendering parity."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SIGNATURE = "void __thiscall CEffectPrim::RenderAirTexture(CEffectPrim *this, matrix *wtm)"


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
    ref_dispatch_source = (ROOT / "Ref/dhxj/RagEffectPrim.cpp").read_text(
        encoding="cp1252"
    )
    exe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8")
    current = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
    header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
    render_tests = (ROOT / "tools/tests/ParticleRenderTests.cpp").read_text(encoding="utf-8")

    ref_render = function_body(ref, SIGNATURE)
    exe_render = function_body(exe, SIGNATURE)
    assert compact(ref_render) == compact(exe_render), (
        "Ref RenderAirTexture differs from canonical executable"
    )
    assert "PP_AIRTEXTURE = 0x62" in header

    assert re.search(
        r"case\s+PP_AIRTEXTURE\s*:[^\n]*\n\s*RenderAirTexture\(wtm\)\s*;\s*break\s*;",
        ref_dispatch_source,
    ), "Ref Render dispatcher lost the PP_AIRTEXTURE route"
    exe_dispatch = function_body(
        exe, "void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm)"
    )
    route_start = exe_dispatch.index("case 0x62:")
    route_end = exe_dispatch.index("case 0x63:", route_start)
    assert "RenderAirTexture(this, wtm)" in exe_dispatch[route_start:route_end]

    local_dispatch = function_body(current, "void CEffectPrim::Render(matrix* viewMatrix)")
    local_route = "if (m_type == PP_AIRTEXTURE)"
    assert local_route in local_dispatch, "local PP_AIRTEXTURE route is missing"
    route_start = local_dispatch.index(local_route)
    route_end = local_dispatch.index("if (m_type == PP_", route_start + len(local_route))
    assert "RenderAirTexture(*this, *viewMatrix);" in local_dispatch[route_start:route_end]
    assert route_start < local_dispatch.index("switch (m_type)")

    local = function_body(current, "void RenderAirTexture(const CEffectPrim& prim, const matrix& viewMatrix)")
    # Four GI slots are visited in order; only live lanes with positive alpha
    # emit. Source p_z += 45 is one 180-byte GI record.
    assert compact("p_z = &this->m_GI[0].vecB_now.z;") in compact(ref_render)
    assert compact("v13 = 4;") in compact(ref_render)
    assert compact("if (*((_BYTE *)p_z - 140) == 1 && *((__int16 *)p_z - 69) > 0)") in compact(ref_render)
    assert compact("p_z += 45;") in compact(ref_render)
    assert compact("--v13;") in compact(ref_render)
    assert compact("while (v13);") in compact(ref_render)
    assert "for (const EffectGi& gi : prim.m_gi)" in local
    assert "if (gi.life != 1 || gi.alphaB <= 0)" in local

    # Source walks four corners at 90-degree intervals, wrapping the table
    # pointer after 360 degrees. The arrays are 1-degree tables; the source's
    # 4*angle expression is a float byte offset (sizeof(float)*angle), not an
    # angle multiplied by four. Local TableAngle therefore uses riseAngle.
    for token in (
        "v6 += 90;",
        "v5 += 90;",
        "if ((int)v6 >= (int)&flt_72D9E4)",
        "v6 -= 360;",
        "v5 -= 360;",
        "v8 = 4;",
        "while (v8);",
    ):
        assert compact(token) in compact(ref_render), f"Ref corner cadence omits {token}"
    for token in (
        "int angle = 0;",
        "for (vector3d& corner : corners)",
        "angle += 90;",
        "if (angle >= 360)",
        "angle -= 360;",
    ):
        assert token in local, f"local corner cadence omits {token}"

    # The source computes v4=4*riseAngle and uses it as a byte displacement
    # from the float table base. Verify both this decomp expression and the
    # canonical 1-degree table initialization before mapping it to a degree
    # index in the local 361-element arrays.
    assert compact("v4 = 4 * *((__int16 *)p_z - 7);") in compact(ref_render)
    assert compact("*(float *)(v4 + 7525444) * v9") in compact(ref_render)
    assert compact("*(float *)(v4 + 7522556) * v9") in compact(ref_render)
    trig_init = function_body(exe, "void __cdecl CreateTrigonometricTable()")
    assert compact("v1 = (double)i * 3.141592 * 0.0055555557;") in compact(trig_init)
    assert "g_sinTable[v0] = ib;" in trig_init and "g_cosTable[v0] = v3;" in trig_init
    assert compact("while (v0 <= 360);") in compact(trig_init)
    assert "const int rise = TableAngle(gi.riseAngle);" in local
    assert "g_cosTable[rise] * radialX" in local
    assert "g_sinTable[rise] * radialX" in local
    print("PASS rise-angle map: PE 4*angle is a byte offset into its 1-degree float table; local TableAngle(angle) selects the same element")

    # Radius and origin map to distance and sparkXYZ. The local expression
    # builds the same rotated X/Z and unrotated Y corner coordinates.
    assert compact("v3 = *(p_z - 3);") in compact(ref_render)
    assert compact("*(p_z - 2) + *(v7 - 5)") in compact(ref_render)
    assert compact("*(v7 - 4) + *(p_z - 1)") in compact(ref_render)
    assert compact("*(v7 - 3) + *p_z") in compact(ref_render)
    for token in (
        "const float radialX = g_cosTable[angle] * gi.distance;",
        "corner.x = g_cosTable[rise] * radialX + gi.sparkX;",
        "corner.y = g_sinTable[angle] * gi.distance + gi.sparkY;",
        "corner.z = g_sinTable[rise] * radialX + gi.sparkZ;",
    ):
        assert token in local, f"local point/origin mapping omits {token}"

    # RenderTeiRect signature is (alpha, four points, matrix, PW, TN, F1,
    # RGB, Ty1,Ty2,Tx,Tx2). It receives alphaB, F1=1, RGB 60/60/255 and
    # full [0,1] UVs. The local helper packs alpha and RGB and submits the same
    # four corners with additive blending and the source UV orientation.
    assert "*((_WORD *)p_z - 69)," in ref_render
    source_submit = (
        "CEffectPrim::RenderTeiRect(v11, &rpCnt, *((_WORD *)p_z - 69), "
        "vec[0], vec[1], vec[2], vec[3], wtm, 0, 0, 1, "
        "0x3Cu, 0x3Cu, 0xFFu, 0.0, 1.0, 0.0, 1.0)"
    )
    assert compact(source_submit) in compact(ref_render)
    assert "static_cast<unsigned int>(static_cast<unsigned short>(gi.alphaB)) << 24" in local
    assert "| 0x003C3CFFu" in local
    local_submit = (
        "SubmitWorldTeiRect(corners[0], corners[1], corners[2], corners[3], viewMatrix, "
        "texture, color, 0.0f, 1.0f, 1.0f, 0.0f, D3DBLEND_ONE, 0.0f, "
        "static_cast<int>(prim.m_renderFlag))"
    )
    assert compact(local_submit) in compact(local)
    assert "near(quad.b.x,10.0f)&&near(quad.b.y,20.0f)&&near(quad.b.z,28.0f)" in compact(render_tests)
    print("PASS submission map: alphaB, blue tint, F1/full UV source call map to local packed color and additive quad submission")
    print("RenderAirTexture audit passed: PE/Ref body, dispatch, gates, corners, transform, origin and submission mapped")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
