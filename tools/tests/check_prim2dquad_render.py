#!/usr/bin/env python3
"""Source map for the PP_2DQUAD / ColdHit screen shard renderer."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
PE_CALLERS = PE
REF = (ROOT / "Ref/RagEffectPrim.cpp").read_text(encoding="cp1252", errors="replace")
REF_EFFECTS = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="cp1252", errors="replace")
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


SIG = "void __thiscall CEffectPrim::Render2DQuad(CEffectPrim *this)"
PE = PE[PE.index(SIG + "; // idb") + 1:]
PE_RENDER = body(PE, SIG)
REF_RENDER = body(REF, SIG)
PE_DISPATCH = body(PE, "void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm)",
                   PE.index("void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm); // idb") + 1)
REF_DISPATCH = body(REF_DISPATCH, "void\tCEffectPrim :: Render(matrix &wtm/*view matrix*/)")
LOCAL_RENDER = body(CURRENT, "void CEffectPrim::Render(matrix* viewMatrix)")
LOCAL_QUAD = body(CURRENT, "void RenderReferenceScreenQuad(")
LOCAL_PREDICATE = body(CURRENT, "bool CEffectPrim::UsesReferenceScreenQuad() const")
LOCAL_CALLER = body(CURRENT, "void CRagEffect::SpawnColdHit()")
PE_COLD_HIT_SIG = "void __thiscall CRagEffect::ColdHit(CRagEffect *this)"
PE_COLD_HIT = body(PE_CALLERS, PE_COLD_HIT_SIG,
                   PE_CALLERS.index(PE_COLD_HIT_SIG + "; // idb") + 1)
REF_COLD_HIT = body(REF_EFFECTS, "void __thiscall CRagEffect::ColdHit(CRagEffect *this)")
PE_PROCESS_SIG = "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)"
PE_PROCESS = body(PE_CALLERS, PE_PROCESS_SIG,
                  PE_CALLERS.index(PE_PROCESS_SIG + "; // idb") + 1)
LOCAL_PROCESS = body(CURRENT, "bool CRagEffect::ProcessEffectFrameLegacy()")

check("canonical PE and primary Ref Render2DQuad bodies normalize identically",
      re.sub(r"\s+", " ", PE_RENDER).strip() == re.sub(r"\s+", " ", REF_RENDER).strip())
check("PE dispatch primitive id 0x02 to Render2DQuad",
      re.search(r"case\s+2\s*:\s*CEffectPrim::Render2DQuad\(this\)", PE_DISPATCH) is not None)
check("readable Ref dispatch routes PP_2DQUAD to Render2DQuad",
      re.search(r"case\s+PP_2DQUAD\s*:\s*Render2DQuad\(\)", REF_DISPATCH) is not None)
check("local enum retains canonical PP_2DQUAD id 0x02",
      re.search(r"PP_2DQUAD\s*=\s*0x2", HEADER) is not None)

# Current code has one construction site (ColdHit/effect51). The primitive
# renderer itself is unconditional, matching the PE/Ref type dispatch.
quad_launches = re.findall(r"LaunchEffectPrim\(\s*PP_2DQUAD\s*,", CURRENT)
check("current local PP_2DQUAD construction is limited to the ColdHit caller",
      len(quad_launches) == 1 and "LaunchEffectPrim(PP_2DQUAD,vector3d{})" in LOCAL_CALLER)
check("canonical PE ColdHit handler launches PP_2DQUAD once",
      len(re.findall(r"LaunchEffectPrim\(this,\s*PP_2DQUAD,", PE_COLD_HIT)) == 1)
check("primary Ref ColdHit handler launches PP_2DQUAD once",
      len(re.findall(r"LaunchEffectPrim\(this,\s*PP_2DQUAD,", REF_COLD_HIT)) == 1)
check("canonical PE and primary Ref ColdHit handler bodies normalize identically",
      re.sub(r"\s+", " ", PE_COLD_HIT).strip() == re.sub(r"\s+", " ", REF_COLD_HIT).strip())
check("local ColdHit preserves source shard and smoke primitive launch routes",
      "LaunchEffectPrim(PP_2DQUAD,vector3d{})" in LOCAL_CALLER
      and "LaunchEffectPrim(PP_2DTEXTURE,vector3d{})" in LOCAL_CALLER
      and "if (m_stateCnt == 0)" in LOCAL_CALLER
      and "if (m_stateCnt == 0 || m_stateCnt == 7)" in LOCAL_CALLER)
check("PE effect 51 dispatches to ColdHit",
      re.search(r"case\s+51\s*:\s*CRagEffect::ColdHit\(this\)", PE_PROCESS) is not None)
check("local effect 51 dispatches through legacy frame processing",
      re.search(r"case\s+51\s*:\s*return\s+ProcessEffectFrameLegacy\(\)",
                (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")) is not None
      and re.search(r"if\s*\(m_type\s*==\s*51\)\s*\{\s*SpawnColdHit\(\);\s*return\s+true;\s*\}", LOCAL_PROCESS) is not None)
check("local PP_2DQUAD predicate follows primitive type regardless of master effect",
      "return m_type == PP_2DQUAD;" in LOCAL_PREDICATE
      and "if (UsesReferenceScreenQuad())" in LOCAL_RENDER
      and "RenderReferenceScreenQuad(*this);" in LOCAL_RENDER)

source_tokens = [
    "GetSin((__int64)this->m_roll)", "GetCos((__int64)this->m_roll)",
    "this->m_widthSize * 0.5", "this->m_heightSize * 0.5", "v4 * 0.83333331",
    "this->m_rp[0].verts[0].y = halfHeight1 - halfHeight",
    "this->m_rp[0].verts[1].y = halfHeight", "this->m_rp[0].verts[2].y = v16",
    "this->m_rp[0].verts[0].tu = 0.0", "this->m_rp[0].verts[0].tv = 0.5",
    "this->m_rp[0].verts[1].tu = 0.5", "this->m_rp[0].verts[1].tv = 1.0",
    "this->m_rp[0].verts[2].tu = 0.5", "this->m_rp[0].verts[2].tv = 0.0",
    "this->m_texture[m_curMotion]", "this->m_argb", "this->m_rp[0].verts[1].color = -1",
    "CRenderer::AddRP(g_renderer, this->m_rp, 1)",
    "CRenderer::AddRP(g_renderer, &this->m_rp[1], 1)",
]
for token in source_tokens:
    check(f"PE/Ref renderer anchor: {token}", token in PE_RENDER and token in REF_RENDER)
check("source transforms three vertices per face by integer-degree roll and screen position",
      "v7 * Cos - v8 * sn + this->m_pos.x" in PE_RENDER
      and "v8 * Cos + v7 * sn + this->m_pos.y" in PE_RENDER
      and "v14 * Cos - v15 * sn + this->m_pos.x" in PE_RENDER
      and "v15 * Cos + v14 * sn + this->m_pos.y" in PE_RENDER)
check("source emits exactly two texture-motion triangles with hard-coded AddRP flag one",
      PE_RENDER.count("CRenderer::AddRP") == 2
      and PE_RENDER.count("this->m_texture[m_curMotion]") == 1
      and PE_RENDER.count("this->m_curMotion") == 2)

local_tokens = [
    "GetSin(static_cast<int>(prim.m_roll))", "GetCos(static_cast<int>(prim.m_roll))",
    "prim.m_widthSize*0.5", "prim.m_heightSize*0.5", "prim.m_heightSize*0.5*0.83333331",
    "{{-halfWidth,inset-halfHeight},{0,halfHeight},{0,-halfHeight}}",
    "{{halfWidth,halfHeight-inset},{0,halfHeight},{0,-halfHeight}}",
    "vertex.x = static_cast<float>(x*cosine-y*sine+prim.m_pos.x)",
    "vertex.y = static_cast<float>(y*cosine+x*sine+prim.m_pos.y)",
    "prim.m_texture[static_cast<size_t>(prim.m_curMotion)]",
    "i == 1 ? 0xFFFFFFFFu : prim.m_argb", "vertex.z = 0.000001f; vertex.oow = 0.99999899f",
    "g_renderer.AddReferenceEffectTriangle(face,1)",
]
for token in local_tokens:
    check(f"local shard mapping: {token}", token in LOCAL_QUAD)
check("local helper emits the two source triangle faces and selected motion texture",
      "for (int side = 0; side < 2; ++side)" in LOCAL_QUAD
      and "prim.m_texture[static_cast<size_t>(prim.m_curMotion)]" in LOCAL_QUAD
      and "const float local[2][3][2]" in LOCAL_QUAD)
check("existing ColdHit fixture pins face geometry/order, UV, color, texture, screen depth and roll truncation",
      all(token in FIXTURE for token in (
          "ColdHit screen quad emits exactly two source shard triangles",
          "ColdHit first shard face uses half dimensions and source five-sixths inset",
          "ColdHit second shard face preserves opposite side inset and original shared-edge order",
          "ColdHit shard faces retain original side and center UVs",
          "ColdHit uses original white center vertex selected motion fixed screen depth and hard-coded alpha flag one",
          "ColdHit shard rotation truncates negative fractional roll",
          "ColdHit missing shard texture produces no fallback geometry")))
print("PP_2DQUAD renderer comparator passed; exact type route and current ColdHit caller are verified.")
