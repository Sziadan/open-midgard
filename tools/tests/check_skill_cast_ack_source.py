#!/usr/bin/env python3
"""Compare cast-ack source branches with the current handler."""

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
                return source[opening:index + 1]
    raise AssertionError(f"unterminated body in {path}")


def code(source: str) -> str:
    source = re.sub(r"/\*.*?\*/|//[^\n]*", " ", source, flags=re.S)
    return re.sub(r"\s+", " ", source).strip()


def ordered(source: str, fragments: list[str], label: str) -> None:
    positions = []
    for fragment in fragments:
        position = source.find(fragment)
        assert position >= 0, f"{label}: missing {fragment}"
        positions.append(position)
    assert positions == sorted(positions), f"{label}: operation order differs"


def main() -> None:
    ref_path = ROOT / "Ref/GameModePacket.cpp"
    exe_path = ROOT / "HighPriest.exe.c"
    ref = code(body(ref_path, "void __thiscall CGameMode::Zc_useskill_ack("))
    exe = code(body(exe_path, "void __thiscall CGameMode::Zc_useskill_ack("))
    assert ref == exe, "Ref/EXE cast-ack complete function bodies differ"
    assert hashlib.sha256(ref.encode()).hexdigest() == "43734ae66b927e952797158726cc5f29d8282193b3b35af8e9aedcf15c0c74d2"

    packet_db = (ROOT / "Ref/RunningServer/packet_db.txt").read_text(encoding="utf-8")
    packet_sizes = (ROOT / "src/network/GronPacket.cpp").read_text(encoding="utf-8")
    assert re.search(r"^\s*0x013e,24\s*$", packet_db, re.M | re.I)
    assert re.search(r"SetPacketSize\(0x013E,\s*24\)", packet_sizes, re.I)
    for path in (ref_path, exe_path):
        dispatch = path.read_text(encoding="utf-8", errors="replace")
        assert re.search(r"case\s+318\s*:\s*\n\s*CGameMode::Zc_useskill_ack", dispatch)

    current_path = ROOT / "src/gamemode/GameModePacket.cpp"
    current_source = current_path.read_text(encoding="utf-8")
    current = code(body(current_path, "void HandleSkillCastAck("))
    assert "router.Register(0x013E, HandleSkillCastAck)" in current_source
    assert "packet.packetLength < 24" in current
    offsets = [
        "ReadLE32(packet.data + 2)", "ReadLE32(packet.data + 6)",
        "ReadLE16(packet.data + 10)", "ReadLE16(packet.data + 12)",
        "ReadLE16(packet.data + 14)", "ReadLE32(packet.data + 16)",
        "ReadLE32(packet.data + 20)",
    ]
    ordered(current, offsets, "packet field reads")

    # Reference branches mapped one-to-one to the current handler.
    source_branches = [
        "if ( v4 == g_session.m_aid )", "if ( m_player )", "if ( v7 == 62 || v7 == 57 )",
        "if ( CSession::IsMonster(&g_session, m_player) )", "if ( v12 )",
        "if ( GameActorByAID && m_player != GameActorByAID )", "if ( *((_WORD *)v2 + 7) == 490 )",
        "else", "if ( (unsigned __int16)v33 > 0x191u )", "if ( (unsigned __int16)v33 == 401 )",
        "if ( v34 == 7 )",
    ]
    for branch in source_branches:
        assert branch in ref, f"Ref cast-ack branch missing: {branch}"
    current_branches = [
        "if (sourceActor == nullptr)",
        "skillId == 62 || skillId == 57", "IsMonsterSkillActor(sourceActor)",
        "if (dstGid != 0)", "targetActor != sourceActor", "skillId == 490",
        "else if (mode.m_world != nullptr)", "skillId == 401", "skillId == 268",
        "skillId == 261 || skillId == 1015",
    ]
    for branch in current_branches:
        assert branch in current, f"current cast-ack branch missing: {branch}"

    # Source does lookup only. Assert both current endpoint resolutions do too,
    # preventing this packet from creating or upcasting actors.
    assert "ResolveSkillPacketActor(mode, srcGid)" in current and "ResolveSkillPacketActor(mode, dstGid)" in current
    assert "ResolveCombatActor" not in current and "EnsureRuntimeActor" not in current
    ordered(current, [
        "sourceActor->SetState(motionType)", "sourceActor->SendMsg(sourceActor, 85",
        "sourceActor->SendMsg(sourceActor, 82", "targetActor->SendMsg(targetActor, 86",
        "sourceActor->LaunchEffect(539", "mode.m_world->m_gameObjectList.push_back(scope)",
    ], "cast-ack operation flow")
    assert "sourceActor->m_isMotionFreezed = 1" in current
    assert "castTimeMs / 300" in current
    assert "SetForceAnimation(96, 0, 0, 96, 0, 0, 0, 0)" in current
    assert "static_cast<u32>(castTimeMs) + 1000u" in current
    print("0x013E Ref/EXE, packet fields, major branches, lookup semantics, and operation order verified")


if __name__ == "__main__":
    main()
