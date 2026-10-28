#!/usr/bin/env python3
"""Canonical source and local-route checks for PP_3DCROSSTEXTURE rendering."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffectPrim.cpp").read_text(encoding="utf-8", errors="replace")
REF_DISPATCH = (ROOT / "Ref/dhxj/RagEffectPrim.cpp").read_text(encoding="cp1252", errors="replace")
CURRENT = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
TESTS = (ROOT / "tools/tests/ParticleRenderTests.cpp").read_text(encoding="utf-8")


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
    raise AssertionError(f"unclosed function body: {marker}")


def compact(text: str) -> str:
    return re.sub(r"\s+", " ", text).strip()


def check(label: str, condition: bool) -> None:
    assert condition, f"FAIL {label}"
    print(f"PASS {label}")


SIG = "void __thiscall CEffectPrim::Render3DCrossTexture(CEffectPrim *this, matrix *wtm)"
PE_RENDER = body(PE, SIG, PE.index("//----- (00646EC0)"))
REF_RENDER = body(REF, SIG)
LOCAL_RENDER = body(CURRENT, "void RenderReferenceCrossTexture(const CEffectPrim& prim, const matrix& view)")
SUBMIT = body(CURRENT, "void SubmitReferenceEffectTriangle(")
LOCAL_PREDICATE = body(CURRENT, "bool CEffectPrim::UsesReferenceCrossTexture() const")
LOCAL_DISPATCH = body(CURRENT, "void CEffectPrim::Render(matrix* viewMatrix)")
PE_DISPATCH = body(PE, "void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm)", PE.index("//----- (00649220)"))
REF_DISPATCH_BODY = body(REF_DISPATCH, "void\tCEffectPrim :: Render(matrix &wtm/*view matrix*/)")
PE_PROCESS = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)\n{")

check("canonical PE and primary Ref Render3DCrossTexture bodies normalize identically",
      compact(PE_RENDER) == compact(REF_RENDER))

# Every canonical PE LaunchEffectPrim call site for PP_3DCROSSTEXTURE is
# collected with its enclosing CRagEffect method, then mapped to OnProcess's
# effect id so the local legacy renderer route cannot silently omit a caller.
method_headers = list(re.finditer(
    r"(?:void|unsigned __int8|int|bool) __thiscall CRagEffect::(\w+)\(CRagEffect \*this[^\n]*\)", PE))
source_callers = set()
for call in re.finditer(r"CRagEffect::LaunchEffectPrim\(this, PP_3DCROSSTEXTURE,", PE):
    preceding = [header for header in method_headers if header.start() < call.start()]
    assert preceding, "LaunchEffectPrim call has no enclosing CRagEffect method"
    source_callers.add(preceding[-1].group(1))
expected_callers = {
    "Enhance", "IceArrow", "FireArrow", "IncAgility", "DecAgility",
    "IncAGIDEX", "BlitzBeat", "Potion_Berserk", "CartBoost",
}
check("all nine direct PE cross-texture callers are inventoried", source_callers == expected_callers)

caller_effects = {
    9: "Enhance", 26: "IceArrow", 31: "FireArrow", 37: "IncAgility",
    38: "DecAgility", 43: "IncAGIDEX", 115: "BlitzBeat",
    220: "Potion_Berserk", 456: "CartBoost",
}
for effect_id, method in caller_effects.items():
    check(f"PE OnProcess effect {effect_id} dispatches {method}",
          re.search(rf"case\s+{effect_id}\s*:\s*(?:result\s*=\s*)?CRagEffect::{method}\(this\)", PE_PROCESS) is not None)
local_effects = {int(value) for value in re.findall(r"m_master->GetEffectType\(\)\s*==\s*(\d+)", LOCAL_PREDICATE)}
check("local cross-texture predicate covers every PE caller and no unrelated source type",
      local_effects == set(caller_effects))
check("local Render dispatch reaches source cross-texture renderer for selected legacy effects",
      "if (UsesReferenceCrossTexture())" in LOCAL_DISPATCH
      and "RenderReferenceCrossTexture(*this, *viewMatrix);" in LOCAL_DISPATCH)

check("PE and Ref primitive render dispatch always route type 0x10 to Render3DCrossTexture",
      re.search(r"case\s+0x10\s*:\s*CEffectPrim::Render3DCrossTexture\(this, wtm\)", PE_DISPATCH) is not None
      and re.search(r"case\s+PP_3DCROSSTEXTURE\s*:\s*Render3DCrossTexture\(wtm\)", REF_DISPATCH_BODY) is not None)

check("two four-corner strips use source width/height axes and matrix basis plus translation",
      "const vector3d local[2][4]" in LOCAL_RENDER
      and "{{-h,0,w},{-h,0,-w},{h,0,w},{h,0,-w}}" in LOCAL_RENDER
      and "{{0,-h,w},{0,-h,-w},{0,h,w},{0,h,-w}}" in LOCAL_RENDER
      and "static_cast<double>(prim.m_matrix.m[0][axis]) * v.x" in LOCAL_RENDER
      and "prim.m_matrix.m[1][axis]" in LOCAL_RENDER
      and "prim.m_matrix.m[2][axis]" in LOCAL_RENDER
      and "prim.m_matrix.m[3][axis]" in LOCAL_RENDER
      and "if (i == 0 && axis == 0) transformed[axis] = static_cast<float>(coordinate + prim.m_pos.x);" in LOCAL_RENDER)
check("four triangles preserve PE vertex winding, UVs, texture, color, and hard-coded submission flag 1",
      "const vector3d first[3] = {points[0],points[1],points[2]};" in LOCAL_RENDER
      and "const vector3d second[3] = {points[3],points[2],points[1]};" in LOCAL_RENDER
      and "{{0,1},{1,1},{0,0}}" in LOCAL_RENDER
      and "{{1,0},{0,0},{1,1}}" in LOCAL_RENDER
      and "prim.m_argb" in LOCAL_RENDER
      and "prim.m_texture[static_cast<size_t>(prim.m_curMotion)]" in LOCAL_RENDER
      and "SubmitReferenceEffectTriangle(first, view, texture, prim.m_argb, firstUV, 1);" in LOCAL_RENDER
      and "SubmitReferenceEffectTriangle(second, view, texture, prim.m_argb, secondUV, 1);" in LOCAL_RENDER
      and "face->srcAlphaMode = (renderFlag & 2) != 0 ? D3DBLEND_ONE : D3DBLEND_SRCALPHA;" in SUBMIT
      and "face->destAlphaMode = (renderFlag & 6) != 0 ? D3DBLEND_ONE : D3DBLEND_INVSRCALPHA;" in SUBMIT
      and "g_renderer.AddReferenceEffectTriangle(face, renderFlag);" in SUBMIT)
check("existing geometry fixture checks all four triangles, both strips, UV/winding, matrix, stored color, texture gate and flag 1",
      "RenderReferenceCrossTexture(cross,view)" in TESTS
      and "cross.m_renderFlag=0x205" in TESTS
      and "cross texture emits four triangles across two perpendicular strips" in TESTS
      and "cross texture preserves original triangle UV order" in TESTS
      and "cross texture uses launch matrix instead of camera billboard orientation" in TESTS
      and "cross texture uses stored color and hard-coded alpha flag one" in TESTS
      and "cross missing texture emits no glow substitute" in TESTS)

print("PP_3DCROSSTEXTURE renderer comparator passed.")
