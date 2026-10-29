#!/usr/bin/env python3
"""Compare effect108 Freezing Init, inline caller, resource, and local route."""
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
pe_init_case = re.search(r"\bcase\s+108\s*:[\s\S]*?\bcase\s+109\s*:", pe_init)
ref_init_case = re.search(r"\bcase\s+108\s*:[\s\S]*?\bcase\s+109\s*:", ref_init)
assert pe_init_case and ref_init_case
pe_init_compact = re.sub(r"\s+", "", pe_init_case.group(0))
ref_init_compact = re.sub(r"\s+", "", ref_init_case.group(0))
check("PE and primary Ref Init map effect108 to Freezing.str",
      "sprintf(StrName,aFreezingStr)" in pe_init_compact
      and "sprintf(StrName,aFreezingStr)" in ref_init_compact)
check("PE and primary Ref effect108 Init branches agree",
      pe_init_compact == ref_init_compact)
check("local STR table preserves Freezing.str",
      '{ 108, "Freezing.str", nullptr, nullptr, 0, 0 }' in STR_TABLE)

pe_process = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
pe_case = re.search(r"\bcase\s+108\s*:[\s\S]*?\bcase\s+111\s*:", pe_process)
assert pe_case
pe_compact = re.sub(r"\s+", "", pe_case.group(0))
for token in (
    "if(!this->m_stateCnt)",
    "this->m_pos.z-v134->z",
    "this->m_pos.x-v134->x",
    "PlayWave(aEffectHunterFr,xco,0.0,zco,250,40,1.0)",
    "gotoLABEL_424",
):
    check(f"PE effect108 caller preserves {token}", token in pe_compact)
check("PE common effect108 continuation calls ProcessEZ2STR with (0,-80,0)",
      "LABEL_424:" in pe_process and "ProcessEZ2STR(this, 0.0, -80.0, 0.0)" in pe_process)

ref_helper = body(REF, "bool __thiscall CRagEffect::Freezing(CRagEffect *this)")
ref_compact = re.sub(r"\s+", "", ref_helper)
for token in (
    "if(!this->m_stateCnt)",
    "this->m_pos.z-p_m_pos->z",
    "this->m_pos.x-p_m_pos->x",
    "PlayWave(aEffectHunterFr,x,0.0,z,250,40,1.0)",
    "ProcessEZ2STR(this,0.0,-80.0,0.0)",
):
    check(f"primary Ref Freezing helper preserves {token}", token in ref_compact)
ref_dispatch = body(REF, "unsigned __int8 __thiscall CRagEffect::OnProcess2(CRagEffect *this)")
check("primary Ref OnProcess2 omits effect108 despite its helper",
      not re.search(r"\bcase\s+108\s*:", ref_dispatch))

image = ExeImage(ROOT / "HighPriest.exe")
for va, expected in ((0x71A014, b"effect\\hunter_freezingtrap.wav"),
                     (0x71AF4C, b"Freezing.str")):
    resolved = image.read_cstring_at(va)
    check(f"PE resource bytes at {va:#x} are exact", resolved is not None and resolved.raw == expected)

check("local effect108 dispatcher selects the clip-wave STR handler",
      "case 108: return ProcessClipWaveStrEffect();" in DISPATCH)
local_clip = body(LOCAL, "bool CRagEffect::ProcessClipWaveStrEffect()")
local_case = re.search(r"case 108\s*:[^\n]*", local_clip)
assert local_case
check("local effect108 preserves its state-zero wave and literal Y zero",
      'case 108: wave = "effect\\\\hunter_freezingtrap.wav"; break;' in local_case.group(0)
      and "randomWaveGate = true" not in local_case.group(0)
      and "TryPlaySingleEffectWave(m_effectPos, wave, yOffset, true);" in local_clip)
check("local helper continues every frame through source-offset ProcessEZ2STR",
      "ProcessEZ2STR(0.0f, -80.0f, 0.0f)" in local_clip)

print("effect108 Freezing Init/caller/resource/local route comparison passed")
