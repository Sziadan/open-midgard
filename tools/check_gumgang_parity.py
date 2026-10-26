#!/usr/bin/env python3
"""Mechanically compare the canonical GumGang renderer branches with the port."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
IMPL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")


def body(text: str, marker: str) -> str:
    start = text.index(marker)
    opening = text.index("{", start)
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[opening + 1 : pos]
    raise AssertionError(f"unclosed function: {marker}")


source = body(EXE, "void __thiscall CEffectPrim::RenderGumGang(\n")
dispatch = body(EXE, "void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm)\n")
tei_rect = body(EXE, "void __thiscall CEffectPrim::RenderTeiRect(\n        CEffectPrim *this,\n        int *rpCnt,\n        __int16 alpha,")
project_vertex = body(EXE, "void __thiscall CRenderer::ProjectVertex(CRenderer *this, const vector3d *src, const matrix *vtm, tlvertex3d *vert)\n")
trig_init = body(EXE, "void __cdecl CreateTrigonometricTable()\n")
impl = body(IMPL, "void RenderGumGang(CEffectPrim& prim,")
impl_helpers = body(IMPL, "bool GumGangFrontFacing(")
submit = body(IMPL, "void SubmitGumGangRect(")

checks = {
    "canonical 0x23 route and default RGB/scale": (
        r"case 0x23:\s*CEffectPrim::RenderGumGang\(this, wtm, 1\.0, 0x64u, 0x64u, 0xFFu\)",
        r"m_type == PP_GUMGANG\)\s*\{\s*RenderGumGang\(\*this, \*viewMatrix, 1\.0f, 100, 100, 255\)",
    ),
    "four GI bands and active-life gate": (r"v173 = 4;[\s\S]*?p_z \+= 45;", r"band < 4[\s\S]*?gi\.life != 1"),
    "band red override uses flag1[4] and persists across bands": (r"\*\(\(_BYTE \*\)p_z - 56\) == 1[\s\S]*?R = -1;", r"gi\.flag\[4\] == 1[\s\S]*?red = 255;\s*green = 20;\s*blue = 20;"),
    "distance scales the four 45-degree base corners": (r"v9 = Rdist \* \*\(p_z - 9\);[\s\S]*?flt_72D930 \* v9", r"gi\.distance\) \* radiusScale[\s\S]*?45 \+ 90 \* corner"),
    "reference base-coordinate globals map to trig-table indices 45,135,225,315": (r"flt_72D4F8 \* v9[\s\S]*?flt_72CDE8 \* v9", r"int angle = 45 \+ 90 \* corner[\s\S]*?g_cosTable\[angle\][\s\S]*?g_sinTable\[angle\]"),
    "shared view rotation uses GI0/GI1 alphaT": (r"Tei_TowardScreen\(this, &vec1, &vec2, &vec3, &vec4, this->m_GI\[0\]\.alphaT, alphaT\)", r"IntimidateTowardScreen\(baseCorners, prim\.m_gi\[0\]\.alphaT, prim\.m_gi\[1\]\.alphaT\)"),
    "four source strip angle slots map to implementation": (r"p_z - 33[\s\S]*?p_z - 29[\s\S]*?p_z - 25[\s\S]*?p_z - 21", r"angleSlots\[4\] = \{ 3, 7, 11, 15 \}"),
    "four source alpha slots map to implementation": (r"p_z - 35[\s\S]*?p_z - 31[\s\S]*?p_z - 27[\s\S]*?p_z - 23", r"alphaSlots\[4\] = \{ 1, 5, 9, 13 \}"),
    "source blend/orientation and texture slots map to implementation": (r"p_z - 34[\s\S]*?p_z - 30[\s\S]*?p_z - 26[\s\S]*?p_z - 22", r"orientationSlots\[4\] = \{ 2, 6, 10, 14 \}[\s\S]*?textureSlots\[4\] = \{ 2, 6, 10, 14 \}"),
    "source caches four angle vectors at scale four": (r"g_cosTable\[\(__int64\)\*\(p_z - 33\)\] \* 4\.0[\s\S]*?g_sinTable\[\(__int64\)\*\(p_z - 21\)\] \* 4\.0", r"g_cosTable\[angle\]\) \* scale[\s\S]*?g_sinTable\[angle\]\) \* scale"),
    "four independent RenderTeiRect calls, one per strip": (r"CEffectPrim::RenderTeiRect\([\s\S]*?CEffectPrim::RenderTeiRect\([\s\S]*?CEffectPrim::RenderTeiRect\([\s\S]*?CEffectPrim::RenderTeiRect\(", r"for \(int strip = 0; strip < 4; \+\+strip\)[\s\S]*?SubmitGumGangRect\("),
    "each facing test truncates difference and applies asymmetric bounds": (r"\(__int64\)v13[\s\S]*?v14 < 70 && v14 > -80 \|\| v14 < -270 \|\| v14 > 270", r"static_cast<int>\(static_cast<double>\(angle\) - screenAngle\)[\s\S]*?difference < 70 && difference > -80[\s\S]*?difference < -270 \|\| difference > 270"),
    "mode 0/2 uses normal facing and all other modes invert": (r"\*\(p_z - 34\) == 0\.0 \|\| \*\(\(_DWORD \*\)p_z - 34\) == 0x40000000", r"orientation == 0\.0f \|\| orientation == 2\.0f[\s\S]*?normalOrder \? front : !front"),
    "source reverse order swaps corners 0/1 and 2/3": (r"v15 = vec2\.x[\s\S]*?v17 = vec5\.x[\s\S]*?v20 = vec4\.x[\s\S]*?v23 = vec7\.x", r"std::swap\(stripCorners\[0\], stripCorners\[1\]\);\s*std::swap\(stripCorners\[2\], stripCorners\[3\]\)"),
    "GumGang supplies four per-strip texture indices, alphas, RGB and source UV rectangle": (r"CEffectPrim::RenderTeiRect\([\s\S]*?CEffectPrim::RenderTeiRect\([\s\S]*?CEffectPrim::RenderTeiRect\([\s\S]*?CEffectPrim::RenderTeiRect\(", r"m_texture\[static_cast<size_t>\(textureIndex\)\][\s\S]*?static_cast<short>\(gi\.height\[static_cast<size_t>\(alphaSlots\[strip\]\)\]\)[\s\S]*?red, green, blue"),
    "RenderTeiRect adds m_pos, selects TN texture, and uses inverse-alpha blend": (r"vec1\.x = vec1\.x \+ this->m_pos\.x[\s\S]*?m_texture\[v29\][\s\S]*?PW == 0[\s\S]*?destAlphaMode = 2", r"AddVec3\(corners\[0\], translation\)[\s\S]*?face->tex = texture[\s\S]*?D3DBLEND_INVSRCALPHA"),
    "RenderTeiRect preserves packed alpha/RGB and fixed U/V corner mapping": (r"m_argb_B = b[\s\S]*?verts\[0\]\.tu = v21[\s\S]*?verts\[1\]\.tu = v25", r"const float uvs\[2\]\[3\]\[2\] = \{[\s\S]*?\{ 0\.0f, 1\.0f \}, \{ 1\.0f, 1\.0f \}, \{ 1\.0f, 0\.0f \} \}[\s\S]*?const unsigned int color = "),
    "tlvertex projection uses reciprocal depth, half-screen offsets, and no cutoff": (r"vert->oow = v4;[\s\S]*?this->m_halfWidth[\s\S]*?this->m_halfHeight[\s\S]*?vert->z =", r"vertex\.oow = static_cast<float>\(reciprocal\)[\s\S]*?g_renderer\.m_halfWidth[\s\S]*?g_renderer\.m_halfHeight[\s\S]*?vertex\.z ="),
}

for name, (source_pattern, impl_pattern) in checks.items():
    source_target = dispatch if name == "canonical 0x23 route and default RGB/scale" else tei_rect if name.startswith("RenderTeiRect") else project_vertex if name.startswith("tlvertex") else source
    assert re.search(source_pattern, source_target, re.S), f"reference missing: {name}"
    target = IMPL if name == "canonical 0x23 route and default RGB/scale" else impl_helpers if name in (
        "each facing test truncates difference and applies asymmetric bounds", "mode 0/2 uses normal facing and all other modes invert"
    ) else submit if name.startswith("RenderTeiRect") else body(IMPL, "void ProjectReferenceEffectVertex(") if name.startswith("tlvertex") else impl
    assert re.search(impl_pattern, target, re.S), f"implementation missing: {name}"
    print(f"PASS {name}")

assert source.count("CEffectPrim::RenderTeiRect(") == 4, "expected exactly four reference strip submissions"
assert "case 0x63:" in dispatch, "canonical 0x63 route missing"
assert re.search(r"case 0x63:\s*CEffectPrim::RenderDoubleGumGang\(this, wtm\)", dispatch), "canonical 0x63 dispatch target mismatch"
assert not re.search(r"m_type == PP_DOUBLEGUMGANG", IMPL), "DoubleGumGang must remain a separate unported function"
print("PASS 0x63 remains separately routed to RenderDoubleGumGang")
print(f"{len(checks) + 1} mechanical RenderGumGang source/implementation comparisons passed")
