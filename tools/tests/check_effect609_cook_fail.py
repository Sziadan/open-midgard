#!/usr/bin/env python3
"""Compare effect609's cook-failure STR mapping and state50 handler."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
DISPATCH = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")
CATALOG = (ROOT / "src/world/RagEffectCatalog.generated.inc").read_text(encoding="utf-8")
STR_TABLE = (ROOT / "src/world/RagEffectStrNameTable.generated.inc").read_text(encoding="utf-8")
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage


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
    case = re.search(r"\bcase\s+609\s*:[\s\S]*?\bbreak\s*;", init)
    check(f"{name} Init maps effect609 to cook_fail.str",
          case is not None and "sprintf(StrName, aCookFailStr);" in case.group(0))

for name, source in (("local catalog", CATALOG), ("local STR table", STR_TABLE)):
    check(f"{name} maps effect609 to cook_fail.str",
          '{ 609, "cook_fail.str", nullptr, nullptr, 0, 0 }' in source)

image = ExeImage(ROOT / "HighPriest.exe")
for va, expected in ((0x71998C, b"caramel_die.wav"), (0x71A814, b"cook_fail.str")):
    value = image.read_cstring_at(va)
    check(f"PE bytes at VA 0x{va:x} resolve to {expected.decode()}",
          value is not None and value.raw == expected)

case_marker = "case 0x261:"
for name, source in (("PE", PE), ("primary Ref", REF)):
    start = source.index(case_marker)
    end = source.index("case 0x262:", start)
    case = re.sub(r"\s+", "", source[start:end])
    copy = "this->m_pos=this->m_master->m_pos;"
    check(f"{name} case609 copies master position before frame50 cue",
          "this->m_stateCnt==50" in case and copy in case
          and case.index(copy) < case.index("PlayWave(aCaramelDieWav,xq,0.0,zq,250,40,1.0);"))
    check(f"{name} case609 continues to and propagates shared STR processing",
          "goto$L146677;" in case)

on_process = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
compact = re.sub(r"\s+", "", on_process)
check("PE shared effect608/609 tail propagates ProcessEZ2STR completion",
      "LABEL_146677:result=CRagEffect::ProcessEZ2STR(this,0.0,-80.0,0.0);if(!result)returnresult;return1;"
      in compact or "result=CRagEffect::ProcessEZ2STR(this,0.0,-80.0,0.0);if(!result)returnresult;return1;"
      in compact)

helper = body(LOCAL, "bool CRagEffect::ProcessTier3StrEffect()")
helper_compact = re.sub(r"\s+", "", helper)
check("local helper maps source m_pos to CRagEffect m_effectPos and schedules exact frame50 cue",
      'case609:waveFrame=50;wave="caramel_die.wav";break;' in helper_compact
      and "m_effectPos=master->m_pos;" in helper_compact
      and "vector3dwavePos=m_effectPos;" in helper_compact)
check("local OnProcess3 dispatches effect609 to its audited helper",
      "case 609: return ProcessTier3StrEffect();" in DISPATCH)
check("local helper propagates ProcessEZ2STR completion for effect609",
      "constboolactive=ProcessEZ2STR(0.0f,-80.0f,0.0f);returnpropagateStrResult?active:true;"
      in helper_compact)

print("effect609 cook-failure Init/OnProcess/resource/local comparison passed")
