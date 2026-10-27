#!/usr/bin/env python3
"""Map CGrannyPc's source hit-wave branch to the current shared actor route."""
from pathlib import Path
import hashlib
import re

ROOT = Path(__file__).resolve().parents[2]
REF = (ROOT / "Ref/Pc.cpp").read_text(encoding="utf-8", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
CURRENT_ACTOR = (ROOT / "src/world/GameActor.cpp").read_text(encoding="utf-8")
CURRENT_WORLD = (ROOT / "src/world/World.cpp").read_text(encoding="utf-8")
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
marker = "unsigned __int8 __thiscall CGrannyPc::OnProcess(CGrannyPc *this)"
ref_sig, ref_body = body(REF, marker)
exe_sig, exe_body = body(EXE, marker)
assert normalize(ref_sig) == normalize(exe_sig) == normalize(marker)
normalized = normalize(ref_body)
assert normalized == normalize(exe_body), "complete Ref/EXE CGrannyPc::OnProcess bodies differ"
assert len(normalized) == 1928
assert hashlib.sha256(normalized.encode()).hexdigest() == \
    "9741db89201901ca2c25362f0a19fe8190143cb23d84f56f853bc2832988ba06"

# Source audio/tint order, with legacy window work identified as excluded.
ordered_source = [
    "CGameActor::ProcessEffectState(&this->CGameActor)",
    "CGameActor::ProcessWillBeAttacked(&this->CGameActor)",
    "CGameActor::ProcessChatBalloon(&this->CGameActor)",
    "this->m_isSingleColor && timeGetTime() > this->m_singleColorStartTick + 100",
    "this->m_sprArgb != -1",
    "CRenderObject::ModifyArgb(&this->CGameActor, 8, 8, 8, 8)",
    "this->CGameActor::ProcessState(this)",
    "CRenderObject::ProcessSound(&this->CGameActor)",
    "if ( this->m_isPlayHitWave )",
    "this->m_pos.z - p_m_pos->z",
    "this->m_pos.x - p_m_pos->x",
    "PlayWave(this->m_hitWaveName, x, 0.0, z, 250, 40, 1.0)",
    "this->m_isPlayHitWave = 0",
    "if ( this->m_roty > 360.0 )",
    "if ( this->m_isFirstProcess == 1 )",
    "this->m_fCurRot = v4",
    "v5 = (__int64)(this->m_fCurRot -",
    "if ( this->m_fCurRot > 360.0 )",
    "if ( this->m_fCurRot < 0.0 )",
]
positions = [ref_body.index(item) for item in ordered_source]
assert positions == sorted(positions), "source OnProcess branch order changed"
assert "m_gage->UIWindow::Move" in ref_body

# Current CGrannyPc is dormant: there are no actor-factory construction
# expressions in current src. If activated, it inherits CGameActor::ProcessState,
# which owns the shared post-motion hit-wave consumer.
assert "class CGrannyPc : public CGameActor" in HEADER
src_text = "\n".join(path.read_text(encoding="utf-8", errors="replace")
                    for path in (ROOT / "src").rglob("*") if path.is_file()
                    and path.suffix in {".cpp", ".h", ".hpp"})
for pattern in (r"\bnew\s+CGrannyPc\b", r"make_unique\s*<\s*CGrannyPc\s*>",
                r"make_shared\s*<\s*CGrannyPc\s*>"):
    assert not re.search(pattern, src_text), f"current CGrannyPc construction found: {pattern}"
_, process_state = body(CURRENT_ACTOR, "u8 CGameActor::ProcessState()")
assert "ProcessQueuedHitWave(*this);" in process_state
assert "Ref runtime-wave: 0x5D9E95 0x5E0FD6" in process_state
_, queued = body(CURRENT_ACTOR, "void ProcessQueuedHitWave(CGameActor& actor)")
assert "if (actor.m_isPlayHitWave == 0)" in queued
play_pos = queued.index("TryPlayQueuedWaveAtActor(actor, actor.m_hitWaveName);")
clear_pos = queued.index("actor.m_isPlayHitWave = 0;", play_pos)
assert play_pos < clear_pos
_, helper = body(CURRENT_ACTOR, "bool TryPlayQueuedWaveAtActor(const CGameActor& actor, const char* waveName)")
assert "PlayWave(waveName, actor.m_pos.x - listener.x, 0.0f," in helper
assert "actor.m_pos.z - listener.z" in helper
print("PASS Ref/EXE CGrannyPc body and source audio/tint/rotation order; shared current post-motion hit-wave route; current class has no construction route")
