#!/usr/bin/env python3
"""Gate the 0x0120 skill-unit removal route against Ref and the canonical EXE."""

from __future__ import annotations

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
                return source[opening:index + 1]
    raise AssertionError(f"unterminated body in {path}")


def code(source: str) -> str:
    source = re.sub(r"/\*.*?\*/|//[^\n]*", " ", source, flags=re.S)
    return re.sub(r"\s+", " ", source).strip()


def main() -> None:
    ref_packet = code(body(ROOT / "Ref/GameModePacket.cpp", "void __thiscall CGameMode::Zc_Skill_Disappear("))
    exe_packet = code(body(ROOT / "HighPriest.exe.c", "void __thiscall CGameMode::Zc_Skill_Disappear("))
    assert ref_packet == exe_packet, "Ref/EXE skill-disappear receiver bodies differ"
    assert re.search(r"CWorld::RemoveSkill\(this->m_world,\s*\*\(_DWORD \*\)\(buf \+ 2\)\)", ref_packet)

    server = code(body(ROOT / "Ref/eAthena_src_2011/map/clif.c", "void clif_skill_delunit("))
    assert re.search(r"WBUFW\(buf,\s*0\)\s*=\s*0x120", server)
    assert re.search(r"WBUFL\(buf,\s*2\)\s*=\s*unit->bl.id", server)
    assert re.search(r"packet_len\(0x120\)", server)
    packet_db = (ROOT / "Ref/RunningServer/packet_db.txt").read_text(encoding="utf-8")
    packet_sizes = (ROOT / "src/network/GronPacket.cpp").read_text(encoding="utf-8")
    assert re.search(r"^\s*0x0120,6\s*$", packet_db, re.M | re.I)
    assert re.search(r"SetPacketSize\(0x0120,\s*6\)", packet_sizes, re.I)

    current_source = (ROOT / "src/gamemode/GameModePacket.cpp").read_text(encoding="utf-8")
    current_handler = code(body(ROOT / "src/gamemode/GameModePacket.cpp", "void HandleSkillDisappear("))
    assert "Register(0x0120, HandleSkillDisappear)" in current_source
    assert "packet.packetLength < 6" in current_handler
    assert "ReadLE32(packet.data + 2)" in current_handler
    assert "mode.m_world->RemoveSkill" in current_handler

    ref_remove = code(body(ROOT / "Ref/GameWorld.cpp", "unsigned __int8 __thiscall CWorld::RemoveSkill("))
    current_remove = code(body(ROOT / "src/world/World.cpp", "bool CWorld::RemoveSkill("))
    assert re.search(r"Value->CSkill::SetRenderInfo\)\(Value, 1\)", ref_remove)
    assert "m_gameObjectList.remove(skill)" in current_remove and "delete skill" in current_remove
    mismatch = (ROOT / "docs/skill/MISMATCHES.md").read_text(encoding="utf-8")
    assert "# `0x0120` skill removal lifecycle" in mismatch
    assert "legacy actor picking" in mismatch
    assert "modern world removal deletes it" in mismatch
    print("0x0120 encoder, Ref/EXE receiver, current routing, and documented removal-lifecycle delta verified")


if __name__ == "__main__":
    main()
