#!/usr/bin/env python3
"""Compare effect95 Quagmire's PE inline caller, primary Ref helper, and local route."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8", errors="replace")
STR_TABLE = (ROOT / "src/world/RagEffectStrNameTable.generated.inc").read_text(encoding="utf-8", errors="replace")
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
pe_init_start = re.search(r"\bcase\s+95\s*:", pe_init)
ref_init_start = re.search(r"\bcase\s+95\s*:", ref_init)
assert pe_init_start and ref_init_start
pe_init_end = re.search(r"\bcase\s+96\s*:", pe_init[pe_init_start.end():])
ref_init_end = re.search(r"\bcase\s+96\s*:", ref_init[ref_init_start.end():])
assert pe_init_end and ref_init_end
pe_init_case = pe_init[pe_init_start.start():pe_init_start.end() + pe_init_end.start()]
ref_init_case = ref_init[ref_init_start.start():ref_init_start.end() + ref_init_end.start()]
check("PE and primary Ref Init map effect95 to Quagmire.str",
      "sprintf(StrName,aQuagmireStr)" in re.sub(r"\s+", "", pe_init_case)
      and "sprintf(StrName,aQuagmireStr)" in re.sub(r"\s+", "", ref_init_case))
check("PE and primary Ref effect95 Init branches agree after whitespace normalization",
      re.sub(r"\s+", "", pe_init_case) == re.sub(r"\s+", "", ref_init_case))
check("local STR table preserves Quagmire.str", '{ 95, "Quagmire.str", nullptr, nullptr, 0, 0 }' in STR_TABLE)


pe_process = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
start = re.search(r"\bcase\s+95\s*:", pe_process)
end = re.search(r"\bcase\s+96\s*:", pe_process[start.end():]) if start else None
assert start and end
pe_case = pe_process[start.start():start.end() + end.start()]
pe_compact = re.sub(r"\s+", "", pe_case)
for token in (
    "if(!this->m_stateCnt&&!(rand()%12))",
    "this->m_pos.z-v127->z",
    "this->m_pos.x-v127->x",
    "PlayWave(aEffectWizardQu,xch,-100.0,zch,250,40,1.0)",
    "gotoLABEL_424",
):
    check(f"PE effect95 caller preserves {token}", token in pe_compact)

ref_quagmire = body(REF, "bool __thiscall CRagEffect::Quagmire(CRagEffect *this)")
ref_compact = re.sub(r"\s+", "", ref_quagmire)
for token in (
    "if(!this->m_stateCnt&&!(rand()%12))",
    "this->m_pos.z-p_m_pos->z",
    "this->m_pos.x-p_m_pos->x",
    "PlayWave(aEffectWizardQu,x,-100.0,z,250,40,1.0)",
    "ProcessEZ2STR(this,0.0,-80.0,0.0)",
):
    check(f"primary Ref Quagmire helper preserves {token}", token in ref_compact)
check("PE and primary Ref both gate one wave attempt on state0 and rand modulo12",
      "if(!this->m_stateCnt&&!(rand()%12))" in pe_compact
      and "if(!this->m_stateCnt&&!(rand()%12))" in ref_compact)

ref_dispatch = body(REF, "unsigned __int8 __thiscall CRagEffect::OnProcess2(CRagEffect *this)")
check("primary Ref OnProcess2 dispatcher omits effect95 while its helper remains present",
      not re.search(r"\bcase\s+95\s*:", ref_dispatch))

image = ExeImage(ROOT / "HighPriest.exe")
for va, expected in ((0x71A0B4, b"effect\\wizard_quagmire.wav"),
                     (0x71B020, b"Quagmire.str")):
    resolved = image.read_cstring_at(va)
    check(f"PE resource bytes at {va:#x} are exact", resolved is not None and resolved.raw == expected)

check("local effect95 dispatcher selects clip-wave STR handler",
      "case 95: return ProcessClipWaveStrEffect();" in DISPATCH)
local_clip = body(LOCAL, "bool CRagEffect::ProcessClipWaveStrEffect()")
local_case_match = re.search(r"case\s+95\s*:[^\n]*", local_clip)
assert local_case_match
check("local effect95 preserves frame-zero gate, one-in-twelve RNG, wave and -100 literal Y",
      "case 95: wave = \"effect\\\\wizard_quagmire.wav\"; yOffset = -100.0f; randomWaveGate = true; break;" in local_case_match.group(0)
      and "m_stateCnt == waveFrame && (!randomWaveGate || rand() % 12 == 0)" in local_clip
      and "TryPlaySingleEffectWave(m_effectPos, wave, yOffset, true);" in local_clip)
check("local helper continues every frame through source-offset ProcessEZ2STR",
      "ProcessEZ2STR(0.0f, -80.0f, 0.0f)" in local_clip)

print("effect95 Quagmire PE/Ref/local caller and resource comparison passed")
