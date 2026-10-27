#!/usr/bin/env python3
"""Compare active CSoundMaker map ambience with the centralized current sound route."""
from pathlib import Path
import hashlib
import re

ROOT = Path(__file__).resolve().parents[2]
REF_NPC = (ROOT / "Ref/Npc.cpp").read_text(encoding="utf-8", errors="replace")
REF_WORLD = (ROOT / "Ref/GameWorld.cpp").read_text(encoding="utf-8", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
CURRENT_MODE = (ROOT / "src/gamemode/GameMode.cpp").read_text(encoding="utf-8")
CURRENT_WORLD_RES = (ROOT / "src/res/WorldRes.h").read_text(encoding="utf-8")
CURRENT_AUDIO = (ROOT / "src/audio/Wave.cpp").read_text(encoding="utf-8")
CURRENT_AUDIO_HEADER = (ROOT / "src/audio/Audio.h").read_text(encoding="utf-8")


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
process_marker = "unsigned __int8 __thiscall CSoundMaker::OnProcess(CSoundMaker *this)"
ref_sig, ref_process = body(REF_NPC, process_marker)
exe_sig, exe_process = body(EXE, process_marker)
assert normalize(ref_sig) == normalize(exe_sig) == normalize(process_marker)
normalized_process = normalize(ref_process)
assert normalized_process == normalize(exe_process)
assert len(normalized_process) == 1920
assert hashlib.sha256(normalized_process.encode()).hexdigest() == \
    "226ea97f520acf8e79603270351bcd599f232b76f05f03669c409c1508c1f849"

# The one PlayWave branch is after cull -> quadrant/edge projection -> boundary,
# name and strict cycle checks, and receives the source edge/listener offsets.
assert ref_process.count("PlayWave(") == 1
ordered = [
    "CalcDist(&this->CRenderObject, m_player->m_pos.x, m_player->m_pos.z)",
    "if ( v3 >= 0.0 )",
    "cZ = v4 / v3",
    "abs32((__int64)cZ)",
    "if ( v6 )",
    "CalcDist(m_player, yPoleb, cZa) - this->m_range <= 75.0",
    "this->m_waveName._Len",
    "timeGetTime() > this->m_lastPlayTime + this->m_cycle",
    "this->m_lastPlayTime = timeGetTime()",
    "PlayWave(Ptr, x, 0.0, z, (__int64)this->m_range, (__int64)(this->m_range * 0.16666667), this->m_vol)",
]
positions = [ref_process.index(item) for item in ordered]
assert positions == sorted(positions), "source cull/edge/audibility/cycle/play order changed"
assert "m_height / m_width" in ref_process

# Verify the factory and message-14 payload bind every map RSW sound source to
# its runtime wave name, geometry, volume, range, position, and cycle.
world_marker = "void __thiscall CWorld::MakeFixedActor(CWorld *this)"
world_ref_sig, world_ref = body(REF_WORLD, world_marker)
world_exe_sig, world_exe = body(EXE, world_marker)
assert normalize(world_ref_sig) == normalize(world_exe_sig) == normalize(world_marker)
assert normalize(world_ref) == normalize(world_exe), "Ref/EXE sound-source factory bodies differ"
assert "if ( g_soundMode )" in world_ref and "world->m_sounds" in world_ref
assert "CSoundMaker::CSoundMaker" in world_ref and "14" in world_ref

send_marker = "void __thiscall CSoundMaker::SendMsg(CSoundMaker *this, CGameObject *sender, int message, int arg1, int arg2, int arg3)"
send_ref = body(REF_NPC, send_marker)[1]
send_exe = body(EXE, send_marker)[1]
assert normalize(send_ref) == normalize(send_exe), "Ref/EXE runtime sound-source copy differs"
for token in ("arg1 + 80", "v7 + 172", "v7 + 176", "v7 + 180", "v7 + 184", "v7 + 160", "v7 + 188"):
    assert token in send_ref, f"map sound-source field copy missing: {token}"

# Compare the active centralized current replacement, including geometry and
# actual PlayWave spatial/provider parameters (not just its filename producer).
current_geometry = body(CURRENT_MODE, "bool ResolveAmbientSoundTrigger(")[1]
geometry_tokens = [
    "centerDistance - (soundDiagonal + soundRange) > kAmbientSoundCullSlack",
    "const float slope = verticalAxis ? 0.0f : playerDz / playerDx",
    "soundHeight / soundWidth",
    "static_cast<std::int64_t>(slope)",
    "absoluteSlope <= integerAspect",
    "static_cast<float>(soundWidth) * xDirection * 0.5f",
    "edgeOffsetZ = edgeOffsetX * slope",
    "static_cast<float>(soundHeight) * zDirection * 0.5f",
    "edgeOffsetX = verticalAxis ? 0.0f : edgeOffsetZ / slope",
    "edgeDistance - soundRange > kAmbientSoundBoundarySlack",
]
geometry_positions = [current_geometry.index(item) for item in geometry_tokens]
assert geometry_positions == sorted(geometry_positions), "current sound edge math/order differs from mapped source branches"

_, update = body(CURRENT_MODE, "void UpdateMapAudio(CGameMode& mode)")
assert "C3dWorldRes::soundSrcInfo* sound : worldRes->m_sounds" in update
assert "const u32 cycleMs = static_cast<u32>(static_cast<s64>(sound->cycle * 1000.0f))" in update
assert "info.endTick = now - cycleMs" in update
assert "if (now > playingState->endTick + playingState->term)" in update
play_at = update.index("PlayWave(wavName.c_str(), trigger.relativeX, 0.0f, trigger.relativeZ,")
assert update.rindex("playingState->endTick = GetTickCount();", 0, play_at) >= 0
assert "static_cast<int>(sound->range)" in update
assert "sound->range * 0.16666667f" in update
assert "sound->vol);" in update
assert "audio->PlaySound(wavName.c_str()" not in update
_, on_update = body(CURRENT_MODE, "void CGameMode::OnUpdate()")
assert "UpdateMapAudio(*this)" in on_update
for field in ("char waveName[80]", "vector3d pos", "float vol", "int width", "int height", "float range", "float cycle"):
    assert field in CURRENT_WORLD_RES

relative_marker = "bool PlayWave(const char* name, float x, float y, float z,"
_, relative_body = body((ROOT / "src/audio/Wave.cpp").read_text(encoding="utf-8"), relative_marker)
assert "PlayWaveRelative(name, x, y, z" in relative_body
assert "return m_backend->PlayWave3D(name, x, y, z, volumeMaxDist, volumeMinDist," in CURRENT_AUDIO
assert "bool PlayWaveRelative(const char* fName" in CURRENT_AUDIO_HEADER

wave_rows = (ROOT / "docs/skill/generated/runtime_wave_callsites.tsv").read_text(encoding="utf-8").splitlines()
wave_row = next(row for row in wave_rows if row.startswith("0x5D6A53\t"))
assert "src/gamemode/GameMode.cpp" in wave_row and "port-anchor-present" in wave_row
print("PASS Ref/EXE CSoundMaker body/factory/runtime fields; current map-audio cull, integer edge branch, strict cycle, provider-backed range attenuation, and runtime producer mapping")
