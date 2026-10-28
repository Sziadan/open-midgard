#!/usr/bin/env python3
"""Source map for the explicit-master PP_3DCYLINDER renderer subset."""
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


SIG = "void __thiscall CEffectPrim::Render3DCylinder(CEffectPrim *this, matrix *wtm)"
PE = PE[PE.index(SIG + "; // idb") + 1:]
PE_RENDER = body(PE, SIG)
REF_RENDER = body(REF, SIG)
PE_DISPATCH = body(PE, "void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm)",
                   PE.index("void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm); // idb") + 1)
REF_DISPATCH = body(REF_DISPATCH, "void\tCEffectPrim :: Render(matrix &wtm/*view matrix*/)")
LOCAL_RENDER = body(CURRENT, "void CEffectPrim::Render(matrix* viewMatrix)")
LOCAL_PREDICATE = body(CURRENT, "bool CEffectPrim::UsesReferenceCylinder() const")
LOCAL_CYLINDER = body(CURRENT, "void RenderReferenceCylinder(")
SUBMIT = body(CURRENT, "void SubmitReferenceEffectTriangle(")

check("canonical PE and primary Ref Render3DCylinder bodies normalize identically",
      re.sub(r"\s+", " ", PE_RENDER).strip() == re.sub(r"\s+", " ", REF_RENDER).strip())
check("PE and primary Ref dispatch primitive id 0x0D to Render3DCylinder",
      re.search(r"case\s+0xD\s*:\s*CEffectPrim::Render3DCylinder\(this, wtm\)", PE_DISPATCH) is not None
      and re.search(r"case\s+PP_3DCYLINDER\s*:\s*Render3DCylinder\(wtm\)", REF_DISPATCH) is not None)
check("local enum retains canonical PP_3DCYLINDER id and selects the specialized renderer",
      re.search(r"PP_3DCYLINDER\s*=\s*0xD", HEADER) is not None
      and "if (UsesReferenceCylinder())" in LOCAL_RENDER
      and "RenderReferenceCylinder(*this, *viewMatrix);" in LOCAL_RENDER
      and "case PP_3DCYLINDER:" in LOCAL_RENDER and "RenderCircleLayer(*this" in LOCAL_RENDER)

expected_masters = {0, 2, 3, 7, 14, 34, 36, 73, 81, 115, 122, 138, 140, 180, 184,
                    218, 219, 220, 221}
actual_masters = {int(v) for v in re.findall(r"case\s+(\d+)\s*:", LOCAL_PREDICATE)}
check("local source-selected cylinder route matches the documented master-effect ID union",
      actual_masters == expected_masters and "m_type != PP_3DCYLINDER" in LOCAL_PREDICATE)

# The PE accumulates a floating angle but truncates each direct trig-table
# index, writes top cosine products to float before the matrix, and submits
# both triangle faces with the same texture/color and hard-coded render flag.
pe_tokens = (
    "uInc = 0.0", "i = 0.0", "temp = 360.0 - this->m_arcAngle", "if ( temp >= 0.0 )",
    "v3 = g_cosTable[(__int64)i]", "v4 = i + this->m_arcAngle", "v5 = (__int64)v4",
    "v61 = v3 * this->m_outerSize", "v62 = *((float *)&v5 + 1) * this->m_outerSize",
    "vec1.x = vec1.x + this->m_pos.x", "vec4.z = v14 + this->m_pos.z",
    "*p_tex = this->m_texture[this->m_curMotion]", "uInc = v72", "while ( v50 <= temp )",
)
missing = [token for token in pe_tokens if token not in PE_RENDER]
check("PE preserves floating angle progression, truncating table indices, float top products and current texture", not missing)
check("PE copies stored ARGB and submits both faces with hard-coded flag one",
      PE_RENDER.count("this->m_argb") >= 4 and PE_RENDER.count("this->m_texture[this->m_curMotion]") == 2
      and PE_RENDER.count("CRenderer::AddRP(g_renderer") == 2
      and "CRenderer::AddRP(g_renderer, rp, 1)" in PE_RENDER)

local_tokens = (
    "const float stop = 360.0f - prim.m_arcAngle", "float angle = 0.0f, u = 0.0f",
    "const double next = static_cast<double>(angle) + prim.m_arcAngle",
    "static_cast<int>(angle), nextAngle = static_cast<int>(next)",
    "static_cast<float>(g_cosTable[degrees] * radius)",
    "prim.m_texture[static_cast<size_t>(prim.m_curMotion)]", "prim.m_argb",
    "const vector3d first[3] = {a,c,b}, second[3] = {b,c,d}",
    "SubmitReferenceEffectTriangle(first, view, texture, prim.m_argb, firstUV, 1)",
    "SubmitReferenceEffectTriangle(second, view, texture, prim.m_argb, secondUV, 1)",
)
missing = [token for token in local_tokens if token not in LOCAL_CYLINDER]
check("local selected branch preserves source angle/index, top float, triangle order, current texture and flag one", not missing)
check("local invalid-motion/null/dummy resource guards are explicit defensive behavior beyond source preconditions",
      "prim.m_curMotion < 0 || prim.m_curMotion >= static_cast<int>(prim.m_texture.size())" in LOCAL_CYLINDER
      and "if (!texture || texture == &CTexMgr::s_dummy_texture) return;" in LOCAL_CYLINDER
      and "this->m_texture[this->m_curMotion]" in PE_RENDER)
check("reference triangle helper preserves ordinary-alpha flag-one material mapping",
      "face->srcAlphaMode = (renderFlag & 2) != 0 ? D3DBLEND_ONE : D3DBLEND_SRCALPHA" in SUBMIT
      and "face->destAlphaMode = (renderFlag & 6) != 0 ? D3DBLEND_ONE : D3DBLEND_INVSRCALPHA" in SUBMIT
      and "g_renderer.AddReferenceEffectTriangle(face, renderFlag)" in SUBMIT)

fixture_expectations = (
    "cylinder submits two independent triangles for each of four wedges",
    "cylinder first triangle preserves lower-start upper-start lower-end order and negative local height",
    "cylinder second triangle preserves lower-end upper-start upper-end order",
    "cylinder selects current texture motion instead of texture zero",
    "cylinder uses stored primitive packed color without reconstructing alpha",
    "cylinder hard-coded flag one ignores primitive renderFlag bits",
    "cylinder fractional loop truncates table index without truncating accumulated angle",
    "cylinder endpoint directly reads table index three-sixty",
    "invalid cylinder arc emits no replacement geometry",
)
check("existing cylinder fixture pins geometry/winding, UV wrap, texture, color, blend, transform and edge guards",
      all(text in FIXTURE for text in fixture_expectations))

# Nonpositive increments can make the source do loop fail to progress. The
# local guard is deliberate; do not execute such malformed source inputs.
check("nonpositive source arc is guarded locally rather than executed as a hanging loop",
      "if (!(prim.m_arcAngle > 0.0f && prim.m_arcAngle <= 360.0f)) return;" in LOCAL_CYLINDER
      and "invalid cylinder arc emits no replacement geometry" in FIXTURE)

print("PP_3DCYLINDER explicit-master subset comparator passed; generic master route remains separate.")
