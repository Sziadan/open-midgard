#!/usr/bin/env python3
"""Source audit for PE/Ref/current PP_COLORPAPER RenderColorPaper parity."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SIGNATURE = "void __thiscall CEffectPrim::RenderColorPaper(CEffectPrim *this, matrix *wtm)"
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
    assert compact(ref_render) == compact(pe_render), "Ref RenderColorPaper differs from canonical PE"
    assert "PP_COLORPAPER" in ref_dispatch
    route = re.search(r"case\s+PP_COLORPAPER\s*:\s*RenderColorPaper\(wtm\)\s*;\s*break\s*;", ref_dispatch)
    assert route, "primary Ref PP_COLORPAPER dispatch missing"

    pe_dispatch = function_body(exe, PE_RENDER)
    pe_start = pe_dispatch.index("case 0x4E:")
    pe_end = pe_dispatch.index("case 0x4F:", pe_start)
    assert "CEffectPrim::RenderColorPaper(this, wtm)" in pe_dispatch[pe_start:pe_end]
    local_dispatch = function_body(current, "void CEffectPrim::Render(matrix* viewMatrix)")
    assert "if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeColorPaper))" in local_dispatch
    route_start = local_dispatch.index("if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeColorPaper))")
    route_end = local_dispatch.index("if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeSphereWind)", route_start)
    assert "RenderColorPaper(*this, *viewMatrix);" in local_dispatch[route_start:route_end]
    assert "constexpr int kPrimTypeColorPaper" in header

    # Four 180-byte GI lanes are visited in order. Only exact life==1 emits.
    for token in (
        "rpCnt=0;", "v32=4;", "p_z=&this->m_GI[0].vecB_now.z;",
        "if(*((_BYTE*)p_z-140)==1)", "p_z+=45;", "--v32;", "while(v32);",
    ):
        assert compact(token) in compact(ref_render), f"PE/Ref lane/gate token missing: {token}"
    local = function_body(current, "void RenderColorPaper(const CEffectPrim& prim, const matrix& viewMatrix)")
    assert "for (const EffectGi& gi : prim.m_gi)" in local
    assert "if (gi.life != 1)" in local and "continue;" in local

    # Source forms a radius-sized quad, rotating four corners at 90-degree
    # increments around RotStart and projecting the circle through riseAngle.
    # Source normalization is a single +/-360 adjustment; local TableAngle
    # provides the corresponding normalized degree-table index.
    for token in (
        "Rx=*(p_z-33);", "v5=*((__int16*)p_z-7);", "v7=*((__int16*)p_z-62);",
        "v12=v7+90;", "v16=v7+180;", "v20=v7+270;",
        "vec1_4=v11*cs2;", "vec1_8=v11*sn2;",
        "vec2_4=v15*cs2;", "vec2_8=v15*sn2;",
        "vec3_4=v19*cs2;", "vec3_8=v19*sn2;",
        "vec4_4=v22*cs2;", "vec4_8=v22*sn2;",
    ):
        assert compact(token) in compact(ref_render), f"PE/Ref corner formula missing: {token}"
    for token in (
        "const int rise = TableAngle(gi.riseAngle);", "const float radius = gi.maxHeight;",
        "auto corner = [&](int spin)", "const int angle = TableAngle(spin);",
        "corner(gi.rotStart)", "corner(gi.rotStart + 90)", "corner(gi.rotStart + 180)", "corner(gi.rotStart + 270)",
        "g_cosTable[rise]", "g_sinTable[rise]", "g_cosTable[angle]", "g_sinTable[angle]",
    ):
        assert compact(token) in compact(local), f"local ColorPaper corner mapping missing: {token}"

    # RenderTeiRect receives source GI color fields and full-frame UVs. In this
    # legacy decompile the positional vector is named vecB_now; local EffectGi
    # separately documents sparkXYZ as world-position data. Keep the raw
    # source offsets explicit: the available Ref headers do not define the
    # legacy GI layout, so do not infer a field-index correspondence solely
    # from these offsets.
    source = compact(ref_render)
    call = source[source.index("CEffectPrim::RenderTeiRect("):]
    for token in (
        "*((_WORD*)p_z-69)", "vector3d", "wtm,0,0,0,(__int64)*(p_z-30),(__int64)*(p_z-29),(__int64)*(p_z-28),0.0,1.0,0.0,1.0",
    ):
        assert compact(token) in call, f"PE/Ref RenderTeiRect source argument missing: {token}"
    source_pos = (
        "vector3d.x=v10+*(p_z-2);", "vector3d.y=vec1_4+*(p_z-1);", "vector3d.z=vec1_8+*p_z;",
        "vector3da.x=v14+*(p_z-2);", "vector3db.x=v18+*(p_z-2);", "vector3dc.x=g_cosTable[v21]*Rx+*(p_z-2);",
    )
    for token in source_pos:
        assert compact(token) in source, f"PE/Ref center/corner field missing: {token}"

    color = "(static_cast<unsigned int>(static_cast<unsigned short>(gi.alphaB)) << 24)"
    assert color in local
    assert "static_cast<unsigned char>(static_cast<int>(gi.height[0]))" in local
    assert "static_cast<unsigned char>(static_cast<int>(gi.height[1]))" in local
    assert "static_cast<unsigned char>(static_cast<int>(gi.height[2]))" in local
    assert "prim.m_pos.x" in local and "prim.m_pos.y" in local and "prim.m_pos.z" in local
    assert "gi.sparkX" in local and "gi.sparkY" in local and "gi.sparkZ" in local

    # The legacy overload is anchored separately for texture, UV, blend and
    # flags. Local submission uses the common TeiRect guard and two triangles.
    rect = function_body(ref, "void __thiscall CEffectPrim::RenderTeiRect(\n        CEffectPrim *this,\n        int *rpCnt,\n        int TexPart,")
    assert "this->m_rp[*rpCnt].tex = *this->m_texture;" in rect or "this->m_rp[v15].tex = *this->m_texture;" in rect
    rect_compact = compact(rect)
    assert "this->m_rp[v15].srcAlphaMode=5;" in rect_compact
    assert "if(PW)" in rect_compact and "destAlphaMode=6;" in rect_compact and "destAlphaMode=2;" in rect_compact
    for token in ("tu = 1.0", "tu = 0.0", "r = (double)TexPart * 0.047619049", "tv = v19", "tv = Tx2", "CRenderer::AddRP(g_renderer, &this->m_rp[*rpCnt], this->m_renderFlag)"):
        assert compact(token) in rect_compact, f"RenderTeiRect submission behavior missing: {token}"
    helper = function_body(current, "void SubmitWorldTeiRect(")
    assert "if (!texture || texture == &CTexMgr::s_dummy_texture)" in helper
    assert "face->destAlphaMode = destBlend;" in helper
    assert "g_renderer.AddRP(face, renderFlags);" in helper
    for token in (
        "texture,", "color,", "0.0f,", "1.0f,", "1.0f,", "0.0f,", "D3DBLEND_ONE,", "static_cast<int>(prim.m_renderFlag)",
    ):
        assert compact(token) in compact(local), f"local submission argument missing: {token}"
    assert "if (!texture || texture == &CTexMgr::s_dummy_texture)" in helper

    print("PASS RenderColorPaper primary Ref/PE normalized body identity and PP_COLORPAPER dispatch")
    print("PASS four GI lanes, exact-life gate, radius/rise/90-degree corner mapping and local world-position fields")
    print("PASS source RenderTeiRect offsets and local alphaB/height RGB, full UV, additive blend, flags and null guard")
    print("NOTE legacy source color/position offsets are preserved verbatim; Ref headers do not provide a GI layout to prove field-index equivalence")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
