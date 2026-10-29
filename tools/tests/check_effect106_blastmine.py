#!/usr/bin/env python3
"""Compare effect106 BlastMine init, inline caller and local STR route."""
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


init_marker = "void __thiscall CRagEffect::Init(CRagEffect *this, CRenderObject *master, int effectId, vector3d fDeltaPos)"
pe_init_case = case_block(body(PE, init_marker), 106, 107)
ref_init_case = case_block(body(REF, init_marker), 106, 107)
for label, branch in (("PE", pe_init_case), ("Ref", ref_init_case)):
    compact = re.sub(r"\s+", "", branch)
    check(f"{label} effect106 selects BlastMine.str", "sprintf(StrName,aBlastmineStr)" in compact)
check("primary Ref and PE Init effect106 branches agree after whitespace normalization",
      re.sub(r"\s+", "", pe_init_case) == re.sub(r"\s+", "", ref_init_case))
check("local STR table preserves the BlastMine filename",
      '{ 106, "BlastMine.str", nullptr, nullptr, 0, 0 }' in STR_TABLE)

image = ExeImage(ROOT / "HighPriest.exe")
for va, expected in (
    (0x71A054, b"effect\\hunter_blastmine.wav"),
    (0x71AF6C, b"BlastMine.str"),
):
    resolved = image.read_cstring_at(va)
    check(f"PE resource bytes at {va:#x} are exact", resolved is not None and resolved.raw == expected)

pe_process = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
pe_case = case_block(pe_process, 106, 107)
compact_case = re.sub(r"\s+", "", pe_case)
for token in (
    "if(this->m_stateCnt==20)",
    "&CModeMgr::GetGameMode(&g_modeMgr)->m_world->m_player->m_pos",
    "this->m_pos.z-v132->z",
    "this->m_pos.x-v132->x",
    "PlayWave(aEffectHunterBl,xcm,0.0,zcm,250,40,1.0)",
    "gotoLABEL_424",
):
    check(f"PE effect106 caller preserves {token}", token in compact_case)
check("PE common effect106 continuation calls ProcessEZ2STR with (0,-80,0)",
      "LABEL_424:" in pe_process and "ProcessEZ2STR(this, 0.0, -80.0, 0.0)" in pe_process)
check("primary Ref dispatcher has no effect106 caller to corroborate",
      not re.search(r"\bcase\s+106\s*:", REF.replace(ref_init_case, "")))

check("local effect106 dispatcher selects the clip-wave STR handler",
      "case 106: return ProcessClipWaveStrEffect();" in DISPATCH)
clip_helper = body(LOCAL, "bool CRagEffect::ProcessClipWaveStrEffect()")
clip_case = case_block(clip_helper, 106, 107)
check("local effect106 has the source state-20 wave and no per-effect overrides",
      'case 106: waveFrame = 20; wave = "effect\\\\hunter_blastmine.wav"; break;' in clip_case
      and "yOffset =" not in clip_case and "copyMasterPosition =" not in clip_case
      and "propagateStrResult =" not in clip_case)
check("local clip-wave helper continues every frame through ProcessEZ2STR and returns its result",
      "TryPlaySingleEffectWave(m_effectPos, wave, yOffset, true);" in clip_helper
      and "const bool active = ProcessEZ2STR(0.0f, -80.0f, 0.0f);" in clip_helper
      and "return propagateStrResult ? active : true;" in clip_helper)

print("effect106 BlastMine Init/caller/resource/local route comparison passed")
