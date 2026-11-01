#!/usr/bin/env python3
"""Mechanically compare the reference PrimCloud branches with its updater port."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
REF = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
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
                return text[opening + 1:pos]
    raise AssertionError(f"unclosed function body: {marker}")


ref = body(REF, "unsigned __int8 __thiscall CEffectPrim::PrimCloud(CEffectPrim *this)\n{")
exe = body(EXE, "unsigned __int8 __thiscall CEffectPrim::PrimCloud(CEffectPrim *this)\n{")
impl = body(IMPL, "bool UpdateWeatherCloudPrimitive(CEffectPrim* prim)")
dispatch = body(EXE, "unsigned __int8 __thiscall CEffectPrim::OnProcess(CEffectPrim *this)\n{")


def check(label: str, condition: bool) -> None:
    assert condition, f"FAIL {label}"
    print(f"PASS {label}")


check("Ref and executable updater bodies are identical",
      re.sub(r"\s+", " ", ref) == re.sub(r"\s+", " ", exe))
check("canonical executable PP_CLOUD update route",
      re.search(r"case 0x21:\s*CEffectPrim::PrimCloud\(this\);", dispatch) is not None)
check("view angles and parent-position refresh match source before band updates",
      "v1->m_GI[0].alphaT = 360 - (__int64)m_view->m_cur.longitude" in ref
      and "v3 = 450 - (__int64)m_view->m_cur.latitude" in ref
      and "first.alphaT = static_cast<short>(360 - static_cast<int>(mode->m_view->GetCameraLongitude()))" in impl
      and "second.alphaT = latitudeAngle" in impl
      and "v1->m_pos.x = *p_x;" in ref and "v1->m_pos.z = p_x[2];" in ref
      and "prim->m_pos = prim->m_master->EffectPosition();" in impl
      and impl.index("prim->m_pos = prim->m_master->EffectPosition();") < impl.index("for (EffectGi& gi : prim->m_gi)"))
check("four live GI bands and alpha-at-99 fast path",
      "v35 = 4" in ref and "p_alphaB += 90" in ref
      and "for (EffectGi& gi : prim->m_gi)" in impl
      and "if (gi.life != 1) continue;" in impl
      and "if (gi.height[7] < 99.0f)" in impl)
check("source alpha ramp variants 0, 2, 3, 5 and fallback are all represented",
      all(token in ref for token in ("if ( *(float *)(p_alphaB + 23) == 0.0 )", "case 0x40000000:", "case 0x40400000:", "case 0x40A00000:", "if ( v6 < 80 )\n            *p_alphaB += 3;"))
      and all(token in impl for token in ("modeType == 0", "modeType == 2 || modeType == 5", "modeType == 3", "gi.alphaB + 3")))
check("process threshold, alpha decay and respawn branches match source modes",
      "if ( *(_DWORD *)(p_alphaB + 5) <= p_alphaB[7] )" in ref
      and "if ( process" not in ref
      and "gi.process > gi.rotStart && --gi.alphaB <= 0" in impl
      and all(token in impl for token in ("modeType == 0", "modeType == 2 || modeType == 5", "modeType == 3", "rand() % 200 + 25")))
check("mode-3 terrain sample, offset translation and source RNG count are preserved",
      "C3dAttr::GetHeight" in ref and "v9 % 5" in ref
      and "GetHeight(prim->m_pos.x + gi.sparkX, prim->m_pos.z + gi.sparkZ) - 20.0f" in impl
      and "gi.sparkX += prim->m_pos.x;" in impl
      and "gi.sparkZ += prim->m_pos.z;" in impl)
check("both 0..359 angle oscillators and reroll formulas are present",
      "v36 = 2" in ref and "if ( v19 > 359.0 )" in ref and "if ( *v14 <= (double)v14[1] )" in ref
      and "for (int angleIndex : {0, 2})" in impl
      and "angle = (std::max)(0.0f, angle - 1.0f)" in impl
      and "angle = (std::min)(359.0f, angle + 1.0f)" in impl
      and "359 - rand() % angleLimit" in impl and "rand() % angleLimit" in impl)
check("all five source sway profiles, persistent fast-band phase and unconditional true return",
      all(token in ref for token in ("* 0.015", "* 0.2", "* 0.050000001", "* 0.035"))
      and all(token in impl for token in ("modeType == 3", "modeType == 4", "modeType == 5", "angle0Decreasing", "gi.height[8] += 1.0f"))
      and "return true;" in impl)
check("active height[7] >= 99 fast path still advances height[8] outside the mode guard",
      re.search(r"if \(gi\.height\[7\] < 99\.0f\) \{[\s\S]*?\n        \}\n        gi\.height\[8\] \+= 1\.0f;", impl) is not None
      and "if ( *(float *)(p_alphaB + 23) >= 99.0 )" in ref
      and ref.index("if ( *(float *)(p_alphaB + 23) >= 99.0 )") < ref.index("v37 = *(float *)(p_alphaB + 25) + 1.0"))

print("10 mechanical PrimCloud comparisons passed")
