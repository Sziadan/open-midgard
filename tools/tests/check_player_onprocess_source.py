#!/usr/bin/env python3
"""Audit CPlayer::OnProcess source branches and map the queued-wave route."""
from pathlib import Path
import hashlib
import re

ROOT = Path(__file__).resolve().parents[2]
REF = (ROOT / "Ref/Player.cpp").read_text(encoding="utf-8", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
CURRENT = (ROOT / "src/world/GameActor.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "src/world/GameActor.h").read_text(encoding="utf-8")


def body(text: str, marker: str) -> tuple[str, str]:
    start = text.rfind(marker)
    assert start >= 0, f"definition missing: {marker}"
    opening = text.find("{", start)
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[start:opening].strip(), text[opening + 1:pos]
    raise AssertionError(f"unclosed function: {marker}")


normalize = lambda value: re.sub(r"\s+", " ", value).strip()
marker = "unsigned __int8 __thiscall CPlayer::OnProcess(CPlayer *this)"
ref_sig, ref_body = body(REF, marker)
exe_sig, exe_body = body(EXE, marker)
assert normalize(ref_sig) == normalize(exe_sig) == normalize(marker)
normalized = normalize(ref_body)
assert normalized == normalize(exe_body), "complete Ref/EXE CPlayer::OnProcess bodies differ"
print(f"Ref/EXE body equality: {len(normalized)} normalized characters, SHA-256 {hashlib.sha256(normalized.encode()).hexdigest()}")

# The producer branch is the same queued hit-wave route already compared for
# CPc: after ProcessState and ProcessSound, use actor-minus-player X/Z, then
# clear the pending flag. Tint lifecycle is covered in actor_tint_source_check.
ordered_source = [
    "CGameActor::ProcessBodyState(this)",
    "CGameActor::ProcessEffectState(this)",
    "CGameActor::ProcessWillBeAttacked(this)",
    "this->m_isSingleColor && timeGetTime() > this->m_singleColorStartTick + 100",
    "if ( this->m_sprArgb != -1 )",
    "CRenderObject::ModifyArgb(this, 8, 8, 8, 8)",
    "if ( this->m_preMoveOn && timeGetTime() >",
    "CPlayer::ProcessProceed(this)",
    "this->CGameActor::ProcessState(this)",
    "CRenderObject::ProcessSound(this)",
    "if ( this->m_isPlayHitWave )",
    "PlayWave(this->m_hitWaveName, x, 0.0, z, 250, 40, 1.0)",
    "this->m_isPlayHitWave = 0",
]
positions = [ref_body.index(item) for item in ordered_source]
assert positions == sorted(positions), "source body/pre-move/proceed/audio order changed"
assert ref_body.count("PlayWave(") == 1

# The sound branch maps to modern shared actor processing. Remaining OnProcess
# movement branches are deliberately surfaced as known gaps so a passing audit
# cannot be read as complete CPlayer function parity.
assert "class CPlayer : public CPc" in HEADER
assert "u8 OnProcess() override;" not in HEADER[HEADER.index("class CPlayer :"):HEADER.index("struct SkillStandEntry")]
_, process_state = body(CURRENT, "u8 CGameActor::ProcessState()")
assert "ProcessQueuedHitWave(*this);" in process_state
assert "Ref runtime-wave: 0x5D9E95 0x5E0FD6 0x5E48A1" in process_state
_, queued = body(CURRENT, "void ProcessQueuedHitWave(CGameActor& actor)")
assert "if (actor.m_isPlayHitWave == 0)" in queued
play_pos = queued.index("TryPlayQueuedWaveAtActor(actor, actor.m_hitWaveName);")
clear_pos = queued.index("actor.m_isPlayHitWave = 0;", play_pos)
assert play_pos < clear_pos
assert "actor.m_pos.x - listener.x" in CURRENT
assert "actor.m_pos.z - listener.z" in CURRENT

assert "void CPlayer::ProcessPreMove()\n{\n}" in CURRENT
assert "CPlayer::ProcessProceed" not in CURRENT
print("AUDIT PASS queued-wave branch maps to current shared route; known CPlayer pre-move/proceed gaps remain, so this is not full OnProcess parity")
