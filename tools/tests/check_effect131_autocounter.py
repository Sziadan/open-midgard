#!/usr/bin/env python3
"""Compare effect131 AutoCounter Init, inline caller, resources, and local route."""
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


init_sig = "void __thiscall CRagEffect::Init(CRagEffect *this, CRenderObject *master, int effectId, vector3d fDeltaPos)"
pe_init = body(PE, init_sig)
ref_init = body(REF, init_sig)
for source_name, source in (("PE", pe_init), ("primary Ref", ref_init)):
    match = re.search(r"\bcase\s+131\s*:[\s\S]*?\bbreak\s*;", source)
    assert match
    check(f"{source_name} Init maps effect131 to AutoCounter.str",
          "sprintf(StrName,aAutocounterStr)" in re.sub(r"\s+", "", match.group(0)))
check("local STR table preserves AutoCounter.str",
      '{ 131, "AutoCounter.str", nullptr, nullptr, 0, 0 }' in STR_TABLE)

pe_process = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
pe_case = re.search(r"\bcase\s+131\s*:[\s\S]*?\bgoto\s+LABEL_424\s*;", pe_process)
assert pe_case
pe_compact = re.sub(r"\s+", "", pe_case.group(0))
for token in (
    "if(!this->m_stateCnt)",
    "this->m_pos.z-v137->z",
    "this->m_pos.x-v137->x",
    "PlayWave(aEffectKnightAu,xcr,0.0,zcr,250,40,1.0)",
):
    check(f"PE effect131 caller preserves {token}", token in pe_compact)
check("PE shared effect131 tail calls ProcessEZ2STR(0,-80,0)",
      "LABEL_424:" in pe_process and "ProcessEZ2STR(this, 0.0, -80.0, 0.0)" in pe_process)

ref_helper = body(REF, "bool __thiscall CRagEffect::AutoCounter(CRagEffect *this)")
ref_compact = re.sub(r"\s+", "", ref_helper)
for token in (
    "if(!this->m_stateCnt)",
    "this->m_pos.z-p_m_pos->z",
    "this->m_pos.x-p_m_pos->x",
    "PlayWave(aEffectKnightAu,x,0.0,z,250,40,1.0)",
    "ProcessEZ2STR(this,0.0,-80.0,0.0)",
):
    check(f"primary Ref AutoCounter helper preserves {token}", token in ref_compact)

image = ExeImage(ROOT / "HighPriest.exe")
for va, expected in ((0x719FB8, b"effect\\knight_autocounter.wav"),
                     (0x71AEB8, b"AutoCounter.str")):
    resolved = image.read_cstring_at(va)
    check(f"PE resource bytes at {va:#x} are exact", resolved is not None and resolved.raw == expected)

check("local effect131 dispatcher selects clip-wave STR handler",
      "case 131: return ProcessClipWaveStrEffect();" in DISPATCH)
local_clip = body(LOCAL, "bool CRagEffect::ProcessClipWaveStrEffect()")
local_case = re.search(r"case 131\s*:[^\n]*", local_clip)
assert local_case
check("local effect131 uses the verified frame-zero cue",
      'case 131: wave = "effect\\\\knight_autocounter.wav"; break;' in local_case.group(0)
      and "TryPlaySingleEffectWave(m_effectPos, wave, yOffset, true);" in local_clip)
check("local helper propagates shared ProcessEZ2STR completion",
      "return propagateStrResult ? active : true;" in local_clip
      and "ProcessEZ2STR(0.0f, -80.0f, 0.0f)" in local_clip)

print("effect131 AutoCounter Init/caller/resource/local route comparison passed")
