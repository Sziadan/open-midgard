#!/usr/bin/env python3
"""Compare the canonical PP_3DSPHERE renderer with its local route."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def body(source: str, signature: str) -> str:
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 1
    cursor = opening + 1
    while depth:
        depth += (source[cursor] == "{") - (source[cursor] == "}")
        cursor += 1
    return source[start:cursor]


def compact(source: str) -> str:
    return re.sub(r"\s+", "", source)


def require(ok: bool, reason: str) -> None:
    if not ok:
        raise SystemExit(reason)


pe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
ref = (ROOT / "Ref/RagEffectPrim.cpp").read_text(encoding="utf-8", errors="replace")
local = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
tests = (ROOT / "tools/tests/ParticleRenderTests.cpp").read_text(encoding="utf-8", errors="replace")
signature = "void __thiscall CEffectPrim::Render3DSphere(CEffectPrim *this, matrix *wtm)"
pe_render, ref_render = body(pe, signature), body(ref, signature)
require(compact(pe_render) == compact(ref_render), "PE and primary Ref Render3DSphere bodies differ")

# Lock the recovered mesh's generation, corner order, projection and submission
# contract. The generated local triangles must retain the source's two-face cell.
for token in (
    "360.0/this->m_arcAngle", "1.0/(180.0/this->m_arcAngle)",
    "j+this->m_arcAngle+0.25+this->m_longitude", "(__int64)i",
    "CRenderer::ProjectVertex(g_renderer,&src,wtm,&vert)",
    "*v95=v17", "*v91=v19", "*v96=v21", "*v94=v23",
    "*v79=v26", "*v79=v43",
    "*v84=*this->m_texture", "CRenderer::AddRP(g_renderer,v49,1)",
    "CRenderer::AddRP(g_renderer,v50,1)",
):
    require(token in compact(pe_render), f"canonical sphere mesh contract missing {token}")

dispatch_sig = "void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm)"
pe_dispatch = body(pe, dispatch_sig)
case_start = pe_dispatch.index("case 0xE:")
case_end = pe_dispatch.index("case 0xF:", case_start)
require("Render3DSphere(this, wtm)" in pe_dispatch[case_start:case_end], "PE primitive id 0x0E no longer dispatches sphere")
# The primary Ref file has no Render dispatcher; the PE switch is the authority
# for primitive id 0x0E, while local parent routing is checked below.

helper = body(local, "void RenderReferenceSphere(const CEffectPrim& prim, const matrix& view)")
ordered = (
    "prim.m_texture.empty()",
    "texture == &CTexMgr::s_dummy_texture",
    "GetRadian(latitude)", "GetRadian(longitude)",
    "first[3]", "second[3]",
    "firstUV[3][2]", "secondUV[3][2]",
    "SubmitReferenceEffectTriangle(first,view,texture,prim.m_argb,firstUV,1)",
    "SubmitReferenceEffectTriangle(second,view,texture,prim.m_argb,secondUV,1)",
)
positions = [helper.find(token) for token in ordered]
require(all(pos >= 0 for pos in positions) and positions == sorted(positions), f"local sphere mesh/state ordering differs: {list(zip(ordered, positions))}")
require("prim.m_arcAngle > 0.0f && prim.m_arcAngle <= 180.0f" in local,
        "local sphere arc guard changed")
require("last?static_cast<float>(z+prim.m_pos.z):static_cast<float>(z)+prim.m_pos.z" in compact(helper),
        "sphere's source last-corner addition order changed")
submit = body(local, "void SubmitReferenceEffectTriangle(")
require("face->srcAlphaMode = (renderFlag & 2) != 0 ? D3DBLEND_ONE : D3DBLEND_SRCALPHA" in submit
        and "face->destAlphaMode = (renderFlag & 6) != 0 ? D3DBLEND_ONE : D3DBLEND_INVSRCALPHA" in submit
        and "g_renderer.AddReferenceEffectTriangle(face, renderFlag)" in submit,
        "shared triangle alpha modes or render-flag queue routing changed")

route = body(local, "bool CEffectPrim::UsesReferenceSphere() const")
require(compact(route) == "boolCEffectPrim::UsesReferenceSphere()const{returnm_type==PP_3DSPHERE&&m_master&&(m_master->GetEffectType()==17||m_master->GetEffectType()==170);}",
        "sphere parent-effect route set changed (expected 17 and 170)")
local_dispatch = body(local, "void CEffectPrim::Render(matrix* viewMatrix)")
require("if (UsesReferenceSphere())" in local_dispatch and "RenderReferenceSphere(*this,*viewMatrix);" in local_dispatch,
        "local sphere renderer route missing")

for token in (
    'sphere.m_texture={&g_texMgr.texture}', 'sphere.m_argb=0x10FFFFFF',
    'sphere.m_renderFlag=0x205', 'g_renderer.m_rpAlphaList.size()==16',
    'point(-90,10)', 'point(0,101)', 'a->m_verts[0].tu==0',
    'a->tex==&g_texMgr.texture', 'g_renderer.m_rpAlphaNoDepthList.empty()',
    'sphere.m_arcAngle=36', 'sphere.m_arcAngle=72.5f',
    'sphere.m_texture.clear()',
):
    require(token in tests, f"focused sphere vertex/state fixture missing {token}")

print("Render3DSphere PE/primary-Ref body, PP_3DSPHERE dispatch, local parent route, mesh and state fixture passed")
