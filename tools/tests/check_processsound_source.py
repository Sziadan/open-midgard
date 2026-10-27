#!/usr/bin/env python3
"""Compare the executable ProcessSound branches with the current motion-wave route."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
REF = (ROOT / "Ref/GameActor.cpp").read_text(encoding="utf-8", errors="replace")
GAME_MODE_REF = (ROOT / "Ref/GameMode.cpp").read_text(encoding="utf-8", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
CURRENT = (ROOT / "src/world/GameActor.cpp").read_text(encoding="utf-8")
MSG_EFFECT = (ROOT / "src/world/MsgEffect.cpp").read_text(encoding="utf-8")


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
marker = "void __thiscall CRenderObject::ProcessSound(CRenderObject *this)"
ref_sig, ref = body(REF, marker)
exe_sig, exe = body(EXE, marker)
assert normalize(ref_sig) == normalize(exe_sig) == normalize(marker)
assert normalize(ref) == normalize(exe), "Ref and canonical EXE ProcessSound bodies differ"
assert len(normalize(ref)) == 5686, "canonical ProcessSound body changed; redo branch comparison"

# Compare the three source dispatch branches in order, not only their call count.
assert ref.count("PlayWave(") == 3
assert [ref.index("PlayWave("), ref.index("PlayWave(", ref.index("PlayWave(") + 1),
        ref.rindex("PlayWave(")] == sorted(
            [ref.index("PlayWave("), ref.index("PlayWave(", ref.index("PlayWave(") + 1),
             ref.rindex("PlayWave(")])
assert ref.index("if ( m_curMotion > m_oldMotion )") < ref.index("if ( m_curMotion < m_oldMotion )")
assert ref.index("PlayWave(") < ref.index("PlayWave(", ref.index("PlayWave(") + 1) < ref.rindex("PlayWave(")
assert "m_oldMotion + 1" in ref and "v15 < numMotion" in ref and "v26 > v40" in ref
assert ref.count("std::string::find(&eventName, aWav") == 2
assert "memchr(v49._Ptr, aWav[0]" in ref and "memcmp(v32, aWav, v30)" in ref
assert ref.count("PlayWave(") == exe.count("PlayWave(")

# Current actor path must keep the executable's event-name filter and spatial arguments.
assert "bool ContainsAsciiCaseSensitive(" in CURRENT
assert 'ContainsAsciiCaseSensitive(eventName, ".wav")' in CURRENT
assert "ContainsAsciiCaseInsensitive" not in CURRENT
assert "PlayWave(waveName, actor.m_pos.x - listener.x, 0.0f," in CURRENT
assert "actor.m_pos.y - listener.y" not in CURRENT
assert "PlayMotionWaveEvents(object, actRes, action, previousMotion, clampedMotion)" in CURRENT
assert "playRange(oldMotion + 1, newMotion)" in CURRENT
assert "playRange(oldMotion + 1, motionCount - 1)" in CURRENT
assert "playRange(0, newMotion)" in CURRENT
assert "motionIndex <= last" in CURRENT and "motion->eventId < 0" in CURRENT

# Record implementation limits as test assertions so this gate cannot imply full base-class parity.
assert "dynamic_cast<CGameActor*>(object)" in CURRENT
assert "trackMotionWaveEventsInProcessMotion" in CURRENT
assert "actor.m_isPc != 0" in CURRENT
assert "ProcessQueuedHitWave(*this)" in CURRENT
assert "bool ContainsAsciiCaseSensitive(" in MSG_EFFECT
assert 'ContainsAsciiCaseSensitive(eventName, ".wav")' in MSG_EFFECT
assert "ContainsAsciiCaseInsensitive" not in MSG_EFFECT
assert "const vector3d& origin = effect.m_pos;" in MSG_EFFECT
assert "PlayWave(eventName, origin.x - listener.x, 0.0f, origin.z - listener.z)" in MSG_EFFECT
wave_rows = (ROOT / "docs/skill/generated/runtime_wave_callsites.tsv").read_text(encoding="utf-8").splitlines()
for va in ("0x555B8E", "0x555CCF", "0x555E5F"):
    row = next(row for row in wave_rows if row.startswith(va + "\t"))
    assert "src/world/GameActor.cpp" in row and "port-anchor-present" in row

# The only other EXE caller is CBlowEffect::OnProcess, but its sole constructor
# factory has no caller in either canonical EXE or Ref, so it is dead-code scope.
make_marker = "void __thiscall CGameMode::MakeBlowEffect(CGameMode *this, float cx, float cy, float cz)"
_, make_body = body(EXE, make_marker)
assert EXE.count("CGameMode::MakeBlowEffect(") == 2, "MakeBlowEffect gained an executable callsite"
assert "CBlowEffect::CBlowEffect(v5)" in make_body
assert GAME_MODE_REF.count("CGameMode::MakeBlowEffect(") == 2, "Ref gained a MakeBlowEffect callsite"
print("PASS Ref/EXE ProcessSound body equality (5,686 normalized chars), ordered forward/wrap branches, event filter, Y=0 playback, actor/CMsgEffect producers, and no live MakeBlowEffect/CBlowEffect factory route")
