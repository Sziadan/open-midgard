#!/usr/bin/env python3
"""Compare effects130/250 TwoHand Init, shared caller, assets, and local route."""
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
    start = re.search(rf"\bcase\s+{case_id}\s*:", source)
    if not start:
        raise AssertionError(f"missing Init case {case_id}")
    end = re.search(r"\bcase\s+\d+\s*:", source[start.end():])
    segment = source[start.start():start.end() + end.start()] if end else source[start.start():]
    return re.sub(r"\s+", "", segment)


init_sig = "void __thiscall CRagEffect::Init(CRagEffect *this, CRenderObject *master, int effectId, vector3d fDeltaPos)"
pe_init = body(PE, init_sig)
ref_init = body(REF, init_sig)
for effect_id in (130, 250):
    pe_case = init_case(pe_init, effect_id)
    ref_case = init_case(ref_init, effect_id)
    check(f"PE and primary Ref Init route effect{effect_id} to TwoHand.str",
          ("goto$L147779;" in pe_case or "sprintf(StrName,aTwohandStr)" in pe_case)
          and ("goto$L147779;" in ref_case or "sprintf(StrName,aTwohandStr)" in ref_case))
    check(f"local STR table preserves effect{effect_id} TwoHand.str",
          f'{{ {effect_id}, "TwoHand.str", nullptr, nullptr, 0, 0 }}' in STR_TABLE)

pe_process = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
pe_case_match = re.search(r"\bcase\s+130\s*:[\s\S]*?\bcase\s+131\s*:", pe_process)
assert pe_case_match
pe_case = re.sub(r"\s+", "", pe_case_match.group(0))
for token in (
    "case130:case250:",
    "if(!this->m_stateCnt)",
    "this->m_duration=9999",
    "this->m_pos.z-v55->z",
    "this->m_pos.x-v55->x",
    "PlayWave(aEffectKnightTw,xw,0.0,zw,250,40,1.0)",
    "gotoLABEL_99",
):
    check(f"PE effects130/250 shared caller preserves {token}", token in pe_case)
pe_process_compact = re.sub(r"\s+", "", pe_process)
check("PE LABEL_99 calls ProcessEZ2STR(0,-80,0) and discards the return value",
      "LABEL_99:CRagEffect::ProcessEZ2STR(this,0.0,-80.0,0.0);break;" in pe_process_compact)

ref_helper = body(REF, "bool __thiscall CRagEffect::TwoHandQuicken(CRagEffect *this)")
ref_compact = re.sub(r"\s+", "", ref_helper)
for token in (
    "if(!this->m_stateCnt)",
    "this->m_duration=9999",
    "this->m_pos.z-p_m_pos->z",
    "this->m_pos.x-p_m_pos->x",
    "PlayWave(aEffectKnightTw,x,0.0,z,250,40,1.0)",
    "ProcessEZ2STR(this,0.0,-80.0,0.0)",
):
    check(f"primary Ref TwoHandQuicken helper preserves {token}", token in ref_compact)
ref_dispatch = body(REF, "unsigned __int8 __thiscall CRagEffect::OnProcess2(CRagEffect *this)")
check("primary Ref OnProcess2 omits case130",
      not re.search(r"\bcase\s+130\s*:", ref_dispatch))

image = ExeImage(ROOT / "HighPriest.exe")
for va, expected in ((0x7192D8, b"effect\\knight_twohandquicken.wav"),
                     (0x71AD30, b"TwoHand.str")):
    resolved = image.read_cstring_at(va)
    check(f"PE resource bytes at {va:#x} are exact", resolved is not None and resolved.raw == expected)

for effect_id in (130, 250):
    check(f"local effect{effect_id} dispatcher selects clip-wave STR handler",
          f"case {effect_id}: return ProcessClipWaveStrEffect();" in DISPATCH)
local_clip = body(LOCAL, "bool CRagEffect::ProcessClipWaveStrEffect()")
clip_case = re.search(r"case 130: case 250:[^\n]*", local_clip)
assert clip_case
check("local effects130/250 preserve wave, duration extension, and non-propagated STR result",
      'case 130: case 250: wave = "effect\\\\knight_twohandquicken.wav"; propagateStrResult = false; extendDuration = true; break;' in clip_case.group(0)
      and "if (extendDuration) m_duration = 9999;" in local_clip
      and "return propagateStrResult ? active : true;" in local_clip)
check("local helper emits the wave and calls source-offset ProcessEZ2STR",
      "TryPlaySingleEffectWave(m_effectPos, wave, yOffset, true);" in local_clip
      and "ProcessEZ2STR(0.0f, -80.0f, 0.0f)" in local_clip)

print("effects130/250 TwoHand Init/caller/resource/local route comparison passed")
