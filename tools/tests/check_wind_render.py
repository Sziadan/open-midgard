#!/usr/bin/env python3
"""Source audit for PE/Ref/current PP_WIND RenderWind parity."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SIGNATURE = "void __thiscall CEffectPrim::RenderWind(CEffectPrim *this, matrix *wtm)"
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
    assert compact(ref_render) == compact(pe_render), "Ref RenderWind differs from canonical PE"
    assert "PP_WIND = 0x3E" in header
    assert re.search(r"case\s+PP_WIND\s*:\s*RenderWind\(wtm\)\s*;\s*break\s*;", ref_dispatch)

    pe_dispatch = function_body(exe, PE_RENDER)
    pe_start = pe_dispatch.index("case 0x3E:")
    pe_end = pe_dispatch.index("case 0x40:", pe_start)
    assert "CEffectPrim::RenderWind(this, wtm)" in pe_dispatch[pe_start:pe_end]

    local_dispatch = function_body(current, "void CEffectPrim::Render(matrix* viewMatrix)")
    normal_route = "if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeWind) && !UsesLegacyBandPayload())"
    assert normal_route in local_dispatch
    route_start = local_dispatch.index(normal_route)
    route_end = local_dispatch.index("if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeCasting4)", route_start)
    assert "RenderWind(*this, *viewMatrix);" in local_dispatch[route_start:route_end]

    legacy = function_body(current, "bool CEffectPrim::UsesLegacyBandPayload() const")
    assert "m_master && m_master->IsWorldEffect()" in legacy
    assert "m_type == PP_MAPPILLAR || m_type == PP_PORTAL || m_type == PP_WIND" in legacy
    assert "PP_WIND" in local_dispatch[local_dispatch.index("void CEffectPrim::Render(matrix* viewMatrix)"):]
    assert "if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeWind) && !UsesLegacyBandPayload())" in local_dispatch

    local = function_body(current, "void RenderWind(CEffectPrim& prim, const matrix& viewMatrix)")
    sample = function_body(current, "void WindRenderSample(int angle, int riseAngle, float height, float distance, float maxHeight, vector3d* base, vector3d* top)")

    # Four GI records require exact life one, then use integer full-angle/21
    # cadence, modulo angle and a special closing angle at order20/full360.
    for token in (
        "p_distance = &this->m_GI[0].distance;", "v18 = 241;", "v3 = *((__int16 *)p_distance - 61);",
        "BasicAngle = v3 / 21;", "if ( v3 > 0 )", "v6 = (count + *((__int16 *)p_distance - 56)) % 360;",
        "if ( *((_WORD *)p_distance - 61) == 360 && order == 20 )",
        "v6 = *((__int16 *)p_distance - 56);", "count += BasicAngle;", "while ( count < v11 );",
        "p_distance += 45;", "while ( v18 < 421 );",
    ):
        assert compact(token) in compact(ref_render), f"PE/Ref cadence/lane path missing: {token}"
    assert "for (EffectGi& gi : prim.m_gi)" in local
    assert "if (gi.life != 1)" in local and "continue;" in local
    assert "const int basicAngle = gi.fullDisplayAngle / 21;" in local
    assert "if (basicAngle <= 0)" in local
    assert "for (int count = 0; count < gi.fullDisplayAngle; count += basicAngle)" in local
    assert "if (gi.fullDisplayAngle == 360 && order == 20)" in local
    assert "angle = TableAngle(gi.rotStart);" in local

    # Safety deviation: source enters for any positive fullDisplayAngle; when
    # 1..20, BasicAngle is zero and the canonical count loop cannot progress.
    # The local renderer skips that range to avoid hanging; neither source nor
    # local zero-step loop is executed by this comparator or a runtime fixture.
    assert compact("BasicAngle = v3 / 21;") in compact(ref_render)
    assert "if ( v3 > 0 )" in ref_render
    assert "while ( count < v11 );" in ref_render
    assert "if (basicAngle <= 0)" in local and "continue;" in local

    # Geometry: source writes the ground ring and top point into the GI scratch
    # fields every sample, then connects previous/current base/top after point 0.
    for token in (
        "*v4 = v7 * *p_distance;", "p_distance[3] = sn1 * *p_distance;",
        "p_distance[2] = -*(p_distance - 30);", "Rx = g_cosTable[v9] * *v8;",
        "Ry = *(float *)(v9 * 4 + 7522556) * *v8;", "*v5 = Rx * v7 + *v4;",
        "p_distance[6] = Rx * sn1 + p_distance[3];", "p_distance[5] = -(Ry + *(p_distance - 30));",
        "if ( !v10 )", "order - 1", "p_distance[7] = *v4;", "p_distance[10] = *v5;",
    ):
        assert compact(token) in compact(ref_render), f"PE/Ref geometry/scratch path missing: {token}"
    for token in (
        "g_cosTable[spin]", "g_sinTable[spin]", "base->y = -maxHeight;",
        "g_cosTable[rise]", "g_sinTable[rise]", "base->x", "base->z", "top->x", "top->y", "top->z",
    ):
        assert token in sample, f"local WindRenderSample mapping missing: {token}"
    for token in (
        "gi.sparkX = base.x;", "gi.sparkY = base.y;", "gi.sparkZ = base.z;",
        "gi.vecTNowX = top.x;", "gi.vecTNowY = top.y;", "gi.vecTNowZ = top.z;",
        "gi.vecBPreX = gi.sparkX;", "gi.arcBaselineY = gi.sparkY;", "gi.vecBPreZ = gi.sparkZ;",
        "gi.vecTPreX = gi.vecTNowX;", "gi.vecTPreY = gi.vecTNowY;", "gi.vecTPreZ = gi.vecTNowZ;",
    ):
        assert token in local, f"local GI scratch mapping missing: {token}"

    # Source emits from sample 1 with order-1 texture part, two alpha fields,
    # then previous/current base/top vectors. Local creates adjacent 1/21 U
    # strips and submits translated geometry through the shared TeiRect helper.
    assert compact("if ( !v10 )") in compact(ref_render)
    assert compact("CEffectPrim::RenderTeiRect(") in compact(ref_render)
    assert compact("order - 1,") in compact(ref_render)
    assert compact("*((_WORD *)p_distance - 63), *((_WORD *)p_distance - 63),") in compact(ref_render)
    ordered_source = (
        "*(vector3d *)(p_distance + 7), *(vector3d *)(p_distance + 1),",
        "*(vector3d *)(p_distance + 4), *(vector3d *)(p_distance + 10), wtm",
    )
    source = compact(ref_render)
    cursor = 0
    for token in ordered_source:
        found = source.find(compact(token), cursor)
        assert found >= 0, f"PE/Ref strip call omits or reorders: {token}"
        cursor = found + len(compact(token))
    for token in (
        "const int part = order - 1;", "static_cast<double>(part) * 0.047619049",
        "static_cast<double>(part + 1) * 0.047619049", "const vector3d previousBase{ gi.vecBPreX, gi.arcBaselineY, gi.vecBPreZ };",
        "const vector3d previousTop{ gi.vecTPreX, gi.vecTPreY, gi.vecTPreZ };",
        "AddVec3(previousBase, prim.m_pos)", "AddVec3(base, prim.m_pos)", "AddVec3(top, prim.m_pos)",
        "AddVec3(previousTop, prim.m_pos)", "SubmitWorldTeiRect(worldPreviousBase",
        "u0, u1,", "blend,", "static_cast<int>(prim.m_renderFlag)",
    ):
        assert compact(token) in compact(local), f"local strip/submission mapping missing: {token}"
    assert "CastingSizeRgb(prim.m_size)" in local
    assert "CastingSizeAlpha(prim.m_size" in local
    assert "CastingSizeBlend(prim.m_size)" in local

    # Existing tests cover the local sample formula across representative rises.
    sample_start = render_tests.index("for(int rise:{0,23,45,90})")
    sample_end = render_tests.index("for(int angle:{0,90,180,270})", sample_start)
    sample_fixture = render_tests[sample_start:sample_end]
    assert "WindRenderSample(angle,rise,height,distance,maxHeight,&base,&top);" in sample_fixture
    assert "wind casting sample matches source base/rise geometry and source float stores" in sample_fixture
    print("PASS RenderWind primary Ref/PE normalized body identity and PP_WIND dispatch")
    print("PASS local normal PP_WIND route is separate from world-effect legacy band payload")
    print("PASS four life-gated lanes, angle cadence/seam, geometry, GI scratch writes and strip submission map")
    print("PASS source BasicAngle==0 at positive angles 1..20 is recorded; local skips to avoid source nonprogressing loop")
    print("PASS existing WindRenderSample fixture covers representative rise-angle geometry")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
