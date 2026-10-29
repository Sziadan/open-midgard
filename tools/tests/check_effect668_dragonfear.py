#!/usr/bin/env python3
"""Compare effect668's timed dragonfear wave, message and STR behavior."""
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
    compact = re.sub(r"\s+", "", init)
    check(f"{name} Init maps effects668/670 to dfear.str",
          "case668:case670:sprintf(StrName,aDfearStr);break;" in compact)

for name, source in (("local catalog", CATALOG), ("local STR table", STR_TABLE)):
    check(f"{name} maps effect668 to dfear.str",
          '{ 668, "dfear.str", nullptr, nullptr, 0, 0 }' in source)

image = ExeImage(ROOT / "HighPriest.exe")
for va, expected in ((0x719538, b"effect\\dragonfear.wav"), (0x71A7F8, b"dfear.str")):
    resolved = image.read_cstring_at(va)
    check(f"PE bytes at VA 0x{va:x} equal {expected.decode()}",
          resolved is not None and resolved.raw == expected)

for name, source in (("PE", PE), ("primary Ref", REF)):
    on_process = body(source, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
    start = on_process.index("case 0x29C:")
    end = on_process.index("case 0x29D:", start)
    case = re.sub(r"\s+", "", on_process[start:end])
    copy = "this->m_pos=this->m_master->m_pos;"
    wave = "PlayWave(aEffectDragonfe,xbm,0.0,zbm,250,40,1.0);"
    message = "v121->CMode::SendMsg(v121,135,0,0,0);"
    check(f"{name} effect668 gates state1 position copy, exact wave and message in order",
          "this->m_stateCnt==1" in case and copy in case and wave in case and message in case
          and case.index(copy) < case.index(wave) < case.index(message)
          and "goto$L146219;" in case)
    compact = re.sub(r"\s+", "", on_process)
    check(f"{name} effect668 shared STR tail ignores completion and returns true",
          "$L146219:CRagEffect::ProcessEZ2STR(this,0.0,-80.0,0.0);return1;" in compact)

helper = body(LOCAL, "bool CRagEffect::ProcessTier3StrEffect()")
compact = re.sub(r"\s+", "", helper)
check("local effect668 maps state1 wave and non-propagated STR result",
      'case668:waveFrame=1;wave="effect\\\\dragonfear.wav";propagateStrResult=false;break;' in compact)
check("local effect668 updates effect position, plays cue before message135, and ignores completion",
      "m_effectPos=master->m_pos;" in compact
      and "TryPlayEffectWaveAt(wavePos,wave);" in compact
      and "mode->SendMsg(135,0,0,0);" in compact
      and compact.index("TryPlayEffectWaveAt(wavePos,wave);") < compact.index("mode->SendMsg(135,0,0,0);")
      and "returnpropagateStrResult?active:true;" in compact)
check("local OnProcess3 dispatches effect668 to the tier-3 helper",
      "case 668: return ProcessTier3StrEffect();" in DISPATCH)

print("effect668 dragonfear/STR comparison passed")
