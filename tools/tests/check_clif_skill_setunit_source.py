#!/usr/bin/env python3
"""Compare ordinary and graffiti skill-unit encoders with both client routes."""

from __future__ import annotations

import hashlib
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def body(path: Path, signature: str) -> str:
    source = path.read_text(encoding="utf-8", errors="replace")
    start = source.rfind(signature)
    if start < 0:
        raise AssertionError(f"missing {signature!r} in {path}")
    opening = source.index("{", start)
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening : index + 1]
    raise AssertionError(f"unterminated body in {path}")


def code(source: str) -> str:
    source = re.sub(r"/\*.*?\*/|//[^\n]*", " ", source, flags=re.S)
    return re.sub(r"\s+", " ", source).strip()


def main() -> None:
    server = code(body(ROOT / "Ref/eAthena_src_2011/map/clif.c", "void clif_skill_setunit("))
    graffiti_server, basic_server = server.split("#endif", 1)
    assert re.search(r"#if\s+PACKETVER\s*>=\s*3", graffiti_server)
    ref_path = ROOT / "Ref/GameModePacket.cpp"
    ref_entry = code(body(ref_path, "void __thiscall CGameMode::Zc_Skill_Entry("))
    exe_entry = code(body(ROOT / "HighPriest.exe.c", "void __thiscall CGameMode::Zc_Skill_Entry("))
    ref_graffiti = code(body(ref_path, "void __thiscall CGameMode::Zc_Skill_Entry2("))
    exe_graffiti = code(body(ROOT / "HighPriest.exe.c", "void __thiscall CGameMode::Zc_Skill_Entry2("))
    current_source = (ROOT / "src/gamemode/GameModePacket.cpp").read_text(encoding="utf-8")
    current_basic = code(body(ROOT / "src/gamemode/GameModePacket.cpp", "void HandleSkillUnitSet("))
    current_extended = code(body(ROOT / "src/gamemode/GameModePacket.cpp", "void HandleSkillUnitSet2("))
    placement = code(body(ROOT / "src/gamemode/GameModePacket.cpp", "void PlaceSkillUnit("))
    visibility = code(body(ROOT / "src/gamemode/GameModePacket.cpp", "void RememberVisibleTrap("))
    db = (ROOT / "Ref/RunningServer/packet_db.txt").read_text(encoding="utf-8")
    sizes = (ROOT / "src/network/GronPacket.cpp").read_text(encoding="utf-8")
    profile = (ROOT / "src/network/MapSendProfile.cpp").read_text(encoding="utf-8")

    assert ref_entry == exe_entry, "Ref/EXE Zc_Skill_Entry bodies differ"
    assert ref_graffiti == exe_graffiti, "Ref/EXE Zc_Skill_Entry2 bodies differ"
    assert hashlib.sha256(ref_entry.encode()).hexdigest() == "f4ee2b17628ecd79895180d4dcfc2fc05f1146c90993a6db1a6a36fd7f5ad2ad"
    assert hashlib.sha256(ref_graffiti.encode()).hexdigest() == "6c0366507f3579e874cc845a5639c041fceb66bacecbbd1f428d6d921d28315f"

    basic_fields = [
        r"WBUFW\(buf,\s*0\)\s*=\s*0x11f", r"WBUFL\(buf,\s*2\)\s*=\s*unit->bl.id",
        r"WBUFL\(buf,\s*6\)\s*=\s*unit->group->src_id", r"WBUFW\(buf,\s*10\)\s*=\s*unit->bl.x",
        r"WBUFW\(buf,\s*12\)\s*=\s*unit->bl.y", r"WBUFB\(buf,\s*14\)\s*=\s*unit->group->unit_id",
        r"WBUFB\(buf,\s*15\)\s*=\s*1",
    ]
    basic_positions = []
    for pattern in basic_fields:
        match = re.search(pattern, basic_server)
        assert match, f"basic skill-unit field missing: {pattern}"
        basic_positions.append(match.start())
    assert basic_positions == sorted(basic_positions)
    assert "unit->group->state.song_dance&0x1" in basic_server
    assert "UNT_DISSONANCE:UNT_UGLYDANCE" in basic_server

    graffiti_fields = [
        r"WBUFW\(buf,\s*0\)\s*=\s*0x1c9", r"WBUFL\(buf,\s*2\)\s*=\s*unit->bl.id",
        r"WBUFL\(buf,\s*6\)\s*=\s*unit->group->src_id", r"WBUFW\(buf,\s*10\)\s*=\s*unit->bl.x",
        r"WBUFW\(buf,\s*12\)\s*=\s*unit->bl.y", r"WBUFB\(buf,\s*14\)\s*=\s*unit->group->unit_id",
        r"WBUFB\(buf,\s*15\)\s*=\s*1", r"WBUFB\(buf,\s*16\)\s*=\s*1",
        r"safestrncpy\(\(char\*\)WBUFP\(buf,\s*17\),\s*unit->group->valstr,\s*MESSAGE_SIZE\)",
    ]
    graffiti_positions = []
    for pattern in graffiti_fields:
        match = re.search(pattern, graffiti_server)
        assert match, f"graffiti skill-unit field missing: {pattern}"
        graffiti_positions.append(match.start())
    assert graffiti_positions == sorted(graffiti_positions)
    assert "unit->group->unit_id==UNT_GRAFFITI" in graffiti_server

    for pattern in (r"0x011f,16", r"0x01c9,97"):
        assert re.search(rf"^\s*{pattern}\s*$", db, re.M | re.I), f"packet_db entry missing: {pattern}"
    for pattern in (r"SetPacketSize\(0x011F,\s*16\)", r"SetPacketSize\(0x01C9,\s*97\)"):
        assert re.search(pattern, sizes, re.I), f"current packet length missing: {pattern}"
    assert re.search(r"skillUnitSetBasic\s*=\s*0x011F", profile)
    assert re.search(r"skillUnitSetExtended\s*=\s*0x01C9", profile)
    assert "RegisterHandlerIfValid(router, receiveProfile.skillUnitSetBasic, HandleSkillUnitSet)" in current_source
    assert "RegisterHandlerIfValid(router, receiveProfile.skillUnitSetExtended, HandleSkillUnitSet2)" in current_source

    for pattern in (r"ReadLE32\(packet\.data\s*\+\s*2\)", r"ReadLE32\(packet\.data\s*\+\s*6\)",
                    r"ReadLE16\(packet\.data\s*\+\s*10\)", r"ReadLE16\(packet\.data\s*\+\s*12\)",
                    r"packet\.data\[14\]"):
        assert re.search(pattern, current_basic), f"basic current receive field missing: {pattern}"
    assert "RememberVisibleTrap(mode, unitAid, creatorAid, visible)" in current_basic
    assert "PlaceSkillUnit(mode, unitAid, creatorAid, job, cellX, cellY, visible)" in current_basic
    assert "job < 0x2D" in placement and "SendMsg(skill, 45" in placement
    assert "GetSkillByAID(unitAid)" in placement and "mode.m_world->m_gameObjectList.push_back(skill)" in placement
    assert "mode.m_world->m_skillList.push_back(skill)" in placement
    assert "creatorAid == g_session.m_aid || traps.empty()" in visibility
    assert "if (*it == unitAid)" in visibility and "traps.erase(it)" in visibility and "visible = 0" in visibility
    assert "traps.push_back(unitAid)" in visibility
    assert "if (packet.data[16] == 1)" in current_extended
    assert "ReadLE16(packet.data + 10)" in current_extended and "ReadLE16(packet.data + 12)" in current_extended
    assert "std::memcpy(text, packet.data + 17, 79)" in current_extended and "text[79] = '\\0'" in current_extended
    assert "CGraffiSkill" in current_extended and "SendMsg(graffiti, 22" in current_extended
    assert "SendMsg(graffiti, 117" in current_extended
    assert "PlaceSkillUnit(mode, unitAid, creatorAid, job, cellX, cellY, visible)" in current_extended
    ref_dispatch = code(ref_path.read_text(encoding="utf-8", errors="replace"))
    assert re.search(r"case 287:\s*CGameMode::Zc_Skill_Entry\(this,\s*buf\)", ref_dispatch)
    assert re.search(r"case 457:\s*CGameMode::Zc_Skill_Entry2\(this,\s*buf\)", ref_dispatch)

    print("clif_skill_setunit gate passed: basic 0x011F and graffiti 0x01C9 layouts, packet lengths, Ref/EXE receivers, trap visibility, job threshold, and graffiti text route match.")


if __name__ == "__main__":
    main()
