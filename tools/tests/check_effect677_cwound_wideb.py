#!/usr/bin/env python3
"""Compare effect677's state1 wideb cue and cwound STR route."""
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


for name, source in (("PE", PE), ("primary Ref", REF)):
    init = body(source, "void __thiscall CRagEffect::Init(CRagEffect *this, CRenderObject *master, int effectId, vector3d fDeltaPos)")
    case = re.search(r"\bcase\s+677\s*:[\s\S]*?\bbreak\s*;", init)
    check(f"{name} Init maps effect677 to cwound.str",
          case is not None and "sprintf(StrName, aCwoundStr);" in case.group(0))
for name, source in (("local catalog", CATALOG), ("local STR table", STR_TABLE)):
    check(f"{name} maps effect677 to cwound.str",
          '{ 677, "cwound.str", nullptr, nullptr, 0, 0 }' in source)
image = ExeImage(ROOT / "HighPriest.exe")
for va, expected in ((0x719CA8, b"effect\\wideb.wav"), (0x71A7E0, b"cwound.str")):
    resolved = image.read_cstring_at(va)
    check(f"PE bytes at VA 0x{va:x} equal {expected.decode()}",
          resolved is not None and resolved.raw == expected)

for name, source in (("PE", PE), ("primary Ref", REF)):
    on_process = body(source, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
    start = on_process.index("case 0x2A5:")
    end = on_process.index("case 0x2A6:", start)
    case = re.sub(r"\s+", "", on_process[start:end])
    copy = "this->m_pos=this->m_master->m_pos;"
    wave = "PlayWave(aEffectWidebWav,x,0.0,z,250,40,1.0);"
    check(f"{name} effect677 gates master copy and wideb cue on state1 before shared STR tail",
          "this->m_stateCnt==1" in case and copy in case and wave in case
          and case.index(copy) < case.index(wave))
    compact = re.sub(r"\s+", "", on_process)
    check(f"{name} shared STR tail ignores completion and returns true",
          "$L146219:CRagEffect::ProcessEZ2STR(this,0.0,-80.0,0.0);return1;" in compact)

helper = body(LOCAL, "bool CRagEffect::ProcessTier3StrEffect()")
compact = re.sub(r"\s+", "", helper)
check("local effect677 schedules state1 wideb and ignores STR result",
      'case677:waveFrame=1;wave="effect\\\\wideb.wav";propagateStrResult=false;break;' in compact
      and "returnpropagateStrResult?active:true;" in compact)
check("local helper updates effect position and plays listener-relative cue",
      "m_effectPos=master->m_pos;" in compact and "vector3dwavePos=m_effectPos;" in compact
      and "TryPlayEffectWaveAt(wavePos,wave);" in compact)
check("local effect677 dispatches to the tier-3 helper",
      "case 677: return ProcessTier3StrEffect();" in DISPATCH)
print("effect677 cwound/wideb STR comparison passed")
