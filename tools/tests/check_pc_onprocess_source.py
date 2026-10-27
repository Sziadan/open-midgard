#!/usr/bin/env python3
"""Compare CPc::OnProcess's active queued-hit-wave branch to its current route."""
from pathlib import Path
import hashlib
import re

ROOT = Path(__file__).resolve().parents[2]
REF = (ROOT / "Ref/Pc.cpp").read_text(encoding="utf-8", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
CURRENT = (ROOT / "src/world/GameActor.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "src/world/GameActor.h").read_text(encoding="utf-8")
AUDIO_HEADER = (ROOT / "src/audio/Audio.h").read_text(encoding="utf-8")


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
marker = "unsigned __int8 __thiscall CPc::OnProcess(CPc *this)"
ref_sig, ref_body = body(REF, marker)
exe_sig, exe_body = body(EXE, marker)
assert normalize(ref_sig) == normalize(exe_sig) == normalize(marker)
normalized = normalize(ref_body)
assert normalized == normalize(exe_body), "complete Ref/EXE CPc::OnProcess bodies differ"
assert len(normalized) == 1027
assert hashlib.sha256(normalized.encode()).hexdigest() == \
    "8de8153591680520e7eacc0f973685c1dd20db962ff96075eba1836274d350d7"

# Compare in-scope source behavior in its exact ordered branch. UIPcGage::Move
# is intentionally excluded as legacy UI-only behavior.
ordered_source = [
    "CGameActor::ProcessEffectState(this)",
    "CGameActor::ProcessWillBeAttacked(this)",
    "CGameActor::ProcessChatBalloon(this)",
    "this->m_isSingleColor && timeGetTime() > this->m_singleColorStartTick + 100",
    "this->m_colorOfSingleColor = -65536",
    "CRenderObject::ModifyArgb(this, 8, 8, 8, 8)",
    "this->CGameActor::ProcessState(this)",
    "CRenderObject::ProcessSound(this)",
    "if ( this->m_isPlayHitWave )",
    "this->m_pos.z - p_m_pos->z",
    "this->m_pos.x - p_m_pos->x",
    "PlayWave(this->m_hitWaveName, x, 0.0, z, 250, 40, 1.0)",
    "this->m_isPlayHitWave = 0",
]
source_positions = [ref_body.index(item) for item in ordered_source]
assert source_positions == sorted(source_positions), "source gameplay/audio branch order changed"

# Current equivalent is centralized in ProcessState because the modern actor
# hierarchy folds the PC override into CGameActor. Playback remains after motion
# event production, uses actor-relative X/Z and PlayWave defaults, then clears
# only the pending flag after the attempt, matching the source queued branch.
assert "u8 OnProcess() override;" not in HEADER[HEADER.index("class CPc :"):HEADER.index("class CPlayer :")]
_, queued = body(CURRENT, "void ProcessQueuedHitWave(CGameActor& actor)")
assert "if (actor.m_isPlayHitWave == 0)" in queued
assert "m_hitWaveName[0]" not in queued
play_pos = queued.index("TryPlayQueuedWaveAtActor(actor, actor.m_hitWaveName);")
clear_pos = queued.index("actor.m_isPlayHitWave = 0;", play_pos)
assert play_pos < clear_pos
assert "actor.m_isPlayHitWave = 0;" not in queued[:play_pos]
_, try_play = body(CURRENT, "bool TryPlayQueuedWaveAtActor(const CGameActor& actor, const char* waveName)")
assert "PlayWave(waveName, actor.m_pos.x - listener.x, 0.0f," in try_play
assert "actor.m_pos.z - listener.z" in try_play
assert "int volumeMaxDist = 250, int volumeMinDist = 40, float vFactor = 1.0f" in AUDIO_HEADER
_, process_state = body(CURRENT, "u8 CGameActor::ProcessState()")
assert "ProcessQueuedHitWave(*this);" in process_state
assert process_state.index("ProcessQueuedHitWave(*this);") > process_state.index("ProcessMotion();")
assert "Ref runtime-wave: 0x5D9E95" in process_state
assert "m_gage->UIWindow::Move" in ref_body
print("PASS Ref/EXE CPc::OnProcess equality; ordered gameplay/audio branches; current post-motion queued wave, actor-relative PlayWave defaults, and source flag-clear timing")
