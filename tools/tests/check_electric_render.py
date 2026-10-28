#!/usr/bin/env python3
"""Source audit for PE/Ref/current PP_ELECTRIC rendering parity."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SIGNATURE = "void __thiscall CEffectPrim::RenderElectric(CEffectPrim *this, matrix *wtm)"


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


def assert_ordered(source: str, tokens: tuple[str, ...], label: str) -> None:
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

    ref_render = function_body(ref, SIGNATURE)
    exe_render = function_body(exe, SIGNATURE)
    assert compact(ref_render) == compact(exe_render), (
        "Ref RenderElectric differs from canonical executable"
    )
    assert "PP_ELECTRIC = 0x65" in header

    assert re.search(
        r"case\s+PP_ELECTRIC\s*:\s*RenderElectric\(wtm\)\s*;\s*break\s*;",
        ref_dispatch_source,
    ), "Ref Render dispatcher lost the PP_ELECTRIC route"
    exe_dispatch = function_body(
        exe, "void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm)"
    )
    route_start = exe_dispatch.index("case 0x65:")
    route_end = exe_dispatch.index("case 0x66:", route_start)
    assert "RenderElectric(this, wtm)" in exe_dispatch[route_start:route_end]

    local_dispatch = function_body(current, "void CEffectPrim::Render(matrix* viewMatrix)")
    local_route = "if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeElectric))"
    assert local_route in local_dispatch, "local PP_ELECTRIC route is missing"
    route_start = local_dispatch.index(local_route)
    route_end = local_dispatch.index(
        "if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeAttackEnergy))", route_start
    )
    assert "RenderElectric(*this, *viewMatrix);" in local_dispatch[route_start:route_end]
    assert route_start < local_dispatch.index("switch (m_type)")

    local = function_body(current, "void RenderElectric(const CEffectPrim& prim, const matrix& viewMatrix)")
    path = function_body(
        current,
        "void ElectricPathPoint(int riseAngle, float angle, float heightFactor, float distance,",
    )

    # Four lanes and the render-time RNG schedule. Random jitter is generated
    # before lifecycle/alpha/process gates, in Y/X/Z order, for every sample.
    for token in (
        "p_y = &this->m_GI[0].vecT_now.y;",
        "v31 = 4;",
        "i = rand() % 7;",
        "heightR[v4] = (double)i * 0.1 - 0.30000001;",
        "i = rand() % 7;",
        "distXR[v4] = (double)i * 0.1 - 0.30000001;",
        "v5 = rand() % 7;",
        "distZR[v4 - 1] = (double)v5 * 0.1 - 0.30000001;",
        "while ((double)i < *p_y);",
        "if (*((_BYTE *)p_y - 148) == 1",
        "p_y += 45;",
        "--v31;",
        "while (v31);",
    ):
        assert compact(token) in compact(ref_render), f"Ref lane/RNG operation missing: {token}"
    source_rng_order = compact(ref_render)
    rng_positions = [
        source_rng_order.index("i=rand()%7;"),
        source_rng_order.index("i=rand()%7;", source_rng_order.index("i=rand()%7;") + 1),
        source_rng_order.index("v5=rand()%7;"),
    ]
    assert rng_positions == sorted(rng_positions), "source jitter RNG is not Y/X/Z ordered"
    rng_before_gate = source_rng_order.index("v5=rand()%7;") < source_rng_order.index("if(*((_BYTE*)p_y-148)==1")
    assert rng_before_gate, "source lane gate moved ahead of jitter draws"
    for token in (
        "for (const EffectGi& gi : prim.m_gi)",
        "float jitterY[21]{};",
        "float jitterX[21]{};",
        "float jitterZ[21]{};",
        "rand() % 7",
        "static_cast<double>(rand() % 7) * 0.1 - 0.30000001",
        "if (jitterSample < 21)",
        "if (gi.life != 1 || gi.alphaB <= 0 || gi.process <= 0 || gi.vecTNowY <= 0.0f)",
    ):
        assert token in local, f"local lane/RNG path missing: {token}"
    assert_ordered(local, (
        "const float y =", "const float x =", "const float z =",
        "jitterY[jitterSample] = y;", "jitterX[jitterSample] = x;", "jitterZ[jitterSample] = z;",
        "if (gi.life != 1 || gi.alphaB <= 0 || gi.process <= 0 || gi.vecTNowY <= 0.0f)",
    ), "local renderer")

    # Segment height/distance and vertical path map to ElectricPathPoint. The
    # source 4*rise expression is a float-table byte displacement, i.e. the
    # local degree-table element rise. The canonical trig table is 1 degree
    # per element.
    assert compact("if (*(v6 - 1) > 0.0) v7 = *(v6 - 1) * 0.011111111 * *(p_y - 5);") in compact(ref_render)
    assert compact("g_sinTable[(__int16)(__int64)*(v6 - 1)] * *(p_y - 1) * *(p_y - 5)") in compact(ref_render)
    assert "heightR[v3] - 2.0" in ref_render and "heightR[v3 + 1] - 2.0" in ref_render
    assert "distXR[v3]" in ref_render and "distZR[v3]" in ref_render
    for token in (
        "const int facing = TableAngle(riseAngle);",
        "const double radial = angle > 0.0f ? static_cast<double>(angle) * 0.011111111 * distance : 0.0;",
        "g_sinTable[angleIndex]) * heightFactor * distance",
        "static_cast<double>(jitterY) - 2.0 + point->y",
        "point->x += originX;",
        "point->y += originY;",
        "point->z += originZ;",
    ):
        assert token in path, f"local path-point mapping missing: {token}"
    trig_init = function_body(exe, "void __cdecl CreateTrigonometricTable()")
    assert compact("v1 = (double)i * 3.141592 * 0.0055555557;") in compact(trig_init)
    assert "g_cosTable[v0] = v3;" in trig_init and compact("while (v0 <= 360);") in compact(trig_init)
    assert compact("v8 = 4 * *((__int16 *)p_y - 11);") in compact(ref_render)

    # Orientation uses normalized X/Z deltas, a cosine-table scan capped at
    # 180, sign-selected opposite heading, then +90 with a one-step wrap.
    for token in (
        "Rx = vecA.z - vecB.z;",
        "v12 = vecA.x - vecB.x;",
        "Ry = v12;",
        "Rx = Rx / v13;",
        "Ry = Ry / v13;",
        "while (Ry < (double)g_cosTable[v14])",
        "if (Rx < 0.0)",
        "v16 = v15 + 90;",
        "if (v16 >= 360) v16 -= 360;",
    ):
        assert compact(token) in compact(ref_render), f"Ref heading mapping missing: {token}"
    for token in (
        "const float dx = pointA.x - pointB.x;",
        "const float dz = pointA.z - pointB.z;",
        "const float nx = dx / length;",
        "const float nz = dz / length;",
        "while (nx < g_cosTable[angle])",
        "if (++angle > 180)",
        "heading = nz < 0.0f ? 360 - angle : angle;",
        "const int side = TableAngle(heading + 90);",
    ):
        assert token in local, f"local heading mapping missing: {token}"

    # Only samples 1..18 draw; four radius positions at 0/120/240/360 yield
    # three strips. The call uses alphaB-5*sample, sample tint and full UVs.
    for token in (
        "if (v3 > 0 && v3 < 19)",
        "for (k = 0; k <= 1440; k += 480)",
        "if (k > 0 && *((__int16 *)p_y - 73) - i > 0)",
        "6 * v3,\n                6 * v3,\n                0xFFu,",
        "0.0,\n                1.0,\n                0.0,\n                1.0",
    ):
        assert compact(token) in compact(ref_render), f"Ref strip/submission branch missing: {token}"
    assert "for (int sample = 0; static_cast<float>(sample) < gi.vecTNowY && sample < samples; ++sample)" in local
    assert "if (sample > 0 && sample < 19)" in local
    assert "const int alpha = gi.alphaB - fade;" in local and "fade += 5;" in local
    assert "for (int around = 0; around <= 360; around += 120)" in local
    assert "const unsigned int color = (static_cast<unsigned int>(static_cast<unsigned char>(alpha)) << 24)" in local
    assert "(static_cast<unsigned int>(6 * sample) << 16)" in local
    assert "(static_cast<unsigned int>(6 * sample) << 8)" in local and "| 255u;" in local
    assert "SubmitWorldTeiRect(currentA, currentB, previousB, previousA, viewMatrix, texture, color," in local
    assert "0.0f, 1.0f, 1.0f, 0.0f, D3DBLEND_ONE" in local
    print("PASS rise-angle map: PE 4*angle is a float-table byte offset; local TableAngle(angle) selects the same degree entry")
    print("RenderElectric audit passed: PE/Ref body, dispatch, ordered RNG, path/heading geometry, strip branches and submission mapped")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
