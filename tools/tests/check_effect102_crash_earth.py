#!/usr/bin/env python3
"""Compare effect102 CrashEarth Init, inline caller, resources, and local STR route."""
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


def case_block(source: str, case_id: int, next_id: int) -> str:
    start = re.search(rf"\bcase\s+{case_id}\s*:", source)
    if not start:
        raise AssertionError(f"missing case {case_id}")
    end = re.search(rf"\bcase\s+{next_id}\s*:", source[start.end():])
    if not end:
        raise AssertionError(f"missing following case {next_id}")
    return source[start.start():start.end() + end.start()]


def check(label: str, ok: bool) -> None:
    assert ok, f"FAIL {label}"
    print(f"PASS {label}")


init_sig = "void __thiscall CRagEffect::Init(CRagEffect *this, CRenderObject *master, int effectId, vector3d fDeltaPos)"
pe_init = body(PE, init_sig)
ref_init = body(REF, init_sig)
pe_init_case = case_block(pe_init, 102, 103)
ref_init_case = case_block(ref_init, 102, 103)
for label, branch in (("PE", pe_init_case), ("Ref", ref_init_case)):
    check(f"{label} effect102 selects CrashEarth.str", "sprintf(StrName,aCrashearthStr)" in re.sub(r"\s+", "", branch))
check("primary Ref and PE Init effect102 branches agree after whitespace normalization",
      re.sub(r"\s+", "", pe_init_case) == re.sub(r"\s+", "", ref_init_case))
check("local STR table preserves CrashEarth.str", '{ 102, "CrashEarth.str", nullptr, nullptr, 0, 0 }' in STR_TABLE)

image = ExeImage(ROOT / "HighPriest.exe")
for va, expected in ((0x719798, b"effect\\wizard_fire_pillar_b.wav"),
                     (0x71AFD8, b"CrashEarth.str")):
    resolved = image.read_cstring_at(va)
    check(f"PE resource bytes at {va:#x} are exact", resolved is not None and resolved.raw == expected)

pe_process = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
pe_case = case_block(pe_process, 102, 103)
compact = re.sub(r"\s+", "", pe_case)
for token in (
    "if(this->m_stateCnt==30)",
    "this->m_pos.z-v130->z",
    "this->m_pos.x-v130->x",
    "PlayWave(aEffectWizardFi,xck,0.0,zck,250,40,1.0)",
    "if(this->m_stateCnt==35)",
    "GameMode->CMode::SendMsg(GameMode,135,0,0,0)",
    "gotoLABEL_424",
):
    check(f"PE effect102 caller preserves {token}", token in compact)
check("PE common effect102 continuation calls ProcessEZ2STR with (0,-80,0)",
      "LABEL_424:" in pe_process and "ProcessEZ2STR(this, 0.0, -80.0, 0.0)" in pe_process)
check("primary Ref dispatcher has no effect102 OnProcess caller",
      not re.search(r"\bcase\s+102\s*:", REF.replace(ref_init_case, "")))

check("local effect102 dispatcher selects clip-wave STR handler",
      "case 102: return ProcessClipWaveStrEffect();" in DISPATCH)
clip_helper = body(LOCAL, "bool CRagEffect::ProcessClipWaveStrEffect()")
clip_case_start = re.search(r"\bcase\s+102\s*:", clip_helper)
assert clip_case_start
clip_case_end = re.search(r"\bdefault\s*:", clip_helper[clip_case_start.end():])
assert clip_case_end
clip_case = clip_helper[clip_case_start.start():clip_case_start.end() + clip_case_end.start()]
check("local effect102 has the exact wave and frame/message gates",
      'case 102: waveFrame = 30; wave = "effect\\\\wizard_fire_pillar_b.wav"; quakeFrame = 35; break;' in clip_case)
check("local helper emits the wave, source message, and shared STR continuation at ordered gates",
      "if (m_stateCnt == waveFrame" in clip_helper
      and "if (m_stateCnt == quakeFrame)" in clip_helper
      and "mode->SendMsg(135, 0, 0, 0);" in clip_helper
      and "ProcessEZ2STR(0.0f, -80.0f, 0.0f)" in clip_helper)

print("effect102 CrashEarth Init/caller/resource/local route comparison passed")
