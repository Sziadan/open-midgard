#!/usr/bin/env python3
"""Source and geometry regression checks for PP_3DAURA Render3DAura."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
REF_DISPATCH = (ROOT / "Ref/dhxj/RagEffectPrim.cpp").read_text(encoding="cp1252", errors="replace")
CURRENT = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
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
    raise AssertionError(f"unclosed body: {marker}")


def compact(text: str) -> str:
    return re.sub(r"\s+", " ", text).strip()


def check(label: str, condition: bool) -> None:
    assert condition, f"FAIL {label}"
    print(f"PASS {label}")


PE_SIGNATURE = "void __thiscall CEffectPrim::Render3DAura(CEffectPrim *this, matrix *wtm)"
REF_SIGNATURE = PE_SIGNATURE
pe = body(PE, PE_SIGNATURE, PE.index("//----- (0061C580)"))
ref = body(REF, REF_SIGNATURE)
local = body(CURRENT, "void Render3DAura(CEffectPrim& prim, const matrix& viewMatrix)")
local_submit = body(CURRENT, "void SubmitWorldTeiRect(const vector3d& vec1,")
tei_helper = body(PE, "void __thiscall CEffectPrim::RenderTeiRect(\n        CEffectPrim *this,\n        int *rpCnt,\n        __int16 alpha,", PE.index("//----- (00611650)"))
check("canonical PE and primary Ref Render3DAura bodies match", compact(pe) == compact(ref))
check("source lane gate, size selector, radius/rise equation and four 90-degree samples are preserved",
      "*((_BYTE *)p_RotStart - 16) != 1" in pe
      and "this->m_size >= 110.0" in pe
      and "g_sinTable[p_RotStart[55]] + 1.0" in pe
      and "*((float *)p_RotStart + 28) * 0.8 + Rx" in pe
      and "p_RotStart += 90" in pe)

ref_dispatch = body(REF_DISPATCH, "void\tCEffectPrim :: Render(matrix &wtm/*view matrix*/)")
pe_dispatch = body(PE, "void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm)", PE.index("//----- (00649220)"))
check("PE and secondary Ref route the five source kinds through Render3DAura",
      all(re.search(rf"case\s+PP_{name}\s*:", ref_dispatch)
          for name in ("3DAURA", "MAPMAGICZONE", "GROUNDSHAKE", "LANDPROTECTOR", "FOOT"))
      and "Render3DAura(wtm)" in ref_dispatch
      and all(re.search(rf"case\s+{value}\s*:", pe_dispatch)
          for value in ("0x1A", "0x1C", "0x27", "0x43", "0x58"))
      and "CEffectPrim::Render3DAura(this, wtm)" in pe_dispatch)

render_dispatch = body(CURRENT, "void CEffectPrim::Render(matrix* viewMatrix)")
check("local renderer routes all five source kinds to the dedicated aura helper",
      all(f"m_type == PP_{name}" in render_dispatch
          for name in ("3DAURA", "MAPMAGICZONE", "GROUNDSHAKE", "LANDPROTECTOR", "FOOT"))
      and "Render3DAura(*this, *viewMatrix);" in render_dispatch)
check("local geometry preserves four radius samples, scratch fields, selector colors and crossed-face order",
      "g_sinTable[TableAngle(gi.riseAngle)]" in local
      and "g_cosTable[a180]" in local and "g_sinTable[a90]" in local
      and "mutableGi.vecTNowX = samples[1].x" in local
      and "mutableGi.vecBPreX = samples[2].x" in local
      and "mutableGi.vecTPreX = samples[3].x" in local
      and "case 0x42CC0000u" in local and "case 0x42DE0000u" in local
      and "case 0x42E00000u" in local
      and "const vector3d corners[4] = { samples[2], samples[0], samples[1], samples[3] }" in local
      and "place(corners[0]), place(corners[1]), place(corners[2]), place(corners[3])" in local
      and "const D3DBLEND blend = prim.m_size == 110.0f ? D3DBLEND_INVSRCALPHA : D3DBLEND_ONE" in local)
check("RenderTeiRect source maps PW=0 to ONE, TN=0/F1=0 to texture slot zero and world position",
      "v20 = PW == 0" in tei_helper and "destAlphaMode = 2" in tei_helper
      and "destAlphaMode = 6" in tei_helper and "m_texture[v29]" in tei_helper
      and "if ( !(_BYTE)F1 )" in tei_helper
      and "vec1.x = vec1.x + this->m_pos.x" in tei_helper
      and "m_argb_B = b + ((g + ((r + (alpha << 8)) << 8)) << 8)" in tei_helper
      and "verts[0].tu = v21" in tei_helper and "verts[0].tv = Ty2" in tei_helper
      and "verts[2].tv = Ty1" in tei_helper)
check("local submission matches source full UVs, additive blend, RGB selector and raw render flags",
      "viewMatrix, texture, color, 0.0f, 1.0f, 1.0f, 0.0f" in local
      and "D3DBLEND_ONE" in local and "D3DBLEND_INVSRCALPHA" in local
      and "static_cast<int>(prim.m_renderFlag)" in local
      and "face->destAlphaMode = destBlend;" in local_submit)
check("focused ParticleRenderTests cover one-lane geometry/scratch/tint, size-100 duplicate faces, size-101 red, and size-110 blend",
      "Render3DAura(aura,identity)" in TESTS
      and "Render3DAura(aura,identity)" in TESTS
      and "size 100 emits both crossed white faces" in TESTS
      and "size 101 emits two red faces" in TESTS
      and "size 110 uses inverse-alpha blending" in TESTS)
print("Render3DAura source/geometry comparator passed; Render3DAura_2 remains the next renderer target.")
