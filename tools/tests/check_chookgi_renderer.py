from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
READ = (ROOT / "Ref/dhxj/RagEffect2.cpp").read_text(encoding="cp949", errors="replace")
DISPATCH = (ROOT / "Ref/dhxj/RagEffectPrim.cpp").read_text(encoding="cp949", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="cp949", errors="replace")
IMPL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")


def body(text, marker):
    start = text.index(marker)
    opening = text.index("{", start)
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{": depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[opening + 1:pos]
    raise AssertionError(f"unclosed body: {marker}")


def need(condition, label):
    if not condition:
        raise AssertionError(label)


read = body(READ, "void\tCEffectPrim :: RenderCHOOKGI(matrix &wtm)")
canonical = body(EXE, "void __thiscall CEffectPrim::RenderCHOOKGI(CEffectPrim *this, matrix *wtm)\n{")
current = body(IMPL, "void RenderChookGi(CEffectPrim& prim, const matrix& viewMatrix)")

need("for (ec=0 ; ec<1 ; ec++)" in read and "this->m_GI[0]" in canonical and "for (" not in canonical,
     "readable loop and canonical direct access both inspect only GI lane zero")
need("life == 1 && m_GI[ec].alphaB>0" in read and "life == 1 && this->m_GI[0].alphaB > 0" in canonical
     and "gi.life != 1 || gi.alphaB <= 0" in current, "exact life and positive-alpha guard")
need(read.index("Rx=m_GI[ec].distance") < read.index("RenderTeiRect(&rpCnt") < read.index("Rx=m_GI[ec].distance*0.5f")
     < read.rindex("RenderTeiRect(&rpCnt"), "readable outer-then-inner pass order")
need(canonical.index("this->m_GI[0].distance") < canonical.index("CEffectPrim::RenderTeiRect")
     < canonical.index("this->m_GI[0].distance * 0.5") < canonical.rindex("CEffectPrim::RenderTeiRect"),
     "canonical outer-then-inner pass order")
need("outerRadius = gi.distance" in current and "innerRadius = gi.distance * 0.5f" in current
     and current.index("outerRadius") < current.index("renderPass(outerRadius") < current.index("innerRadius") < current.index("renderPass(innerRadius"),
     "current preserves both radius formulas and pass order")
need("height[8]" in read and "height[8]" in canonical and "height[8]" in current
     and "distance*0.05f" in read and "distance * 0.050000001" in canonical
     and "gi.distance * 0.05f" in current, "shared outer-distance wobble is used for both radii")
need(all(f"Angle+{angle}" in read for angle in (90,180,270)) and "g_cosTable[Angle]" in read
     and "g_sinTable[0]" in canonical and "flt_72D5AC" in canonical
     and "corner * 90" in current, "four source quarter-turn corners")
need("Tei_TowardScreen" in read and "Tei_TowardScreen" in canonical
     and "corners[corner].z=z*g_cosTable[prim.m_gi[1].alphaT]" in current
     and "corners[corner].x=x*g_cosTable[gi.alphaT]" in current,
     "billboard rotation route")
need("vec1+=m_GI[ec].vecB_now" in read and "this->m_GI[0].vecB_now.x" in canonical
     and "gi.sparkX,gi.sparkY,gi.sparkZ" in current and "prim.m_pos" in current,
     "GI offset and primitive origin are both applied")
need("68,42,30" in read and "0x44u" in canonical and "modeThree ? 68 : 0" in current,
     "flag-one outer pass RGB (68,42,30); other mode RGB (0,0,255)")
need("255,255,255" in read and "0xFFu,\n        0xFFu,\n        0xFFu" in canonical
     and "modeThree ? 255 : 0" in current, "flag-one inner white; other mode yellow")
need("RenderTeiRect(&rpCnt, m_GI[ec].alphaB" in read and "CEffectPrim::RenderTeiRect" in canonical
     and "D3DBLEND_ONE" in current and "0.0f, 1.0f, 1.0f, 0.0f" in current,
     "alpha, additive blend, full texture UVs, and source-compatible quad submission")

dispatch = body(DISPATCH, "void CEffectPrim::Render()") if "void CEffectPrim::Render()" in DISPATCH else ""
need(re.search(r"case\s+PP_CHOOKGI:\s*RenderCHOOKGI\(wtm\);", DISPATCH),
     "readable primitive renderer dispatch selects RenderCHOOKGI")
need("case 0x20:" in EXE[EXE.index("void __thiscall CEffectPrim::Render("):]
     and "CEffectPrim::RenderCHOOKGI(this, wtm)" in EXE[EXE.index("void __thiscall CEffectPrim::Render("):],
     "canonical primitive renderer dispatch selects RenderCHOOKGI")
need(re.search(r"if\s*\(m_type\s*==\s*PP_CHOOKGI\)\s*\{\s*RenderChookGi\(\*this,\s*\*viewMatrix\);\s*return;", IMPL),
     "current primitive renderer dispatch route")
print("CHOOKGI renderer comparison passed: source/canonical/current guards, two radii/passes, billboard, colors, UV/blend and route")
