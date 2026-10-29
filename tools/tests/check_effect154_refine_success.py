#!/usr/bin/env python3
"""Compare effect154 refine-success caller, resources, and local route."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
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
for source_name, source, symbol in (("PE", PE, "aBsRefinesucces"), ("primary Ref", REF, "aBsRefinesucces")):
    init = body(source, init_marker)
    case = re.search(r"\bcase\s+154\s*:[\s\S]*?\bbreak\s*;", init)
    assert case
    check(f"{source_name} Init maps effect154 to bs_RefineSuccess.str",
          f"sprintf(StrName,{symbol})" in re.sub(r"\s+", "", case.group(0)))
check("local STR table preserves effect154 bs_RefineSuccess.str",
      '{ 154, "bs_RefineSuccess.str", nullptr, nullptr, 0, 0 }' in STR_TABLE)

pe_process = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
pe_case = re.search(r"\bcase\s+154\s*:[\s\S]*?\bgoto\s+LABEL_424\s*;", pe_process)
assert pe_case
compact = re.sub(r"\s+", "", pe_case.group(0))
for token in (
    "if(this->m_stateCnt==15)",
    "PlayWave(",
    "250,40,1.0)",
):
    check(f"PE effect154 caller preserves {token}", token in compact)
check("PE effect154 cue uses effect-minus-player X/Z and literal Y zero",
      "this->m_pos.z-" in compact and "this->m_pos.x-" in compact
      and "PlayWave(" in compact and ",0.0," in compact)
check("PE shared tail propagates ProcessEZ2STR(0,-80,0) completion",
      "LABEL_424:result=CRagEffect::ProcessEZ2STR(this,0.0,-80.0,0.0);LABEL_425:if(!result)returnresult;break;"
      in re.sub(r"\s+", "", pe_process))

image = ExeImage(ROOT / "HighPriest.exe")
for va, expected in ((0x719F58, b"effect\\bs_refinesuccess.wav"),
                     (0x71AD78, b"bs_RefineSuccess.str")):
    resolved = image.read_cstring_at(va)
    check(f"PE resource bytes at {va:#x} are exact", resolved is not None and resolved.raw == expected)

check("local effect154 dispatches to clip-wave STR helper",
      "case 154: return ProcessClipWaveStrEffect();" in DISPATCH)
local_clip = body(LOCAL, "bool CRagEffect::ProcessClipWaveStrEffect()")
local_case = re.search(r"case 154\s*:[^\n]*", local_clip)
assert local_case
check("local effect154 configures frame-15 cue and propagates STR result",
      'case 154: waveFrame = 15; wave = "effect\\\\bs_refinesuccess.wav"; break;' in local_case.group(0)
      and "TryPlaySingleEffectWave(m_effectPos, wave, yOffset, true);" in local_clip
      and "return propagateStrResult ? active : true;" in local_clip)

print("effect154 refine-success Init/caller/resource/local route comparison passed")
