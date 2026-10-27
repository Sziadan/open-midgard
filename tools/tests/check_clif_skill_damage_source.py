#!/usr/bin/env python3
"""Compare eAthena damage-skill encodings with Ref, EXE, and the client decoder."""

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


def ordered_matches(source: str, patterns: list[str], message: str) -> None:
    positions = []
    for pattern in patterns:
        match = re.search(pattern, source)
        assert match, f"{message}: missing {pattern}"
        positions.append(match.start())
    assert positions == sorted(positions), f"{message}: source order differs"


def main() -> None:
    server = code(body(ROOT / "Ref/eAthena_src_2011/map/clif.c", "int clif_skill_damage("))
    ref_path = ROOT / "Ref/GameModePacket.cpp"
    ref_basic = code(body(ref_path, "void __thiscall CGameMode::Zc_Notify_Skill("))
    exe_basic = code(body(ROOT / "HighPriest.exe.c", "void __thiscall CGameMode::Zc_Notify_Skill("))
    ref_extended = code(body(ref_path, "void __thiscall CGameMode::Zc_Notify_Skill2("))
    exe_extended = code(body(ROOT / "HighPriest.exe.c", "void __thiscall CGameMode::Zc_Notify_Skill2("))
    current = code(body(ROOT / "src/gamemode/GameModePacket.cpp", "void HandleSkillDamageNotify("))
    profile = (ROOT / "src/network/MapSendProfile.cpp").read_text(encoding="utf-8")
    sizes = (ROOT / "src/network/GronPacket.cpp").read_text(encoding="utf-8")
    db = (ROOT / "Ref/RunningServer/packet_db.txt").read_text(encoding="utf-8")

    assert ref_basic == exe_basic, "Ref/EXE Zc_Notify_Skill bodies differ"
    assert ref_extended == exe_extended, "Ref/EXE Zc_Notify_Skill2 bodies differ"
    assert hashlib.sha256(ref_basic.encode()).hexdigest() == "b09bfee40bc85baf9ae618465c3d7864d77e4b805fbbe78f13a8c85115429842"
    assert hashlib.sha256(ref_extended.encode()).hexdigest() == "5a8ee7f872af44bad8c77d99bb7293a20224c4d4675622970460bcb4be2cfa60"

    assert "#if PACKETVER < 3" in server and "#else" in server
    for pattern in [r"WBUFW\(buf,\s*0\)\s*=\s*0x114", r"WBUFW\(buf,\s*0\)\s*=\s*0x1de"]:
        assert re.search(pattern, server), f"server opcode branch missing: {pattern}"
    old_fields = [
        r"WBUFW\(buf,\s*2\)\s*=\s*skill_id", r"WBUFL\(buf,\s*4\)\s*=\s*src->id",
        r"WBUFL\(buf,\s*8\)\s*=\s*dst->id", r"WBUFL\(buf,\s*12\)\s*=\s*tick",
        r"WBUFL\(buf,\s*16\)\s*=\s*sdelay", r"WBUFL\(buf,\s*20\)\s*=\s*ddelay",
        r"WBUFW\(buf,\s*24\)\s*=\s*damage", r"WBUFW\(buf,\s*26\)\s*=\s*skill_lv",
        r"WBUFW\(buf,\s*28\)\s*=\s*div", r"WBUFB\(buf,\s*30\)\s*=\s*type",
    ]
    new_fields = [
        r"WBUFW\(buf,\s*2\)\s*=\s*skill_id", r"WBUFL\(buf,\s*4\)\s*=\s*src->id",
        r"WBUFL\(buf,\s*8\)\s*=\s*dst->id", r"WBUFL\(buf,\s*12\)\s*=\s*tick",
        r"WBUFL\(buf,\s*16\)\s*=\s*sdelay", r"WBUFL\(buf,\s*20\)\s*=\s*ddelay",
        r"WBUFL\(buf,\s*24\)\s*=\s*damage", r"WBUFW\(buf,\s*28\)\s*=\s*skill_lv",
        r"WBUFW\(buf,\s*30\)\s*=\s*div", r"WBUFB\(buf,\s*32\)\s*=\s*type",
    ]
    ordered_matches(server, old_fields, "basic packet fields")
    ordered_matches(server, new_fields, "extended packet fields")
    for pattern in [r"0x0114,31", r"0x01de,33"]:
        assert re.search(rf"^\s*{pattern}\s*$", db, re.M | re.I), f"packet_db entry missing: {pattern}"
    for pattern in [r"SetPacketSize\(0x0114,\s*31\)", r"SetPacketSize\(0x01DE,\s*33\)"]:
        assert re.search(pattern, sizes, re.I), f"current packet length missing: {pattern}"
    assert re.search(r"skillDamageNotifyBasic\s*=\s*0x0114", profile)
    assert re.search(r"skillDamageNotifyExtended\s*=\s*0x01DE", profile)

    current_fields = [
        r"ReadLE16\(packet\.data\s*\+\s*2\)", r"ReadLE32\(packet\.data\s*\+\s*4\)",
        r"ReadLE32\(packet\.data\s*\+\s*8\)", r"ReadLE32\(packet\.data\s*\+\s*16\)",
        r"ReadLE32\(packet\.data\s*\+\s*20\)", r"isLongDamage\s*\?\s*static_cast<int>\(ReadLE32\(packet\.data\s*\+\s*24\)\)",
        r"static_cast<int16_t>\(ReadLE16\(packet\.data\s*\+\s*24\)\)",
        r"static_cast<int16_t>\(ReadLE16\(packet\.data\s*\+\s*\(isLongDamage\s*\?\s*28\s*:\s*26\)\)\)",
        r"static_cast<int16_t>\(ReadLE16\(packet\.data\s*\+\s*\(isLongDamage\s*\?\s*30\s*:\s*28\)\)\)",
        r"packet\.data\[isLongDamage\s*\?\s*32\s*:\s*30\]",
    ]
    ordered_matches(current, current_fields, "current receive field decoders")
    assert "DeliverSkillUse" in current and "args.level = level;" in current and "args.count = div;" in current
    print("clif_skill_damage gate passed: both packet layouts, offsets, lengths, Ref/EXE receiver bodies, signed level/count decoding, and current profile routes match.")


if __name__ == "__main__":
    main()
