#!/usr/bin/env python3
"""Compare 0x01B9 cast cancellation against Ref/EXE and current actor lookup."""

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
    ref_path = ROOT / "Ref/GameModePacket.cpp"
    exe_path = ROOT / "HighPriest.exe.c"
    ref = code(body(ref_path, "void __thiscall CGameMode::Zc_Dispel("))
    exe = code(body(exe_path, "void __thiscall CGameMode::Zc_Dispel("))
    assert ref == exe, "Ref/EXE Zc_Dispel bodies differ"
    assert "buf + 2" in ref and "g_session.m_aid" in ref
    ordered_ref = ["if ( *(_DWORD *)(buf + 2) == g_session.m_aid )",
                   "SendMsg(this->m_world->m_player, 0, 84",
                   "SendMsg(this->m_world->m_player, 0, 87",
                   "SendMsg(this->m_world->m_player, 0, 83",
                   "CWorld::GetGameActorByAID", "if ( GameActorByAID )",
                   "SendMsg(GameActorByAID, 0, 84", "SendMsg(v4, 0, 87", "SendMsg(v4, 0, 83"]
    ref_positions = [ref.find(fragment) for fragment in ordered_ref]
    assert all(position >= 0 for position in ref_positions)
    assert ref_positions == sorted(ref_positions), "Ref dispel branch/message order changed"

    server = code(body(ROOT / "Ref/eAthena_src_2011/map/clif.c", "void clif_skillcastcancel("))
    assert re.search(r"WBUFW\(buf,\s*0\)\s*=\s*0x1b9", server)
    assert re.search(r"WBUFL\(buf,\s*2\)\s*=\s*bl->id", server)
    assert re.search(r"packet_len\(0x1b9\).*AREA", server)
    db = (ROOT / "Ref/RunningServer/packet_db.txt").read_text(encoding="utf-8")
    sizes = (ROOT / "src/network/GronPacket.cpp").read_text(encoding="utf-8")
    assert re.search(r"^\s*0x01b9,6\s*$", db, re.M | re.I)
    assert re.search(r"SetPacketSize\(0x01B9,\s*6\)", sizes, re.I)
    for path in (ref_path, exe_path):
        assert re.search(r"case\s+441\s*:\s*\n\s*CGameMode::Zc_Dispel", path.read_text(encoding="utf-8", errors="replace"))

    current_path = ROOT / "src/gamemode/GameModePacket.cpp"
    source = current_path.read_text(encoding="utf-8")
    current = code(body(current_path, "void HandleSkillCastCancel("))
    assert "router.Register(0x01B9, HandleSkillCastCancel)" in source
    assert "packet.packetLength < 6" in current and "ReadLE32(packet.data + 2)" in current
    assert "ResolveSkillPacketActor(mode, actorId)" in current
    assert "m_runtimeActors" not in current
    assert "ResolveNotifyEffectActor" not in current and "EnsureRuntimeActor" not in current
    positions = [current.find(f"actor->SendMsg(actor, {message},") for message in (84, 87, 83)]
    assert all(position >= 0 for position in positions) and positions == sorted(positions)
    mismatch = (ROOT / "docs/skill/MISMATCHES.md").read_text(encoding="utf-8")
    assert "WP10.7 skill-packet actor registry" in mismatch
    print("0x01B9 encoder, Ref/EXE receiver, message order, and non-creating current lookup verified")


if __name__ == "__main__":
    main()
