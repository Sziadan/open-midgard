#!/usr/bin/env python3
"""Compare the server's no-damage skill packet encoder with the client route."""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def body(path: Path, signature: str) -> str:
    text = path.read_text(encoding="utf-8", errors="replace")
    start = text.rfind(signature)
    if start < 0:
        raise AssertionError(f"missing {signature!r} in {path}")
    opening = text.index("{", start)
    depth = 0
    for index in range(opening, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[opening : index + 1]
    raise AssertionError(f"unterminated body in {path}")


def code(source: str) -> str:
    source = re.sub(r"/\*.*?\*/|//[^\n]*", " ", source, flags=re.S)
    return re.sub(r"\s+", " ", source).strip()


def main() -> None:
    server_path = ROOT / "Ref/eAthena_src_2011/map/clif.c"
    server = code(body(server_path, "int clif_skill_nodamage("))
    client_ref_path = ROOT / "Ref/GameModePacket.cpp"
    client_ref = code(body(client_ref_path, "void __thiscall CGameMode::Zc_Use_Skill("))
    client_exe = code(body(ROOT / "HighPriest.exe.c", "void __thiscall CGameMode::Zc_Use_Skill("))
    current_text = (ROOT / "src/gamemode/GameModePacket.cpp").read_text(encoding="utf-8")
    current_receiver = code(body(ROOT / "src/gamemode/GameModePacket.cpp", "void HandleSkillNoDamageNotify("))
    packet_db = (ROOT / "Ref/RunningServer/packet_db.txt").read_text(encoding="utf-8")
    network = (ROOT / "src/network/GronPacket.cpp").read_text(encoding="utf-8")
    profile = (ROOT / "src/network/MapSendProfile.cpp").read_text(encoding="utf-8")

    assert client_ref == client_exe, "Ref and EXE Zc_Use_Skill bodies differ"
    expected_writes = [
        r"WBUFW\(buf,\s*0\)\s*=\s*0x11a",
        r"WBUFW\(buf,\s*2\)\s*=\s*skill_id",
        r"WBUFW\(buf,\s*4\)\s*=\s*min\(heal,\s*INT16_MAX\)",
        r"WBUFL\(buf,\s*6\)\s*=\s*dst->id",
        r"WBUFL\(buf,\s*10\)\s*=\s*src\s*\?\s*src->id\s*:\s*0",
        r"WBUFB\(buf,\s*14\)\s*=\s*fail",
    ]
    write_positions = []
    for pattern in expected_writes:
        match = re.search(pattern, server)
        assert match, f"server packet field write missing: {pattern}"
        write_positions.append(match.start())
    assert write_positions == sorted(write_positions), "server packet field write order changed"
    assert re.search(r"if\s*\(disguised\(dst\)\).*?AREA_WOS.*?SELF", server)
    assert re.search(r"else\s+clif_send\(buf,\s*packet_len\(0x11a\),\s*dst,\s*AREA\)", server)
    assert re.search(r"if\s*\(src\s*&&\s*disguised\(src\)\).*?SELF", server)
    assert "return fail;" in server
    assert re.search(r"^\s*0x011a,15\s*$", packet_db, re.M), "packet_ver 23 must define a 15-byte 0x011A"
    assert re.search(r"SetPacketSize\(0x011A,\s*15\)", network, re.I)
    assert re.search(r"skillNoDamageNotify\s*=\s*0x011A", profile, re.I)
    assert "router.Register(0x011A, HandleSkillNoDamageNotify)" in current_text or (
        "RegisterHandlerIfValid(router, receiveProfile.skillNoDamageNotify, HandleSkillNoDamageNotify)" in current_text
    )
    current_reads = [
        r"ReadLE16\(packet\.data\s*\+\s*2\)",
        r"static_cast<int16_t>\(ReadLE16\(packet\.data\s*\+\s*4\)\)",
        r"ReadLE32\(packet\.data\s*\+\s*6\)",
        r"ReadLE32\(packet\.data\s*\+\s*10\)",
        r"packet\.data\[14\]",
    ]
    read_positions = []
    for pattern in current_reads:
        match = re.search(pattern, current_receiver)
        assert match, f"current receiver field read missing: {pattern}"
        read_positions.append(match.start())
    assert read_positions == sorted(read_positions), "current packet read order changed"
    assert "DeliverUseSkill(mode, srcGid, dstGid, skillId, rawLevel, result != 0)" in current_receiver

    # Ref dispatch associates protocol packet id 0x011A with case 282.
    ref_dispatch = client_ref_path.read_text(encoding="utf-8", errors="replace")
    assert re.search(r"case\s+282\s*:\s*CGameMode::Zc_Use_Skill\(this,\s*buf\)", code(ref_dispatch))
    print("clif_skill_nodamage gate passed: all six wire fields, disguise send branches, packet length/opcode, and Ref/EXE/current receiver route match.")


if __name__ == "__main__":
    main()
