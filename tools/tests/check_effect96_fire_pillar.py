#!/usr/bin/env python3
"""Compare effect96 Fire Pillar Init, inline caller, resource, and local route."""
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
pe_init_case = re.search(r"\bcase\s+96\s*:[\s\S]*?\bcase\s+97\s*:", pe_init)
ref_init_case = re.search(r"\bcase\s+96\s*:[\s\S]*?\bcase\s+97\s*:", ref_init)
assert pe_init_case and ref_init_case
pe_init_compact = re.sub(r"\s+", "", pe_init_case.group(0))
ref_init_compact = re.sub(r"\s+", "", ref_init_case.group(0))
check("PE and primary Ref Init map effect96 to FirePillar.str",
      "sprintf(StrName,aFirepillarStr)" in pe_init_compact
      and "sprintf(StrName,aFirepillarStr)" in ref_init_compact)
check("primary Ref and PE Init effect96 branches agree",
      pe_init_compact == ref_init_compact)
check("local STR table preserves FirePillar.str",
      '{ 96, "FirePillar.str", nullptr, nullptr, 0, 0 }' in STR_TABLE)

pe_process = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
pe_case = re.search(r"\bcase\s+96\s*:[\s\S]*?\bcase\s+97\s*:", pe_process)
assert pe_case
pe_compact = re.sub(r"\s+", "", pe_case.group(0))
for token in (
    "if(this->m_stateCnt==75)",
    "this->m_pos.z-v128->z",
    "this->m_pos.x-v128->x",
    "PlayWave(aEffectWizardFi_0,xci,0.0,zci,250,40,1.0)",
    "gotoLABEL_424",
):
    check(f"PE effect96 caller preserves {token}", token in pe_compact)
check("PE common effect96 continuation calls ProcessEZ2STR with (0,-80,0)",
      "LABEL_424:" in pe_process and "ProcessEZ2STR(this, 0.0, -80.0, 0.0)" in pe_process)

ref_dispatch = body(REF, "unsigned __int8 __thiscall CRagEffect::OnProcess2(CRagEffect *this)")
check("primary Ref OnProcess2 dispatcher omits effect96",
      not re.search(r"\bcase\s+96\s*:", ref_dispatch))

image = ExeImage(ROOT / "HighPriest.exe")
for va, expected in ((0x71A094, b"effect\\wizard_fire_pillar_a.wav"),
                     (0x71B010, b"FirePillar.str")):
    resolved = image.read_cstring_at(va)
    check(f"PE resource bytes at {va:#x} are exact", resolved is not None and resolved.raw == expected)

check("local effect96 dispatcher selects the clip-wave STR handler",
      "case 96: return ProcessClipWaveStrEffect();" in DISPATCH)
local_clip = body(LOCAL, "bool CRagEffect::ProcessClipWaveStrEffect()")
local_case = re.search(r"case 96\s*:[^\n]*", local_clip)
assert local_case
check("local effect96 preserves its state75 wave and literal Y zero",
      'case 96: waveFrame = 75; wave = "effect\\\\wizard_fire_pillar_a.wav"; break;' in local_case.group(0)
      and "randomWaveGate = true" not in local_case.group(0)
      and "TryPlaySingleEffectWave(m_effectPos, wave, yOffset, true);" in local_clip)
check("local helper continues every frame through source-offset ProcessEZ2STR",
      "ProcessEZ2STR(0.0f, -80.0f, 0.0f)" in local_clip)

print("effect96 Fire Pillar Init/caller/resource/local route comparison passed")
