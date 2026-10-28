#!/usr/bin/env python3
"""Source map for the selected PP_2DTEXTURE screen-sprite renderer."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffectPrim.cpp").read_text(encoding="cp1252", errors="replace")
REF_DISPATCH = (ROOT / "Ref/dhxj/RagEffectPrim.cpp").read_text(encoding="cp1252", errors="replace")
CURRENT = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
FIXTURE = (ROOT / "tools/tests/ParticleRenderTests.cpp").read_text(encoding="utf-8")


def body(text: str, marker: str, start: int = 0) -> str:
    index = text.index(marker, start)
    opening = text.index("{", index)
    depth = 0
    for cursor in range(opening, len(text)):
        if text[cursor] == "{":
            depth += 1
        elif text[cursor] == "}":
            depth -= 1
            if depth == 0:
                return text[opening + 1:cursor]
    raise AssertionError(f"unclosed body: {marker}")


def check(label: str, condition: bool) -> None:
    assert condition, f"FAIL {label}"
    print(f"PASS {label}")


SIG = "void __thiscall CEffectPrim::Render2DTexture(CEffectPrim *this)"
PE = PE[PE.index(SIG + "; // idb") + 1:]
PE_RENDER = body(PE, SIG)
REF_RENDER = body(REF, SIG)
PE_DISPATCH = body(PE, "void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm)",
                   PE.index("void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm); // idb") + 1)
REF_DISPATCH = body(REF_DISPATCH, "void\tCEffectPrim :: Render(matrix &wtm/*view matrix*/)")
LOCAL_RENDER = body(CURRENT, "void CEffectPrim::Render(matrix* viewMatrix)")
LOCAL_PREDICATE = body(CURRENT, "bool CEffectPrim::UsesReferenceScreenTexture() const")
LOCAL_SCREEN = body(CURRENT, "void RenderReferenceScreenTexture(")
SUBMIT = body(CURRENT, "void SubmitReferenceEffectTriangle(")

check("canonical PE and primary Ref Render2DTexture bodies normalize identically",
      re.sub(r"\s+", " ", PE_RENDER).strip() == re.sub(r"\s+", " ", REF_RENDER).strip())
check("PE dispatch primitive ids 0x00 and 0x01 to Render2DTexture",
      re.search(r"case 0:\s*case 1:\s*CEffectPrim::Render2DTexture\(this\)", PE_DISPATCH) is not None)
check("primary Ref dispatch PP_2DTEXTURE and PP_2DTEXTUREORBIT to Render2DTexture",
      re.search(r"case\s+PP_2DTEXTURE:\s*case\s+PP_2DTEXTUREORBIT:\s*Render2DTexture\(\)", REF_DISPATCH) is not None)
check("canonical local enum preserves PP_2DTEXTURE id zero", re.search(r"PP_2DTEXTURE\s*=\s*0x0", HEADER) is not None)

expected_masters = {11, 38, 37, 43, 32, 4, 5, 1, 183, 46, 50, 51}
actual_masters = {int(v) for v in re.findall(r"m_master->GetEffectType\(\)\s*==\s*(\d+)", LOCAL_PREDICATE)}
check("local selected PP_2DTEXTURE master set is exact",
      expected_masters == actual_masters and "m_type == PP_2DTEXTURE" in LOCAL_PREDICATE)
check("local PP_2DTEXTURE pattern bit 0x4 enters the screen-texture renderer independently",
      "m_type == PP_2DTEXTURE && (m_pattern & 4) != 0" in LOCAL_RENDER
      and "RenderReferenceScreenTexture(*this);" in LOCAL_RENDER
      and LOCAL_RENDER.index("m_type == PP_2DTEXTURE && (m_pattern & 4) != 0")
      < LOCAL_RENDER.index("UsesReferenceScreenTexture()"))
check("nonselected PP_2DTEXTURE remains a separate generic screen-quad route",
      "case PP_2DTEXTURE:" in LOCAL_RENDER and "case PP_2DFLASH:" in LOCAL_RENDER
      and "SubmitScreenQuad(anchor," in LOCAL_RENDER)

source_steps = [
    "GetSin((__int64)this->m_roll)", "GetCos((__int64)this->m_roll)",
    "if ( (this->m_pattern & 1) != 0 )", "v5 = m_heightSize * -2.0",
    "rx = v27 - v31", "ry = v6 + v29", "rx_8 = v27 - v30", "rx_12 = v26 - v30",
    "this->m_rp[0].verts[0].x = rx_8", "this->m_rp[0].verts[0].y = v9 + v6",
    "this->m_rp[0].verts[1].x = rx", "this->m_rp[0].verts[1].y = ry",
    "this->m_rp[0].verts[2].x = rx_12", "this->m_rp[0].verts[2].y = v10",
    "this->m_rp[0].tex = m_texture[m_curMotion]", "CRenderer::AddRP(g_renderer, this->m_rp, 1)",
    "this->m_rp[1].verts[0].x = v14 - 0.5", "this->m_rp[1].verts[1].x = rx + this->m_pos.x - 0.5",
    "this->m_rp[1].verts[2].x = rx_4 + this->m_pos.x - 0.5",
    "this->m_rp[1].tex = v16[v18]", "CRenderer::AddRP(g_renderer, &this->m_rp[1], 1)",
]
for step in source_steps:
    check(f"PE/Ref screen-render anchor: {step}", step in PE_RENDER and step in REF_RENDER)
check("source explicitly uses the integer roll for trig-table calls and pattern-dependent pivot",
      "GetSin((__int64)this->m_roll)" in PE_RENDER
      and "GetCos((__int64)this->m_roll)" in PE_RENDER
      and "y1 = 0.0" in PE_RENDER and "v5 = m_heightSize * -2.0" in PE_RENDER)
check("source emits two separate triangles with matching current-motion texture and hard-coded flag one",
      PE_RENDER.count("CRenderer::AddRP") == 2
      and "m_texture[m_curMotion]" in PE_RENDER
      and "this->m_rp[1].tex = v16[v18]" in PE_RENDER
      and "AddRP(g_renderer, this->m_rp, 1)" in PE_RENDER
      and "AddRP(g_renderer, &this->m_rp[1], 1)" in PE_RENDER)

local_steps = [
    "GetSin(static_cast<int>(prim.m_roll))", "GetCos(static_cast<int>(prim.m_roll))",
    "(prim.m_pattern & 1) != 0", "static_cast<double>(h) * -2.0",
    "leftCos-topSin", "bottomCos+leftSin", "rightCos-topSin", "rightSin+topCos",
    "const int order[2][3] = {{0,1,2},{2,1,3}}",
    "prim.m_texture[static_cast<size_t>(prim.m_curMotion)]",
    "vertex.z = 0.000001f; vertex.oow = 0.99999899f",
    "vertex.color = prim.m_argb", "vertex.specular = 0xFF000000u",
    "g_renderer.AddReferenceEffectTriangle(face, 1)",
]
for step in local_steps:
    check(f"local screen-render mapping: {step}", step in LOCAL_SCREEN)
check("reference triangle helper preserves hard-coded flag-one alpha blend and queue",
      "(renderFlag & 2) != 0 ? D3DBLEND_ONE : D3DBLEND_SRCALPHA" in SUBMIT
      and "(renderFlag & 6) != 0 ? D3DBLEND_ONE : D3DBLEND_INVSRCALPHA" in SUBMIT
      and "g_renderer.AddReferenceEffectTriangle(face, renderFlag)" in SUBMIT)
check("existing screen sprite fixtures pin pattern, integer roll, two faces, UV/color, texture motion and flag one",
      all(token in FIXTURE for token in (
          "SandWind submits exactly two screen triangles", "SandWind preserves source screen UV order",
          "SandWind preserves fixed depth reciprocal stored color and current texture",
          "SandWind uses original hard-coded alpha flag one regardless of renderFlag",
          "Hit2 bottom-pivot geometry extends twice source height above anchor",
          "Hit2 screen renderer truncates roll before source trig-table rotation",
          "Hit5/6 centered rotated geometry retains source pivot rather than Hit2 bottom origin",
          "Napalm screen renderer selects advanced animation texture and packed rising alpha",
      )))
print("PP_2DTEXTURE selected screen-render subset comparator passed; generic route is separate.")
