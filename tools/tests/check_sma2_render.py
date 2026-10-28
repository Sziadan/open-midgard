#!/usr/bin/env python3
"""Source audit for PE/Ref/current PP_SMA2 rendering parity."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCE_SIGNATURE = "void __thiscall CEffectPrim::RenderSMA2(CEffectPrim *this, matrix *wtm)"
PE_RENDER_SIGNATURE = "void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm)"


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
    ref_dispatch = (ROOT / "Ref/dhxj/RagEffectPrim.cpp").read_text(encoding="cp1252")
    exe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8")
    current = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
    header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")

    ref_render = function_body(ref, SOURCE_SIGNATURE)
    exe_render = function_body(exe, SOURCE_SIGNATURE)
    assert compact(ref_render) == compact(exe_render), "Ref RenderSMA2 differs from canonical PE"
    assert "PP_SMA2 = 0x79" in header

    assert re.search(r"case\s+PP_SMA2\s*:\s*RenderSMA2\(wtm\)\s*;\s*break\s*;", ref_dispatch)
    pe_dispatch = function_body(exe, PE_RENDER_SIGNATURE)
    pe_start = pe_dispatch.index("case 0x79:")
    pe_end = pe_dispatch.index("case 0x7E:", pe_start)
    assert "CEffectPrim::RenderSMA2(this, wtm)" in pe_dispatch[pe_start:pe_end]

    local_dispatch = function_body(current, "void CEffectPrim::Render(matrix* viewMatrix)")
    assert "if (m_type == PP_SMA2)" in local_dispatch
    local_route_start = local_dispatch.index("if (m_type == PP_SMA2)")
    local_route = local_dispatch[local_route_start:local_dispatch.index("if (m_type == PP_BASH3D)", local_route_start)]
    assert "RenderSma2(*this, *viewMatrix);" in local_route
    local = function_body(current, "void RenderSma2(CEffectPrim& prim, const matrix& viewMatrix)")

    # The source walks four GI records and requires life exactly one.
    ordered(compact(ref_render), ("p_full_display_angle=&this->m_GI[0].full_display_angle;", "do", "if(*((_BYTE*)p_full_display_angle-6)==1)", "p_full_display_angle+=90;", "while(v28<421);"), "PE/Ref lane gate")
    assert "for (const EffectGi& gi : prim.m_gi)" in local
    assert "if (gi.life != 1) continue;" in local

    # BasicAngle is the source integer fullDisplayAngle/20. The source do/while
    # condition and local inclusive while both retain the zero-step edge for
    # nonnegative angles below 20; do not execute that edge in a runtime fixture.
    assert "1717986919LL * *p_full_display_angle" in ref_render
    assert "BasicAngle = (v6 >> 31) + v6;" in ref_render
    assert "v13 = BasicAngle + count <= *p_full_display_angle;" in ref_render
    assert "const int basicAngle = static_cast<int>(gi.fullDisplayAngle) / 20;" in local
    assert "while (gi.fullDisplayAngle >= 0 && count <= gi.fullDisplayAngle)" in local
    assert "count += basicAngle;" in local

    # Angle/order closure: count starts at zero, uses rotStart modulo 360,
    # and a 360-degree sweep's twentieth segment returns to rotStart. Height
    # storage clamps at element 20 after incrementing.
    for token in (
        "v5 = 0;", "order = 0;", "count = 0;",
        "v7 = (count + p_full_display_angle[5]) % 360;",
        "if ( *p_full_display_angle == 360 && v5 == 20 )",
        "v7 = p_full_display_angle[5];", "order = ++v5;",
        "if ( v5 >= 21 )", "v5 = 20;", "order = 20;",
    ):
        assert compact(token) in compact(ref_render), f"PE/Ref sweep closure missing: {token}"
    for token in (
        "int order = 0;", "int count = 0;", "int angle = (count + gi.rotStart) % 360;",
        "if (gi.fullDisplayAngle == 360 && order == 20) angle = gi.rotStart;",
        "++order;", "if (order >= 21) order = 20;", "count += basicAngle;",
    ):
        assert token in local, f"local sweep closure missing: {token}"

    # Geometry maps base radius from distance; top displacement reads the
    # per-order height and riseAngle cosine/sine table values. Delta position
    # is added to both endpoints before strip construction.
    for token in (
        "v8 = g_cosTable[v7];", "sn1 = SLODWORD(g_sinTable[v7]);",
        "(float *)(p_full_display_angle + 61)", "*(float *)(v9 + 7525444) * *v10",
        "*(float *)(v9 + 7522556) * *v10", "vec2.x = vec2a + this->m_deltaPos.x;",
        "vec4.x = v12 + this->m_deltaPos.x;", "vec4.y = vec4_4 + vec2.y;",
    ):
        assert compact(token) in compact(ref_render), f"PE/Ref geometry missing: {token}"
    for token in (
        "static_cast<double>(cosine) * gi.distance", "static_cast<double>(sine) * gi.distance",
        "g_cosTable[gi.riseAngle]", "g_sinTable[gi.riseAngle]",
        "gi.height[static_cast<size_t>(order)]", "AddVec3(currentBase, prim.m_deltaPos)",
        "AddVec3(currentTop, prim.m_deltaPos)",
    ):
        assert token in local, f"local geometry mapping missing: {token}"

    # Source submits after the first point, using adjacent 1/21 UV boundaries,
    # alphaB and fixed RGB 90/90/255. RenderTeiRect arguments keep full V range.
    source_call = compact(ref_render)
    assert "if(v5>0)" in source_call
    ordered(source_call, (
        "Tx2=(double)order*0.047619049;", "Tx=(double)(v5-1)*0.047619049;",
        "CEffectPrim::RenderTeiRect(", "&rpCnt,", "*(p_full_display_angle-2),",
        "0,0,1,0x5Au,0x5Au,0xFFu,0.0,1.0,Tx,Tx2",
    ), "source strip submission")
    for token in (
        "static_cast<float>(order - 1) / 21.0f", "static_cast<float>(order) / 21.0f",
        "static_cast<unsigned short>(gi.alphaB)", "90u << 16", "90u << 8", "255u",
        "SubmitSma2Rect(rpCount, previousBase, currentBase, currentTop, previousTop",
        "viewMatrix, texture, color, u0, u1, static_cast<int>(prim.m_renderFlag)",
    ):
        assert token in local, f"local strip color/UV/submission missing: {token}"
    submit = function_body(current, "void SubmitSma2Rect(int& rpCount,")
    assert "if (rpCount >= 510) return;" in submit
    assert "D3DBLEND_ONE" in submit and "ProjectReferenceEffectVertex" in submit
    assert "const float uvs[4][2] = {{u0,1.0f},{u1,1.0f},{u1,0.0f},{u0,0.0f}};" in submit

    print("PASS RenderSMA2 PE/Ref normalized body identity and PP_SMA2 dispatch routes")
    print("PASS four-lane life gate, inclusive count/order sweep, angle closure, radius/rise geometry and delta position")
    print("PASS alphaB/RGB(90,90,255), adjacent 1/21 strip U coordinates and RenderTeiRect-equivalent submission")
    print("PASS basicAngle==0 for nonnegative fullDisplayAngle<20 is shared source behavior; no hanging fixture executed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
