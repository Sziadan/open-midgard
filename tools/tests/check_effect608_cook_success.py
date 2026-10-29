#!/usr/bin/env python3
"""Compare effect608's cook-success STR mapping and state45 handler."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
DISPATCH = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")
CATALOG = (ROOT / "src/world/RagEffectCatalog.generated.inc").read_text(encoding="utf-8")
STR_TABLE = (ROOT / "src/world/RagEffectStrNameTable.generated.inc").read_text(encoding="utf-8")


def body(source: str, marker: str) -> str:
    start = source.rindex(marker)
    opening = source.index("{", start)
    depth = 0
    for cursor in range(opening, len(source)):
        if source[cursor] == "{":
            depth += 1
        elif source[cursor] == "}":
            depth -= 1
            if depth == 0:
                return source[start:cursor + 1]
    raise AssertionError(f"unterminated function: {marker}")


def check(label: str, ok: bool) -> None:
    assert ok, f"FAIL {label}"
    print(f"PASS {label}")


init_marker = "void __thiscall CRagEffect::Init(CRagEffect *this, CRenderObject *master, int effectId, vector3d fDeltaPos)"
for name, source in (("PE", PE), ("primary Ref", REF)):
    init = body(source, init_marker)
    case = re.search(r"\bcase\s+608\s*:[\s\S]*?\bbreak\s*;", init)
    check(f"{name} Init maps effect608 to cook_suc.str", case is not None and "sprintf(StrName, aCookSucStr);" in case.group(0))

for name, source in (("local catalog", CATALOG), ("local STR table", STR_TABLE)):
    check(f"{name} maps effect608 to cook_suc.str",
          '{ 608, "cook_suc.str", nullptr, nullptr, 0, 0 }' in source)

case_marker = "case 0x260:"
for name, source in (("PE", PE), ("primary Ref", REF)):
    start = source.index(case_marker)
    end = source.index("case 0x261:", start)
    case = re.sub(r"\s+", "", source[start:end])
    copy = "this->m_pos=this->m_master->m_pos;"
    check(f"{name} case608 copies master position before state45 cue",
          copy in case and case.index(copy) < case.index("PlayWave(aHealEffectWav,xbk,0.0,zbk,250,40,1.0);"))
    check(f"{name} case608 runs and propagates ProcessEZ2STR every call",
          "result=CRagEffect::ProcessEZ2STR(this,0.0,-80.0,0.0);if(!result)returnresult;return1;" in case)

on_process = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
case_start = on_process.index(case_marker)
case_end = on_process.index("case 0x261:", case_start)
pe_case = re.sub(r"\s+", "", on_process[case_start:case_end])
check("PE case608 gates cue to state45 and computes listener-relative X/Z from m_pos",
      "if(this->m_stateCnt!=45)" in pe_case
      and "this->m_pos.z-v118->z" in pe_case and "this->m_pos.x-v118->x" in pe_case)

helper = body(LOCAL, "bool CRagEffect::ProcessTier3StrEffect()")
compact = re.sub(r"\s+", "", helper)
check("local effect608 maps source m_pos into the CRagEffect m_effectPos field at event frame",
      "m_effectPos=master->m_pos;" in compact and "vector3dwavePos=m_effectPos;" in compact)
check("local effect608 retains frame45 cue and shared STR result", "case608:waveFrame=45;wave=\"_heal_effect.wav\";break;" in compact
      and "ProcessEZ2STR(0.0f,-80.0f,0.0f);returnpropagateStrResult?active:true;" in compact)
check("local OnProcess3 dispatches effect608 to its audited helper",
      "case 608: return ProcessTier3StrEffect();" in DISPATCH)

print("effect608 cook-success Init/OnProcess/local comparison passed")
