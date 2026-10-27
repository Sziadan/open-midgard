#!/usr/bin/env python3
"""Compare the server ground-skill notification schema with the client route."""

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
    server = code(body(ROOT / "Ref/eAthena_src_2011/map/clif.c", "void clif_skill_poseffect("))
    ref_path = ROOT / "Ref/GameModePacket.cpp"
    ref = code(body(ref_path, "void __thiscall CGameMode::Zc_Notify_Groundskill("))
    exe = code(body(ROOT / "HighPriest.exe.c", "void __thiscall CGameMode::Zc_Notify_Groundskill("))
    mode_ref = code(body(ROOT / "Ref/GameMode2.cpp", "ENUM_SKILL_USE_TYPE __thiscall CGameMode::SendMsg("))
    mode_exe = code(body(ROOT / "HighPriest.exe.c", "ENUM_SKILL_USE_TYPE __thiscall CGameMode::SendMsg("))
    current_source = (ROOT / "src/gamemode/GameModePacket.cpp").read_text(encoding="utf-8")
    current = code(body(ROOT / "src/gamemode/GameModePacket.cpp", "void HandleGroundSkillNotify("))
    packet_db = (ROOT / "Ref/RunningServer/packet_db.txt").read_text(encoding="utf-8")
    sizes = (ROOT / "src/network/GronPacket.cpp").read_text(encoding="utf-8")
    profile = (ROOT / "src/network/MapSendProfile.cpp").read_text(encoding="utf-8")

    assert ref == exe, "Ref/EXE ground-skill receiver bodies differ"
    assert hashlib.sha256(ref.encode()).hexdigest() == "d253c3483b79dd6beeb347d1c5c9ada62f740ce58b77d8518457a35f54640514"
    assert mode_ref == mode_exe, "Ref/EXE CGameMode::SendMsg bodies differ"
    assert hashlib.sha256(mode_ref.encode()).hexdigest() == "63005b4c5c7f62a1ad563c2bb8ecff55fefca00698dd7430a93f9f688477527b"
    server_fields = [
        r"WBUFW\(buf,\s*0\)\s*=\s*0x117",
        r"WBUFW\(buf,\s*2\)\s*=\s*skill_id",
        r"WBUFL\(buf,\s*4\)\s*=\s*src->id",
        r"WBUFW\(buf,\s*8\)\s*=\s*val",
        r"WBUFW\(buf,\s*10\)\s*=\s*x",
        r"WBUFW\(buf,\s*12\)\s*=\s*y",
        r"WBUFL\(buf,\s*14\)\s*=\s*tick",
    ]
    positions = []
    for pattern in server_fields:
        match = re.search(pattern, server)
        assert match, f"server packet field write missing: {pattern}"
        positions.append(match.start())
    assert positions == sorted(positions), "server field write order differs"
    assert re.search(r"if\s*\(disguised\(src\)\).*?AREA_WOS.*?WBUFL\(buf,\s*4\)\s*=\s*-src->id.*?SELF", server)
    assert re.search(r"else\s+clif_send\(buf,\s*packet_len\(0x117\),\s*src,\s*AREA\)", server)

    assert re.search(r"^\s*0x0117,18\s*$", packet_db, re.M)
    assert re.search(r"SetPacketSize\(0x0117,\s*18\)", sizes)
    assert re.search(r"groundSkillNotify\s*=\s*0x0117", profile)
    assert "RegisterHandlerIfValid(router, receiveProfile.groundSkillNotify, HandleGroundSkillNotify)" in current_source
    current_fields = [
        r"ReadLE16\(packet\.data\s*\+\s*2\)",
        r"ReadLE32\(packet\.data\s*\+\s*4\)",
        r"static_cast<int16_t>\(ReadLE16\(packet\.data\s*\+\s*8\)\)",
        r"static_cast<int16_t>\(ReadLE16\(packet\.data\s*\+\s*10\)\)",
        r"static_cast<int16_t>\(ReadLE16\(packet\.data\s*\+\s*12\)\)",
        r"ReadLE32\(packet\.data\s*\+\s*14\)",
    ]
    positions = []
    for pattern in current_fields:
        match = re.search(pattern, current)
        assert match, f"current packet field read missing: {pattern}"
        positions.append(match.start())
    assert positions == sorted(positions), "current packet read order differs"
    assert "sourceActor->SendMsg(sourceActor, 38" in current
    assert "skillId == 47" in current and "LaunchGroundArrowShower" in current
    assert "skillId == 362 || skillId == 369" in current
    assert "GetMonsterSkillInfo" in current
    # Ref's omitted message 99 maps to canonical case 0x63 in CGameMode::SendMsg.
    # Its body updates m_skillUsedTick and builds the source skill-use display
    # string; it has no actor message, effect launch, or sound call. The named
    # Ref/dhxj enum corroborates this legacy UI-only message.
    assert "this->CMode::SendMsg(this, 99," in ref
    sendmsg_branch_start = mode_exe.find("case 0x63u:")
    sendmsg_branch_end = mode_exe.find("case 0x64u:", sendmsg_branch_start)
    assert sendmsg_branch_start >= 0 and sendmsg_branch_end > sendmsg_branch_start
    sendmsg_branch = mode_exe[sendmsg_branch_start:sendmsg_branch_end]
    assert "m_skillUsedTick" in sendmsg_branch and "std::string::string" in sendmsg_branch
    assert not any(token in sendmsg_branch for token in ("LaunchEffect", "PlayWave", "CGameActor::SendMsg"))
    assert "mode.SendMsg(99" not in current
    print("clif_skill_poseffect gate passed: 0x0117 wire layout and route match. Source's message-99 skill-use display branch is documented as UI-only and excluded.")


if __name__ == "__main__":
    main()
