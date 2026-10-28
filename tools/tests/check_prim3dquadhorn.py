#!/usr/bin/env python3
"""Compare canonical PP_3DQUADHORN rendering with the local renderer and fixture."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffectPrim.cpp").read_text(encoding="utf-8", errors="replace")
REF_DISPATCH = (ROOT / "Ref/dhxj/RagEffectPrim.cpp").read_text(encoding="cp1252", errors="replace")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
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
    raise AssertionError(f"unclosed function: {marker}")


def compact(text: str) -> str:
    return re.sub(r"\s+", " ", text).strip()


def check(label: str, condition: bool) -> None:
    assert condition, f"FAIL {label}"
    print(f"PASS {label}")


signature = "void __thiscall CEffectPrim::Render3DQuadHorn(CEffectPrim *this, matrix *wtm)"
pe = body(PE, signature, PE.index("//----- (00646240)"))
ref = body(REF, signature)
local = body(LOCAL, "void RenderReferenceQuadHorn(const CEffectPrim& prim, const matrix& view)")
submit = body(LOCAL, "void SubmitReferenceEffectTriangle(")
pe_dispatch = body(PE, "void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm)", PE.index("//----- (00649220)"))
ref_dispatch = body(REF_DISPATCH, "void\tCEffectPrim :: Render(matrix &wtm/*view matrix*/)")
local_dispatch = body(LOCAL, "void CEffectPrim::Render(matrix* viewMatrix)")

check("canonical PE and primary Ref Render3DQuadHorn bodies normalize identically", compact(pe) == compact(ref))
check("readable Ref routes both horn primitive ids to the shared renderer",
      re.search(r"case\s+PP_3DQUADHORN\s*:", ref_dispatch) is not None
      and re.search(r"case\s+PP_3DQUADHORN2\s*:", ref_dispatch) is not None
      and re.search(r"case\s+PP_3DQUADHORN\s*:[\s\S]{0,100}case\s+PP_3DQUADHORN2\s*:[\s\S]{0,100}Render3DQuadHorn\(wtm\)", ref_dispatch))
check("PE dispatch routes canonical 0x09 and 0x75 to Render3DQuadHorn",
      re.search(r"case\s+9\s*:[\s\S]{0,80}case\s+0x75\s*:[\s\S]{0,80}Render3DQuadHorn\(this, wtm\)", pe_dispatch) is not None)
check("local renderer dispatch exposes the source horn helper",
      "RenderReferenceQuadHorn(*this,*viewMatrix)" in local_dispatch)
check("local geometry preserves all four PE triangle vertex orders and the unusual third-corner Y/Z term order",
      "{{-size,-size,0},{0,0,height},{size,-size,0}}" in local
      and "{{-size,-size,0},{0,0,height},{-size,size,0}}" in local
      and "{{-size,size,0},{0,0,height},{size,size,0}}" in local
      and "{{size,size,0},{0,0,height},{size,-size,0}}" in local
      and "i == 2 ? static_cast<float>(static_cast<double>(m.m[2][1])*p.z + static_cast<double>(m.m[0][1])*p.x + static_cast<double>(m.m[1][1])*p.y + m.m[3][1])" in compact(local)
      and "i == 2 ? static_cast<float>(static_cast<double>(m.m[2][2])*p.z + static_cast<double>(m.m[0][2])*p.x + static_cast<double>(m.m[1][2])*p.y + m.m[3][2])" in compact(local))
check("local triangles keep source texture slot zero, packed color, UV bands, and constant submission flag one",
      "prim.m_texture[0]" in local and "prim.m_argb" in local
      and "const float nextU = static_cast<float>(u + 0.2)" in local
      and "{{u,1},{u,0},{nextU,1}}" in local
      and "SubmitReferenceEffectTriangle(world,view,texture,prim.m_argb,uv,1)" in local
      and "g_renderer.AddReferenceEffectTriangle(face, renderFlag)" in submit)
check("focused fixture checks four face ordering, UV progression, retained transform, texture, color and hard-coded flag",
      "RenderReferenceQuadHorn(ice,view)" in TESTS
      and "FrostDiver quad horn submits original four triangular faces" in TESTS
      and "FrostDiver remaining faces preserve source winding and corner order" in TESTS
      and "FrostDiver horn advances original zero-point-two UV bands per face" in TESTS
      and "FrostDiver horn retains stored color exact texture and hard-coded flag one" in TESTS
      and "FrostDiver apex follows retained rotation" in TESTS
      and "FrostDiver unusual third-corner transform uses each matrix basis term and translation" in TESTS)
print("PP_3DQUADHORN renderer comparator passed.")
