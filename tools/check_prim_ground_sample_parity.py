#!/usr/bin/env python3
"""Mechanically compare the PrimGroundSample source and updater port."""
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


ref = body(REF, "void __thiscall CEffectPrim::PrimGroundSample(CEffectPrim *this)\n{")
exe = body(EXE, "void __thiscall CEffectPrim::PrimGroundSample(CEffectPrim *this)\n{")
impl = body(IMPL, "void UpdateGroundSample(CEffectPrim* prim)")
exe_dispatch = body(EXE, "unsigned __int8 __thiscall CEffectPrim::OnProcess(CEffectPrim *this)\n{")
src_dispatch = body(IMPL, "bool CEffectPrim::ProcessPrimitiveFrame()\n{")


def check(label: str, condition: bool) -> None:
    assert condition, f"FAIL {label}"
    print(f"PASS {label}")


check("Ref and executable updater bodies are identical",
      re.sub(r"\s+", " ", ref) == re.sub(r"\s+", " ", exe))
check("canonical executable 0x70 update route",
      re.search(r"case 0x70:\s*CEffectPrim::PrimGroundSample\(this\);\s*goto \$L145961;", exe_dispatch) is not None)
check("implementation routes PP_GROUNDSAMPLE to the updater",
      re.search(r"m_type == PP_GROUNDSAMPLE\)\s*\{\s*UpdateGroundSample\(this\);\s*return true;", src_dispatch) is not None)
check("four GI slots iterate in source order",
      re.search(r"v2 = 4;[\s\S]*?p_RotStart \+= 90;\s*--v2;[\s\S]*?while \( v2 \);", ref) is not None
      and re.search(r"for \(EffectGi& gi : prim->m_gi\)", impl) is not None)
check("live-only guard; no alpha gate",
      re.search(r"if \( \*\(\(_BYTE \*\)p_RotStart - 16\) == 1 \)", ref) is not None
      and "if (gi.life != 1) continue;" in impl
      and "alphaB" not in impl)
check("source signed-short increment and pre-increment wrap predicate",
      re.search(r"v3 = \*p_RotStart \+ 1;\s*v4 = \(__int16\)\(\*p_RotStart - 359\) < 0;\s*\*p_RotStart = v3;", ref) is not None
      and "GroundSampleShort(static_cast<int>(previous) + 1)" in impl
      and "GroundSampleShort(static_cast<int>(previous) - 359)" in impl)
check("source overflow comparison controls conditional 360 subtraction",
      re.search(r"if \( v4 == __OFSUB__\(v3, 360\) \)\s*\*p_RotStart = v3 - 360;", ref) is not None
      and "const bool overflow = next < 0 && subtract360 >= 0;" in impl
      and "if ((before359 < 0) == overflow)" in impl
      and "GroundSampleShort(static_cast<int>(next) - 360)" in impl)
check("wrapped rotation is stored before maxHeight is recomputed from sine table",
      "*p_RotStart = v3 - 360;" in ref
      and "g_sinTable[*p_RotStart] * 0.5 + 1.5;" in ref
      and ref.index("*p_RotStart = v3 - 360;") < ref.index("g_sinTable[*p_RotStart] * 0.5 + 1.5;")
      and "gi.rotStart = next;" in impl
      and "g_sinTable[gi.rotStart]) * 0.5 + 1.5" in impl
      and impl.index("gi.rotStart = next;") < impl.index("gi.maxHeight ="))
check("16-bit wrap conversion is explicit and source-equivalent",
      re.search(r"short GroundSampleShort\(int value\)[\s\S]*?value & 0xFFFF[\s\S]*?bits < 0x8000 \? bits : bits - 0x10000", IMPL) is not None)

print("8 mechanical PrimGroundSample comparisons passed")
