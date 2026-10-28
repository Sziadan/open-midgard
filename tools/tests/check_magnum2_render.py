#!/usr/bin/env python3
"""Source audit for PE/Ref/current PP_MAGNUM2 RenderMagnum2 parity."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SIGNATURE = "void __thiscall CEffectPrim::RenderMagnum2(CEffectPrim *this, matrix *wtm)"
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

    ref_render = function_body(ref, SIGNATURE)
    pe_render = function_body(exe, SIGNATURE)
    assert compact(ref_render) == compact(pe_render), "Ref RenderMagnum2 differs from canonical PE"
    assert re.search(r"case\s+PP_MAGNUM2\s*:\s*RenderMagnum2\(wtm\)\s*;\s*break\s*;", ref_dispatch)
    assert "PP_MAGNUM2" in header

    pe_dispatch = function_body(exe, PE_RENDER)
    pe_start = pe_dispatch.index("case 0x49:")
    pe_end = pe_dispatch.index("case 0x4A:", pe_start)
    assert "CEffectPrim::RenderMagnum2(this, wtm)" in pe_dispatch[pe_start:pe_end]
    local_dispatch = function_body(current, "void CEffectPrim::Render(matrix* viewMatrix)")
    local_route = "if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeMagnum2))"
    route_start = local_dispatch.index(local_route)
    route_end = local_dispatch.index("if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeSaintWing))", route_start)
    assert "RenderMagnum2(*this, *viewMatrix);" in local_dispatch[route_start:route_end]

    # Four 180-byte lanes, render-count cap and exact-life gate, then the
    # nonnegative angle check and integer cadence.
    for token in (
        "rpCnt=0;", "p_distance=&this->m_GI[0].distance;", "v15=4;",
        "if(rpCnt<256&&*((_BYTE*)p_distance-128)==1)",
        "BasicAngle=v3/20;", "if(v3>=0)", "p_distance+=45;", "--v15;", "while(v15);",
    ):
        assert compact(token) in compact(ref_render), f"PE/Ref lane/gate/cadence token missing: {token}"
    local = function_body(current, "void RenderMagnum2(const CEffectPrim& prim, const matrix& viewMatrix)")
    assert "for (const EffectGi& gi : prim.m_gi)" in local
    assert "if (gi.life != 1 || gi.fullDisplayAngle < 0)" in local
    assert "const int basic = gi.fullDisplayAngle / 20;" in local

    # Source scans count=0 through the inclusive fullDisplayAngle bound;
    # order saturates at 20 and full-circle sample20 closes to RotStart.
    for token in (
        "v4=0;", "order=0;", "count=0;", "while(count<=*((__int16*)p_distance-61));",
        "v8=v4+*((__int16*)p_distance-56);", "if(v8>=360)v8-=360;",
        "if(*((_WORD*)p_distance-61)==360&&order==20)v8=*((__int16*)p_distance-56);",
        "if(++order>=21)order=20;", "v4=BasicAngle+count;", "count+=BasicAngle;",
    ):
        assert compact(token) in compact(ref_render), f"PE/Ref cadence/order token missing: {token}"
    for token in (
        "while (count <= gi.fullDisplayAngle)", "sample + gi.rotStart", "if (angle >= 360)",
        "if (gi.fullDisplayAngle == 360 && order == 20)", "angle = TableAngle(angle);",
        "if (++order >= 21)", "sample = basic + count;", "count += basic;",
    ):
        assert compact(token) in compact(local), f"local cadence/order mapping missing: {token}"

    # Exact shared source zero-step edge: angles 0..19 are nonnegative, yet
    # integer division gives BasicAngle=0 and count remains within the loop.
    assert "if ( v3 >= 0 )" in ref_render and "while ( count <= *((__int16 *)p_distance - 61) );" in ref_render
    assert "if (basic <= 0)" in local and "break;" in local

    # Inner radius is distance at y=-3; outer radius adds maxHeight projected
    # by riseAngle in X/Z and falls by the corresponding sine in Y.
    for token in (
        "*v5=v9**p_distance;", "p_distance[3]=sn1**p_distance;", "p_distance[2]=-3.0;",
        "Rx=g_cosTable[v10]* *(p_distance-30);", "Ry=*(float*)(v10*4+7522556)**(p_distance-30);",
        "*v6=Rx*v9+*v5;", "p_distance[6]=Rx*sn1+p_distance[3];", "p_distance[5]=-(Ry+3.0);",
    ):
        assert compact(token) in compact(ref_render), f"PE/Ref geometry token missing: {token}"
    for token in (
        "g_cosTable[angle] * gi.distance", "-3.0f", "g_sinTable[angle] * gi.distance",
        "g_cosTable[rise] * gi.maxHeight", "g_sinTable[rise] * gi.maxHeight",
        "reach * g_cosTable[angle] + inner.x", "-(lift + 3.0f)",
        "reach * g_sinTable[angle] + inner.z", "TableAngle(gi.riseAngle)",
    ):
        assert compact(token) in compact(local), f"local geometry mapping missing: {token}"

    # The source emits a textured strip only after sample0 and for alphaB>0,
    # carrying alphaB in both RenderTeiRect alpha parameters and order-1 U.
    source = compact(ref_render)
    for token in (
        "if(!v11&&*((__int16*)p_distance-63)>0)", "order-1,",
        "*((_WORD*)p_distance-63),*((_WORD*)p_distance-63),",
        "*(vector3d*)(p_distance+7),*(vector3d*)(p_distance+1),",
        "*(vector3d*)(p_distance+4),*v7,wtm",
    ):
        assert compact(token) in source, f"PE/Ref strip gate/argument missing: {token}"
    for token in (
        "if (order > 0 && gi.alphaB > 0)", "const int part = order - 1;",
        "static_cast<float>(part) * 0.047619049f", "static_cast<float>(part + 1) * 0.047619049f",
        "static_cast<unsigned short>(gi.alphaB)", "0x00FFFFFFu", "u0,", "u1,",
        "1.0f,", "0.0f,", "D3DBLEND_ONE", "static_cast<int>(prim.m_renderFlag)",
    ):
        assert compact(token) in compact(local), f"local strip/color/submission mapping missing: {token}"

    # RenderTeiRect projects vec1/2/3 as its first triangle and vec4/3/1 as
    # the second, the same contract used by the local TeiRect helper.
    rect_signature = "void __thiscall CEffectPrim::RenderTeiRect(\n        CEffectPrim *this,\n        int *rpCnt,\n        int TexPart,\n        __int16 alphaB,\n        __int16 alphaT,"
    rect = compact(function_body(ref, rect_signature))
    assert "this->m_rp[v15].srcAlphaMode=5" in rect
    assert ".tu=1.0" in rect and ".tu=0.0" in rect
    assert "this->m_rp[*rpCnt].tex=*this->m_texture" in rect
    assert "CRenderer::AddRP(g_renderer,&this->m_rp[*rpCnt],this->m_renderFlag)" in rect
    helper = compact(function_body(current, "void SubmitWorldTeiRect("))
    assert "{&vec1,&vec2,&vec3}" in helper and "{&vec4,&vec3,&vec1}" in helper

    # Source slot +7 is previous inner (the tail stores current inner from
    # +1); *v7 addresses prior outer at +10 (the tail stores current outer).
    # Keep this ordering explicit to prevent swapped endpoints/diagonals.
    src_tail = source[source.index("p_distance[7]=*v5;"):]
    assert "p_distance[7]=*v5;" in src_tail and "p_distance[10]=*v6;" in src_tail
    expected_local_order = (
        "SubmitWorldTeiRect(place(previousInner),place(inner),place(outer),place(previousOuter),"
    )
    assert expected_local_order in compact(local), "local strip endpoints do not preserve PE/Ref corner order A-B-C-D"

    # No C++ geometry fixture currently mentions Magnum2; this source-level
    # endpoint-order assertion is the regression guard for the correction.
    tests = (ROOT / "tools/tests/ParticleRenderTests.cpp").read_text(encoding="utf-8")
    assert "RenderMagnum2" not in tests and "PP_MAGNUM2" not in tests

    print("PASS RenderMagnum2 primary Ref/PE normalized body identity and PP_MAGNUM2 dispatch")
    print("PASS four lanes, gates, integer cadence, order closure, radius/rise/Y geometry and alpha strips")
    print("PASS source/local A-B-C-D TeiRect order, white RGB, UV/blend/texture/renderFlag submission")
    print("PASS source BasicAngle==0 nonprogressing range 0..19 is recorded; local safety break is retained and not executed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
