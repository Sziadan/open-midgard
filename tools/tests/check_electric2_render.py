#!/usr/bin/env python3
"""Source audit for PE/Ref/current PP_ELECTRIC2 rendering parity."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SIGNATURE = "void __thiscall CEffectPrim::RenderElectric2(CEffectPrim *this, matrix *wtm)"


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


def ordered(source: str, tokens: tuple[str, ...], label: str) -> None:
    cursor = 0
    for token in tokens:
        found = source.find(token, cursor)
        assert found >= 0, f"{label} omits or reorders: {token}"
        cursor = found + len(token)


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
        "Ref RenderElectric2 differs from canonical executable"
    )
    assert "PP_ELECTRIC2 = 0x66" in header

    assert re.search(
        r"case\s+PP_ELECTRIC2\s*:\s*RenderElectric2\(wtm\)\s*;\s*break\s*;",
        ref_dispatch_source,
    ), "Ref Render dispatcher lost the PP_ELECTRIC2 route"
    exe_dispatch = function_body(
        exe, "void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm)"
    )
    route_start = exe_dispatch.index("case 0x66:")
    route_end = exe_dispatch.index("case 0x67:", route_start)
    assert "RenderElectric2(this, wtm)" in exe_dispatch[route_start:route_end]

    local_dispatch = function_body(current, "void CEffectPrim::Render(matrix* viewMatrix)")
    local_route = "if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeElectric2))"
    assert local_route in local_dispatch, "local PP_ELECTRIC2 route is missing"
    route_start = local_dispatch.index(local_route)
    route_end = local_dispatch.index(
        "if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeElectric))", route_start
    )
    assert "RenderElectric2(*this, *viewMatrix);" in local_dispatch[route_start:route_end]
    assert route_start < local_dispatch.index("switch (m_type)")

    local = function_body(current, "void RenderElectric2(const CEffectPrim& prim, const matrix& viewMatrix)")
    electric2_path = function_body(
        current,
        "void Electric2PathPoint(int riseAngle, int sample, float angle, float heightFactor, float distance,",
    )
    electric_path = function_body(
        current,
        "void ElectricPathPoint(int riseAngle, float angle, float heightFactor, float distance,",
    )

    # Four lanes receive independent Y/X/Z jitter draws before active-lane
    # gates. Each sample uses three rand()%11 values in that order.
    for token in (
        "p_y = &this->m_GI[0].vecT_now.y;",
        "v40 = 4;",
        "if (*p_y > 0.0)",
        "i = rand() % 11;",
        "heightR[v3] = (double)i * 0.1 - 0.5;",
        "i = rand() % 11;",
        "distXR[v3] = (double)i * 0.1 - 0.5;",
        "v4 = rand() % 11;",
        "distZR[v3 - 1] = (double)v4 * 0.1 - 0.5;",
        "while ((double)i < *p_y);",
        "if (*((_BYTE *)p_y - 148) == 1",
        "p_y += 45;",
        "--v40;",
    ):
        assert compact(token) in compact(ref_render), f"Ref lane/RNG path missing: {token}"
    for token in (
        "for (const EffectGi& gi : prim.m_gi)",
        "float jitterY[21]{};",
        "float jitterX[21]{};",
        "float jitterZ[21]{};",
        "rand() % 11",
        "if (jitterSample < 21)",
        "if (gi.life != 1 || gi.alphaB <= 0 || gi.process <= 0 || gi.vecTNowY <= 0.0f || samples <= 0)",
    ):
        assert token in local, f"local lane/RNG path missing: {token}"
    ordered(local, (
        "const float y =", "const float x =", "const float z =",
        "jitterY[jitterSample] = y;", "jitterX[jitterSample] = x;", "jitterZ[jitterSample] = z;",
        "if (gi.life != 1 || gi.alphaB <= 0 || gi.process <= 0 || gi.vecTNowY <= 0.0f || samples <= 0)",
    ), "local renderer")

    # Source span/spread begins at (150,-135), changes by -15/+15 per
    # sample, and radial steps advance by 0.2*distance. The span admits only
    # interior samples; alpha uses spread while span is positive, then span.
    assert "v43 = -135;" in ref_render and "v45 = 150;" in ref_render
    assert compact("if (v45 < 150 && v45 > -135)") in compact(ref_render)
    assert compact("if (v45 < 150)") in compact(ref_render)
    assert compact("v7 = (double)Rx * 0.2 * *(p_y - 5);") in compact(ref_render)
    assert "v43 += 15;" in ref_render and "v45 = v18 - 15;" in ref_render
    assert compact("if (v18 <= 0) v25 = v18 + v24;") in compact(ref_render)
    assert compact("else v25 = v43 + v24;") in compact(ref_render)
    assert "int spread = -135;" in local and "int span = 150;" in local
    assert "const int samples = static_cast<int>(gi.vecTNowY);" in local
    assert "for (int sample = 0; sample < samples; ++sample)" in local
    assert "if (span < 150 && span > -135)" in local
    assert "const int alpha = span <= 0 ? gi.alphaB + span : gi.alphaB + spread;" in local
    assert "spread += 15;" in local and "span -= 15;" in local

    # Path map: positive samples scale radial distance; vertical motion uses
    # sine(height)*vecTNowX*-15, plus jitterY-8 and the lane origin. Critically,
    # v9=4*riseAngle is a byte offset into a float array, not index 4*rise.
    assert compact("v9 = 4 * *((__int16 *)p_y - 11);") in compact(ref_render)
    assert compact("*(float *)(v9 + 7525444)") in compact(ref_render)
    assert compact("*(float *)(v9 + 7522556)") in compact(ref_render)
    assert "v45 < 150" in ref_render and "(double)Rx * 0.2 * *(p_y - 5)" in ref_render
    assert "heightR[v6] - 8.0" in ref_render and "heightR[v6 + 1] - 8.0" in ref_render
    assert "const int facing = TableAngle(riseAngle);" in electric2_path
    assert "const int facing = TableAngle(riseAngle);" in electric_path
    assert "4 * riseAngle" not in electric2_path
    trig_init = function_body(exe, "void __cdecl CreateTrigonometricTable()")
    assert compact("v1 = (double)i * 3.141592 * 0.0055555557;") in compact(trig_init)
    assert "g_cosTable[v0] = v3;" in trig_init and compact("while (v0 <= 360);") in compact(trig_init)
    assert "const double radial = static_cast<double>(sample) * 0.2 * distance;" in electric2_path
    assert "heightFactor * -15.0" in electric2_path
    assert "static_cast<double>(jitterY) - 8.0 + point->y" in electric2_path
    assert "point->x += originX;" in electric2_path and "point->z += originZ;" in electric2_path

    # Heading and strip path: source cosine scan/180 cap/sign select/+90 wrap;
    # 3 ring strips submit only when calculated alpha is positive. Each strip
    # draws green rand()%121 first, then red rand()%121, followed by blue 255.
    for token in (
        "*(float *)&Rx = vecA.z - vecB.z;",
        "v13 = vecA.x - vecB.x;",
        "while (Ry < (double)g_cosTable[v15])",
        "if (*(float *)&Rx < 0.0)",
        "v17 = v16 + 90;",
        "if (v17 >= 360) v17 -= 360;",
        "v20 = 0;",
        "if (v20 > 0)",
        "v26 = rand() % 121;",
        "v27 = rand();",
        "v20 += 480;",
        "while (v20 <= 1440);",
    ):
        assert compact(token) in compact(ref_render), f"Ref heading/submission path missing: {token}"
    ordered(compact(ref_render), ("v26=rand()%121;", "v27=rand();", "RenderTeiRect("), "PE/Ref palette RNG")
    for token in (
        "ElectricHeading(pointA.x - pointB.x, pointA.z - pointB.z)",
        "const int side = TableAngle(ElectricHeading(pointA.x - pointB.x, pointA.z - pointB.z) + 90);",
        "for (int around = 0; around <= 360; around += 120)",
        "const int green = rand() % 121;",
        "const int red = rand() % 121;",
        "if (havePrevious && alpha > 0)",
        "| 255u;",
        "SubmitWorldTeiRect(currentA, currentB, previousB, previousA, viewMatrix, texture, color,",
        "0.0f, 1.0f, 1.0f, 0.0f, D3DBLEND_ONE",
    ):
        assert token in local, f"local heading/submission path missing: {token}"
    fixture_start = render_tests.index("Electric2PathPoint(")
    fixture_end = render_tests.index("\n    }", fixture_start)
    electric2_fixture = compact(render_tests[fixture_start:fixture_end])
    assert "GetCos(rise)" in electric2_fixture and "GetSin(rise)" in electric2_fixture
    assert "GetCos(rise*4)" not in electric2_fixture
    print("PASS rise-angle map: PE 4*angle is a float-table byte offset; Electric2PathPoint uses the matching degree-table element")
    print("RenderElectric2 audit passed: PE/Ref body, dispatch, pre-gate RNG, span/alpha branches, heading, strips and submission mapped")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
