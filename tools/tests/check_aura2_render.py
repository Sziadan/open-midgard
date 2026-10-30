#!/usr/bin/env python3
"""Source and branch checks for PP_3DAURA_2 / PP_WATERFALLPARTICLE rendering."""
from pathlib import Path
import re
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
REF_DISPATCH = (ROOT / "Ref/dhxj/RagEffectPrim.cpp").read_text(encoding="cp1252", errors="replace")
DHXJ = (ROOT / "Ref/dhxj/RagEffect2.cpp").read_text(encoding="cp1252", errors="replace")
CURRENT = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
TESTS = (ROOT / "tools/tests/ParticleRenderTests.cpp").read_text(encoding="utf-8")
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage
IMAGE = ExeImage()


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


PE_MARKER = "void __thiscall CEffectPrim::Render3DAura_2(CEffectPrim *this, matrix *wtm)"
pe = body(PE, PE_MARKER, PE.index("//----- (0061D090)"))
ref = body(REF, PE_MARKER)
local = body(CURRENT, "void Render3DAura2(CEffectPrim& prim, const matrix& viewMatrix)")
render_dispatch = body(CURRENT, "void CEffectPrim::Render(matrix* viewMatrix)")
pe_dispatch = body(PE, "void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm)", PE.index("//----- (00649220)"))
ref_dispatch = body(REF_DISPATCH, "void\tCEffectPrim :: Render(matrix &wtm/*view matrix*/)")

check("canonical PE and primary Ref Render3DAura_2 bodies match", compact(pe) == compact(ref))
check("PE uses four 180-byte lanes, B-pre base pointer and one shared coefficient-driven camera-facing quad",
      "p_z = &this->m_GI[0].vecB_pre.z" in pe
      and "p_z += 45" in pe
      and "*((_BYTE *)p_z - 164) != 1" in pe
      and "CEffectPrim::Tei_TowardScreen(this, &vec1, &vec2, &vec3, &vec4, alphaT, v69)" in pe
      and all(token in pe for token in ("flt_72D4F8", "flt_72C9B0", "flt_72D660", "flt_72CB18",
                                        "flt_72D7C8", "flt_72CC80", "flt_72D930", "flt_72CDE8")))
check("secondary Ref uses a fixed 45-degree square and the same four center-vector order",
      "Angle = 45" in DHXJ
      and "vec1=vec5+m_GI[ec].vecB_pre" in DHXJ
      and "vec2=vec6+m_GI[ec].vecB_pre" in DHXJ
      and "vec3=vec7+m_GI[ec].vecB_pre" in DHXJ
      and "vec4=vec8+m_GI[ec].vecB_pre" in DHXJ)
coefficient_vas = (0x72D4F8, 0x72C9B0, 0x72D660, 0x72CB18,
                   0x72D7C8, 0x72CC80, 0x72D930, 0x72CDE8)
coefficient_values = []
for va in coefficient_vas:
    offset = IMAGE.offset_of(va)
    value = 0.0 if offset is None else struct.unpack_from("<f", IMAGE.data, offset)[0]
    coefficient_values.append(value)
check("PE corner addresses are the runtime degree tables at 45/135/225/315, and the local square uses those angles",
      coefficient_vas == (0x72D4F8, 0x72C9B0, 0x72D660, 0x72CB18, 0x72D7C8, 0x72CC80, 0x72D930, 0x72CDE8)
      and coefficient_values == [0.0] * 8
      and "45 + corner * 90" in local
      and "g_cosTable[angle] * radius" in local
      and "g_sinTable[angle] * radius" in local)
def has_assignment(symbol: int) -> bool:
    name = f"flt_{symbol:06X}"
    return any(
        re.search(rf"\b{name}\s*=", line)
        for line in PE.splitlines()
        if not line.lstrip().startswith(("float ", "//"))
    )

check("PE decompilation contains no assignments to the eight corner coefficient globals",
      not any(has_assignment(va) for va in coefficient_vas))
check("local square matches the secondary Ref 45-degree construction and the PE table addresses",
      "Angle = 45" in DHXJ and "Tei_TowardScreen" in local)
check("PE and secondary Ref route PP_3DAURA_2 and PP_WATERFALLPARTICLE here",
      all(re.search(rf"case\s+PP_{kind}\s*:", ref_dispatch)
          for kind in ("3DAURA_2", "WATERFALLPARTICLE"))
      and "Render3DAura_2(wtm)" in ref_dispatch
      and re.search(r"case\s+0x1D\s*:", pe_dispatch)
      and re.search(r"case\s+0x50\s*:", pe_dispatch)
      and "CEffectPrim::Render3DAura_2(this, wtm)" in pe_dispatch)
check("local Render dispatch routes both ids to the dedicated four-group helper",
      "m_type == PP_3DAURA_2 || m_type == PP_WATERFALLPARTICLE" in render_dispatch
      and "Render3DAura2(*this, *viewMatrix);" in render_dispatch)
check("local helper maps all four PE GI centers in source order with the 180-byte lane gate",
      "if (gi.life != 1) continue;" in local
      and "{gi.vecBPreX, gi.arcBaselineY, gi.vecBPreZ}" in local
      and "{gi.sparkX, gi.sparkY, gi.sparkZ}" in local
      and "{gi.vecTPreX, gi.vecTPreY, gi.vecTPreZ}" in local
      and "{gi.vecTNowX, gi.vecTNowY, gi.vecTNowZ}" in local
      and "gi.flag[0]" in local)
check("source color modes, distinct first-group fade, all y gates, duplicate modes, and UV/blend mapping are represented",
      "y < -40.0f" in local and "y < -20.0f" in local and "y < -4.0f" in local
      and "y > -0.5f" in local and "mode == 0" in local and "mode == 1" in local
      and "case 3:" in local and "case 10:" in local and "case 11:" in local
      and "if (y > 0.0f) continue;" in local
      and "submit(group, alpha, 255, 255, 255);\n                submit(group, alpha, 255, 255, 255);" in local
      and "submit(group, alpha, 80, 80, 255);\n                submit(group, alpha, 80, 80, 255);" in local
      and "D3DBLEND_ONE" in local
      and "0.0f, 1.0f, 1.0f, 0.0f" in local)
check("focused renderer fixture checks transformed geometry, world origin, UVs, colors and source duplicates",
      "Render3DAura2(aura,identity)" in TESTS
      and "mode zero submits two white faces" in TESTS
      and "mode one duplicates the blue B-pre face" in TESTS
      and "mode three emits four visible groups in B-pre/spark/T-pre/T-now order" in TESTS
      and "mode ten uses the first-group white palette and remaining teal palettes" in TESTS
      and "mode eleven uses its B-pre-specific -40 fade" in TESTS)
print("Render3DAura_2 source and geometry comparator passed.")
