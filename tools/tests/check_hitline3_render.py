#!/usr/bin/env python3
"""Source audit for PE/Ref/current PP_HITLINE3 rendering parity."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SIGNATURE = "void __thiscall CEffectPrim::RenderHitLine3(CEffectPrim *this, matrix *wtm)"


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

    ref_render = function_body(ref, SIGNATURE)
    exe_render = function_body(exe, SIGNATURE)
    assert compact(ref_render) == compact(exe_render), (
        "Ref RenderHitLine3 differs from canonical executable"
    )
    assert "PP_HITLINE3 = 0x64" in header

    # The readable Ref switch is not present in Ref/RagEffect2.cpp; use the
    # source tree's dispatcher in Ref/dhxj/RagEffectPrim.cpp and canonical PE.
    assert re.search(
        r"case\s+PP_HITLINE3\s*:\s*RenderHitLine3\(wtm\)\s*;\s*break\s*;",
        ref_dispatch_source,
    ), "Ref Render dispatcher lost the PP_HITLINE3 route"
    exe_dispatch = function_body(
        exe, "void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm)"
    )
    route_start = exe_dispatch.index("case 0x64:")
    route_end = exe_dispatch.index("case 0x65:", route_start)
    assert "RenderHitLine3(this, wtm)" in exe_dispatch[route_start:route_end]

    local_dispatch = function_body(current, "void CEffectPrim::Render(matrix* viewMatrix)")
    local_route = "if (m_type == PP_HITLINE3)"
    assert local_route in local_dispatch, "local PP_HITLINE3 route is missing"
    route_start = local_dispatch.index(local_route)
    route_end = local_dispatch.index("if (m_type == PP_", route_start + len(local_route))
    assert "RenderHitLine3(*this, *viewMatrix);" in local_dispatch[route_start:route_end]
    assert route_start < local_dispatch.index("switch (m_type)")

    local = function_body(current, "void RenderHitLine3(const CEffectPrim& prim, const matrix& viewMatrix)")
    # PE/Ref lane gates and variable-height segment walk: j starts at zero,
    # each iteration advances the history pointer and subtracts 10 alpha.
    for source_token in (
        "if ( *((_BYTE *)p_process - 12) != 1 )",
        "if ( *((__int16 *)p_process - 5) <= 0 )",
        "if ( *p_process <= 0 )",
        "v4 = *((float *)p_process + 34) > 0.0;",
        "if ( !v4 )",
        "j = 0;",
        "v66 = 0;",
        "v66 += 10;",
        "if ( (double)j < *((float *)v25 + 34) )",
    ):
        assert compact(source_token) in compact(ref_render), f"Ref omits gate/bound {source_token}"
        assert compact(source_token) in compact(exe_render), f"PE omits gate/bound {source_token}"
    for local_token in (
        "if (gi.life != 1 || gi.alphaB <= 0 || gi.process <= 0)",
        "for (int segment = 0; static_cast<float>(segment) < gi.vecTNowY; ++segment)",
        "gi.height[static_cast<size_t>(segment)]",
        "gi.height[static_cast<size_t>(segment + 1)]",
        "gi.alphaB - segment * 10",
    ):
        assert local_token in local, f"local helper omits segment gate/bound {local_token}"

    # Segment geometry: positive heights scale radial displacement by distance;
    # the endpoint delta is normalized and scanned against cosine values, then
    # rotated by 90 degrees. Each radius builds four ring vertices with -5 Y.
    for source_token in (
        "*(v5 - 1) * 0.011111111 * *((float *)p_process + 29)",
        "*v67 * 0.011111111 * *((float *)v72 + 29)",
        "while ( Ry < (double)g_cosTable[v14] )",
        "if ( Rx < 0.0 )",
        "v16 = v15 + 90;",
        "if ( v16 >= 360 )\n        v16 -= 360;",
        "v22 = Ry - 5.0;",
        "v18 += 480;",
    ):
        assert compact(source_token) in compact(ref_render), f"Ref geometry omits {source_token}"
        assert compact(source_token) in compact(exe_render), f"PE geometry omits {source_token}"
    for local_token in (
        "heightA / 90.0f) * gi.distance",
        "heightB / 90.0f) * gi.distance",
        "GetReferenceAngle(a.x - b.x, a.z - b.z) + 90",
        "if (angle >= 360)",
        "gi.maxHeight * 4.0f",
        "radius * g_sinTable[ringAngle] - 5.0f",
        "for (int ring = 0; ring <= 3; ++ring)",
    ):
        assert local_token in local, f"local geometry omits {local_token}"

    # Per-segment alphaT palette from the source switch. RenderTeiRect's
    # signature confirms the three final byte arguments are r, g, b.
    assert "case 0:" in ref_render and "case 1:" in ref_render and "case 2:" in ref_render
    assert "v39 = 30 * j;" in ref_render and "v38 = -56;" in ref_render and "v37 = -1;" in ref_render
    assert "v24 = 22 * j + 10;" in ref_render and "v39 = v24;" in ref_render
    assert "v24 = 22 * j + 10;" in ref_render and "v38 = v24;" in ref_render
    assert "v39 = 50 * j;" in ref_render and "v38 = -56;" in ref_render and "v37 = -1;" in ref_render
    assert "v24 = 30 * j + 105;" in ref_render and "if ( v23 < 3 )" in ref_render
    assert compact("*((_WORD *)v72 - 4) == 3") in compact(ref_render)
    assert "v31 = (__int64)((double)(*((__int16 *)v25 - 5) - v66) * 0.5);" in ref_render
    assert "0xFFu" in ref_render and "0.5" in ref_render
    assert "unsigned __int8 r, unsigned __int8 g, unsigned __int8 b" in exe
    regular_call = (
        "CEffectPrim::RenderTeiRect(this, &rpCnt, *((_WORD *)v72 - 5) - v66, "
        "v33, v35, vec3, vec1, wtm, 0, 0, 1, v37, v38, v39, "
        "0.0, 1.0, 0.0, 1.0)"
    )
    wide_call = (
        "CEffectPrim::RenderTeiRect(this, &rpCnt, v31, v34, v36, vec3, vec1, "
        "wtm, 0, 0, 1, 0, 0, 0xFFu, 0.0, 1.0, 0.0, 1.0)"
    )
    assert compact(regular_call) in compact(ref_render)
    assert compact(wide_call) in compact(ref_render)

    # Local mode 0/1/2/3 and wide-strip palette correspond to Ref. The Ref
    # RenderTeiRect signature is (alpha, points, matrix, PW, TN, F1, r, g, b,
    # UV...), so mode 1's repeated v24 is an RGB gray ramp.
    assert "rgb(255, 200, 150 - ((5 - segment) * 30))" in local
    assert "const int channel = 120 - ((5 - segment) * 22);" in local
    assert "rgb(channel, channel, channel)" in local
    assert "rgb(255, 200, 150 - ((3 - segment) * 50))" in local
    assert "const int channel = 255 - ((5 - segment) * 30);" in local
    assert "drawStrip(gi.maxHeight * 4.0f, wideAlpha, 0x000000FFu);" in local
    assert "const unsigned int color = (static_cast<unsigned int>(static_cast<unsigned short>(alpha)) << 24) | rgb;" in local
    assert "SubmitWorldTeiRect(currentA, currentB, previousB, previousA, viewMatrix," in local
    assert "texture, color, 0.0f, 1.0f, 1.0f, 0.0f, D3DBLEND_ONE, 0.0f," in local
    assert "static_cast<int>(prim.m_renderFlag)" in local
    assert "const int wideAlpha = static_cast<int>(static_cast<float>(alpha) * 0.5f);" in local
    print("PASS alphaT palette: source and local alphaT==1 all use RGB 10+22*segment")
    print("PASS submission map: source RenderTeiRect flags/RGB/UV and local SubmitWorldTeiRect color/blend/UV paths checked")

    # The helper passes its texture to shared submission, which centrally skips
    # absent/dummy textures instead of adding an early-return to this renderer.
    assert "CTexture* texture = prim.m_texture.empty() ? nullptr : prim.m_texture[0];" in local
    assert "if (!texture || texture == &CTexMgr::s_dummy_texture)" in function_body(
        current, "void SubmitWorldTeiRect("
    )
    assert "if (!texture || texture == &CTexMgr::s_dummy_texture)" not in local
    assert "m_texture" not in ref_render and "m_texture" not in exe_render
    print("PASS texture handling: helper submits through the shared null/dummy guard")
    print("RenderHitLine3 audit passed: PE/Ref bodies and dispatch plus segment, geometry, color and submission map checked")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
