#!/usr/bin/env python3
"""Compare effects124/127 Poison React Init, shared caller, and local route."""
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


def init_case(source: str, case_id: int) -> str:
    match = re.search(rf"\bcase\s+{case_id}\s*:[\s\S]*?\bbreak\s*;", source)
    if not match:
        raise AssertionError(f"missing Init case {case_id}")
    return re.sub(r"\s+", "", match.group(0))


init_sig = "void __thiscall CRagEffect::Init(CRagEffect *this, CRenderObject *master, int effectId, vector3d fDeltaPos)"
pe_init = body(PE, init_sig)
ref_init = body(REF, init_sig)
for effect_id, symbol, local_str in ((124, "aVenomdustStr", "VenomDust.str"),
                                     (127, "aPoisonreactStr", "PoisonReact.str")):
    pe_case = init_case(pe_init, effect_id)
    ref_case = init_case(ref_init, effect_id)
    check(f"PE and primary Ref Init map effect{effect_id} to {local_str}",
          f"sprintf(StrName,{symbol})" in pe_case and f"sprintf(StrName,{symbol})" in ref_case)
    check(f"PE and primary Ref effect{effect_id} Init branches agree", pe_case == ref_case)
    check(f"local STR table preserves effect{effect_id} {local_str}",
          f'{{ {effect_id}, "{local_str}", nullptr, nullptr, 0, 0 }}' in STR_TABLE)

pe_process = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
pe_cases = re.search(r"\bcase\s+124\s*:[\s\S]*?\bcase\s+128\s*:", pe_process)
assert pe_cases
pe_compact = re.sub(r"\s+", "", pe_cases.group(0))
for token in (
    "case124:case127:",
    "if(!this->m_stateCnt)",
    "this->m_pos.z-v138->z",
    "this->m_pos.x-v138->x",
    "PlayWave(aEffectAssasinP,xcs,0.0,zcs,250,40,1.0)",
    "gotoLABEL_424",
):
    check(f"PE effects124/127 shared caller preserves {token}", token in pe_compact)
check("PE common continuation processes STR at (0,-80,0)",
      "LABEL_424:" in pe_process and "ProcessEZ2STR(this, 0.0, -80.0, 0.0)" in pe_process)

ref_helper = body(REF, "bool __thiscall CRagEffect::PoisonReact2(CRagEffect *this)")
ref_compact = re.sub(r"\s+", "", ref_helper)
for token in (
    "if(!this->m_stateCnt)",
    "this->m_pos.z-p_m_pos->z",
    "this->m_pos.x-p_m_pos->x",
    "PlayWave(aEffectAssasinP,x,0.0,z,250,40,1.0)",
    "ProcessEZ2STR(this,0.0,-80.0,0.0)",
):
    check(f"primary Ref PoisonReact2 helper preserves {token}", token in ref_compact)
ref_dispatch = body(REF, "unsigned __int8 __thiscall CRagEffect::OnProcess2(CRagEffect *this)")
check("primary Ref OnProcess2 omits cases124 and127 despite the shared helper",
      not re.search(r"\bcase\s+(?:124|127)\s*:", ref_dispatch))

image = ExeImage(ROOT / "HighPriest.exe")
for va, expected in ((0x719F98, b"effect\\assasin_poisonreact.wav"),
                     (0x71AEEC, b"VenomDust.str"),
                     (0x71AEC8, b"PoisonReact.str")):
    resolved = image.read_cstring_at(va)
    check(f"PE resource bytes at {va:#x} are exact", resolved is not None and resolved.raw == expected)

for effect_id in (124, 127):
    check(f"local effect{effect_id} dispatcher selects clip-wave STR handler",
          f"case {effect_id}: return ProcessClipWaveStrEffect();" in DISPATCH)
local_clip = body(LOCAL, "bool CRagEffect::ProcessClipWaveStrEffect()")
for effect_id in (124, 127):
    local_case = re.search(rf"case {effect_id}\s*:[^\n]*", local_clip)
    assert local_case
    check(f"local effect{effect_id} has frame-zero poison-react wave at Y zero",
          f'case {effect_id}: wave = "effect\\\\assasin_poisonreact.wav"; break;' in local_case.group(0))
check("local helper plays the wave and continues per-frame STR processing",
      "TryPlaySingleEffectWave(m_effectPos, wave, yOffset, true);" in local_clip
      and "ProcessEZ2STR(0.0f, -80.0f, 0.0f)" in local_clip)

print("effects124/127 Poison React Init/caller/resource/local route comparison passed")
