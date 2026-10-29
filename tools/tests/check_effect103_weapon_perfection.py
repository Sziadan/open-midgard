#!/usr/bin/env python3
"""Compare effect103 WeaponPerfection init, inline caller and local STR route."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
STR_TABLE = (ROOT / "src/world/RagEffectStrNameTable.generated.inc").read_text(encoding="utf-8")

import sys
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


pe_init = body(PE, "void __thiscall CRagEffect::Init(CRagEffect *this, CRenderObject *master, int effectId, vector3d fDeltaPos)")
ref_init = body(REF, "void __thiscall CRagEffect::Init(CRagEffect *this, CRenderObject *master, int effectId, vector3d fDeltaPos)")
pe_init_case = case_block(pe_init, 103, 104)
ref_init_case = case_block(ref_init, 103, 104)
for label, branch in (("PE", pe_init_case), ("Ref", ref_init_case)):
    compact = re.sub(r"\s+", "", branch)
    check(f"{label} effect103 selects minimum/full WeaponPerfection STR by session setting",
          "g_session.m_isMinEffect" in compact
          and "sprintf(StrName,aWeaponperfecti)" in compact
          and "sprintf(StrName,aWeaponperfecti_0)" in compact)
check("primary Ref and PE Init effect103 branches agree after whitespace normalization",
      re.sub(r"\s+", "", pe_init_case) == re.sub(r"\s+", "", ref_init_case))
check("local STR table preserves both source filenames for effect103",
      '{ 103, "WeaponPerfection.str", "WeaponPerfection_min.str", nullptr, 0, 0 }' in STR_TABLE)

image = ExeImage(ROOT / "HighPriest.exe")
for va, expected in (
    (0x71A070, b"effect\\black_weapon_perfection.wav"),
    (0x71AFA4, b"WeaponPerfection.str"),
    (0x71AFBC, b"WeaponPerfection_min.str"),
):
    resolved = image.read_cstring_at(va)
    check(f"PE resource bytes at {va:#x} are exact", resolved is not None and resolved.raw == expected)

pe_process = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
pe_case = case_block(pe_process, 103, 104)
compact_case = re.sub(r"\s+", "", pe_case)
for token in (
    "if(!this->m_stateCnt)",
    "&CModeMgr::GetGameMode(&g_modeMgr)->m_world->m_player->m_pos",
    "this->m_pos.z-v131->z",
    "this->m_pos.x-v131->x",
    "PlayWave(aEffectBlackWea,xcl,0.0,zcl,250,40,1.0)",
    "gotoLABEL_424",
):
    check(f"PE effect103 caller preserves {token}", token in compact_case)
check("PE common effect103 continuation calls ProcessEZ2STR with (0,-80,0)",
      "LABEL_424:" in pe_process and "ProcessEZ2STR(this, 0.0, -80.0, 0.0)" in pe_process)

check("primary Ref dispatcher has no effect103 caller to corroborate",
      not re.search(r"\bcase\s+103\s*:", REF.replace(ref_init_case, "")))
check("local effect103 dispatcher selects the clip-wave STR handler",
      "case 103: return ProcessClipWaveStrEffect();" in DISPATCH)
clip_helper = body(LOCAL, "bool CRagEffect::ProcessClipWaveStrEffect()")
clip_case = case_block(clip_helper, 103, 106)
check("local effect103 has a frame-zero wave and no per-effect overrides",
      "case 103: wave = \"effect\\\\black_weapon_perfection.wav\"; break;" in clip_case
      and "waveFrame =" not in clip_case and "yOffset =" not in clip_case
      and "copyMasterPosition =" not in clip_case and "propagateStrResult =" not in clip_case)
check("local clip-wave helper continues every frame through ProcessEZ2STR and returns its result",
      "TryPlaySingleEffectWave(m_effectPos, wave, yOffset, true);" in clip_helper
      and "const bool active = ProcessEZ2STR(0.0f, -80.0f, 0.0f);" in clip_helper
      and "return propagateStrResult ? active : true;" in clip_helper)

print("effect103 WeaponPerfection Init/caller/resource/local route comparison passed")
