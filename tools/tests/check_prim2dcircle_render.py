#!/usr/bin/env python3
"""Source map for the explicit-master PP_2DCIRCLE renderer subset."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
PE_CALLERS = PE
REF = (ROOT / "Ref/RagEffectPrim.cpp").read_text(encoding="cp1252", errors="replace")
REF_CALLERS = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="cp1252", errors="replace")
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


SIG = "void __thiscall CEffectPrim::Render2DCircle(CEffectPrim *this)"
PE = PE[PE.index(SIG + "; // idb") + 1:]
PE_RENDER = body(PE, SIG)
REF_RENDER = body(REF, SIG)
PE_DISPATCH = body(PE, "void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm)",
                   PE.index("void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm); // idb") + 1)
REF_DISPATCH = body(REF_DISPATCH, "void\tCEffectPrim :: Render(matrix &wtm/*view matrix*/)")
LOCAL_RENDER = body(CURRENT, "void CEffectPrim::Render(matrix* viewMatrix)")
LOCAL_PREDICATE = body(CURRENT, "bool CEffectPrim::UsesReferenceScreenCircle() const")
LOCAL_CIRCLE = body(CURRENT, "void RenderReferenceScreenCircle(")

check("canonical PE and primary Ref Render2DCircle bodies normalize identically",
      re.sub(r"\s+", " ", PE_RENDER).strip() == re.sub(r"\s+", " ", REF_RENDER).strip())
check("PE and primary Ref dispatch primitive id 0x04 to Render2DCircle",
      re.search(r"case\s+4\s*:\s*CEffectPrim::Render2DCircle\(this\)", PE_DISPATCH) is not None
      and re.search(r"case\s+PP_2DCIRCLE\s*:\s*Render2DCircle\(\)", REF_DISPATCH) is not None)
check("local enum preserves PP_2DCIRCLE id 0x04",
      re.search(r"PP_2DCIRCLE\s*=\s*0x4", HEADER) is not None)

expected_masters = {16, 40, 82, 121, 128}
actual_masters = {int(value) for value in re.findall(r"GetEffectType\(\)\s*==\s*(\d+)", LOCAL_PREDICATE)}
check("local source-selected circle subset uses exactly the five current master-effect IDs",
      actual_masters == expected_masters and "m_type == PP_2DCIRCLE" in LOCAL_PREDICATE)
check("selected PP_2DCIRCLE instances use RenderReferenceScreenCircle and other masters keep the generic quad route",
      "if (UsesReferenceScreenCircle())" in LOCAL_RENDER
      and "RenderReferenceScreenCircle(*this);" in LOCAL_RENDER
      and "case PP_2DCIRCLE:" in LOCAL_RENDER
      and "SubmitScreenQuad(anchor," in LOCAL_RENDER)

source_steps = (
    "uInc = 0.0", "i = 0.0", "innerSize = this->m_radius - this->m_size",
    "temp = 360.0 - this->m_arcAngle", "v4 = g_cosTable[(__int64)i]",
    "sn1 = g_sinTable[(__int64)i]", "v6 = (__int64)(i + this->m_arcAngle)",
    "cs2 = g_cosTable[v6]", "sn2 = g_sinTable[v6]",
    "m_rp->verts[0].x = v5 + this->m_pos.x", "v51->x = cs2 * this->m_radius + this->m_pos.x",
    "v37->x = v4 * this->m_radius + this->m_pos.x", "*p_tex = *this->m_texture",
    "CRenderer::AddRP(g_renderer, v10, 1)", "*v8 = v14 + this->m_pos.x",
    "*v52 = cs2 * innerSize + this->m_pos.x", "*v38 = cs2 * this->m_radius + this->m_pos.x",
    "*v20 = *this->m_texture", "CRenderer::AddRP(g_renderer, v11, 1)",
    "uInc = v16", "if ( v16 >= 1.0 )", "while ( v9 <= temp )",
)
for source in (PE_RENDER, REF_RENDER):
    missing = [step for step in source_steps if step not in source]
    check("PE/primary Ref preserve source annulus steps, two faces, slot-zero texture and flag-one queue",
          not missing)

local_steps = (
    "const float inner = prim.m_radius-prim.m_size, stop = 360.0f-prim.m_arcAngle",
    "float angle = 0.0f, u = 0.0f", "static_cast<int>(angle)",
    "static_cast<int>(next)", "g_cosTable[first]", "g_sinTable[first]",
    "prim.m_pos.y-innerY", "static_cast<double>(sine2)*prim.m_radius",
    "prim.m_texture[0]", "prim.m_argb", "u+0.25f",
    "u = nextU >= 1.0f ? 0.0f : nextU", "g_renderer.AddReferenceEffectTriangle(face,1)",
)
missing_local = [step for step in local_steps if step not in LOCAL_CIRCLE]
check("local circle renderer maps annulus geometry, endpoint truncation, fixed texture/color and two flag-one submissions",
      not missing_local)
check("source loop exit compares accumulated float angle with unrounded stop",
      "i = v9" in PE_RENDER and "while ( v9 <= temp )" in PE_RENDER
      and "angle = static_cast<float>(next)" in LOCAL_CIRCLE
      and "if (next > stop) break;" in LOCAL_CIRCLE)
check("all current PP_2DCIRCLE launch sites belong to the specialized five-master subset",
      len(re.findall(r"LaunchEffectPrim\(\s*PP_2DCIRCLE\s*,", CURRENT)) == 5
      and all(name in CURRENT for name in (
          "CRagEffect::ApplyBash", "CRagEffect::ProcessSignum", "CRagEffect::ApplyOverThrust",
          "CRagEffect::ApplySonicBlow", "CRagEffect::SpawnTurnUndead")))
check("canonical PE and primary Ref caller inventories contain the same five PP_2DCIRCLE launch sites",
      len(re.findall(r"LaunchEffectPrim\([^;\n]*PP_2DCIRCLE", PE_CALLERS)) == 5
      and len(re.findall(r"LaunchEffectPrim\([^;\n]*PP_2DCIRCLE", REF_CALLERS)) == 5
      and len(re.findall(r"LaunchEffectPrim\(\s*PP_2DCIRCLE\s*,", CURRENT)) == 5)

fixture_expectations = (
    "OverThrust screen circle submits two triangles per source wedge",
    "OverThrust first wedge preserves inner-start outer-end outer-start order without half-pixel shift",
    "OverThrust second wedge preserves inner-start inner-end outer-end order",
    "OverThrust circle preserves original quarter-phase wedge UVs",
    "OverThrust circle preserves fixed source depth packed alpha texture zero and hard-coded flag one",
    "OverThrust ten-degree circle uses seventy-two triangles and wraps UV every four wedges",
    "screen circle retains fractional arc accumulation before integer endpoint lookup",
    "screen circle endpoint directly reads original table index three-sixty",
    "screen circle preserves source first-inner double product versus second-inner stored float temporary",
    "OverThrust missing circle texture emits no replacement geometry",
)
check("existing screen-circle fixture pins triangle geometry/order, UV, color, transform inputs and edge guards",
      all(expectation in FIXTURE for expectation in fixture_expectations))
print("PP_2DCIRCLE explicit-master subset comparator passed; generic masters remain separate.")
