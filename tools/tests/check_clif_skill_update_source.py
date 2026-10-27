#!/usr/bin/env python3
"""Compare skill-unit reveal packet handling and its trap-model branch."""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TRAP_JOBS = [143, 144, 145, 147, 148, 149, 150, 151, 152, 153]
TRAP_MODELS = [
    "kTrap03_3", "kTrap02", "kTrap01", "kTrap03", "kTrap03_6",
    "kTrap03_4", "kTrap03_5", "kTrap03_2", "kTrap04", "kTrap05",
]


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


def case_ids(source: str) -> list[int]:
    return [int(value) for value in re.findall(r"\bcase\s+(\d+)\s*:", source)]


def main() -> None:
    encoder = code(body(ROOT / "Ref/eAthena_src_2011/map/clif.c", "void clif_skillunit_update("))
    assert re.search(r"WBUFW\(buf,\s*0\)\s*=\s*0x1ac", encoder)
    assert re.search(r"WBUFL\(buf,\s*2\)\s*=\s*bl->id", encoder)
    assert re.search(r"clif_send\(buf,\s*packet_len\(0x1ac\),\s*bl,\s*AREA\)", encoder)
    packet_db = (ROOT / "Ref/RunningServer/packet_db.txt").read_text(encoding="utf-8")
    sizes = (ROOT / "src/network/GronPacket.cpp").read_text(encoding="utf-8")
    assert re.search(r"^\s*0x01ac,6\s*$", packet_db, re.M | re.I)
    assert re.search(r"SetPacketSize\(0x01AC,\s*6\)", sizes, re.I)

    ref_packet = code(body(ROOT / "Ref/GameModePacket.cpp", "void __thiscall CGameMode::Zc_Skill_Update("))
    exe_packet = code(body(ROOT / "HighPriest.exe.c", "void __thiscall CGameMode::Zc_Skill_Update("))
    assert ref_packet == exe_packet, "Ref/EXE Zc_Skill_Update bodies differ"
    assert "*(_DWORD *)(buf + 2)" in ref_packet
    assert "GetSkillByAID" in ref_packet and "GetJob" in ref_packet
    assert "> 138" in ref_packet and "<= 153" in ref_packet and ", 119," in ref_packet
    assert re.search(r"case\s+428\s*:\s*\n\s*CGameMode::Zc_Skill_Update", (ROOT / "Ref/GameModePacket.cpp").read_text(encoding="utf-8"))
    assert re.search(r"case\s+428\s*:\s*\n\s*CGameMode::Zc_Skill_Update", (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8"))

    current_packet_file = ROOT / "src/gamemode/GameModePacket.cpp"
    current_source = current_packet_file.read_text(encoding="utf-8")
    current = code(body(current_packet_file, "void HandleSkillUpdate("))
    assert "Register(0x01AC, HandleSkillUpdate)" in current_source
    assert "packet.packetLength < 6" in current and "ReadLE32(packet.data + 2)" in current
    assert "skill->m_job > 138" in current and "skill->m_job <= 153" in current
    assert "skill->SendMsg(skill, 119" in current

    ref_skill = code(body(ROOT / "Ref/Skill.cpp", "char *__thiscall CSkill::GetTrapModelName("))
    exe_skill = code(body(ROOT / "HighPriest.exe.c", "char *__thiscall CSkill::GetTrapModelName("))
    assert ref_skill == exe_skill, "Ref/EXE trap-model lookup bodies differ"
    ref_jobs = case_ids(ref_skill)
    current_skill_file = ROOT / "src/world/SkillUnit.cpp"
    current_skill_source = current_skill_file.read_text(encoding="utf-8")
    current_models = body(current_skill_file, "const char* CSkill::TrapModelName() const")
    current_jobs = case_ids(code(current_models))
    assert ref_jobs == TRAP_JOBS, f"reference trap jobs changed: {ref_jobs}"
    assert current_jobs == ref_jobs, f"current trap job order differs: {current_jobs}"
    for job, model in zip(TRAP_JOBS, TRAP_MODELS, strict=True):
        assert re.search(rf"case\s+{job}\s*:\s*return\s+{model}\s*;", code(current_models)), f"job {job} maps to wrong model"

    ref_send = code(body(ROOT / "Ref/Skill.cpp", "void __thiscall CSkill::SendMsg("))
    exe_send = code(body(ROOT / "HighPriest.exe.c", "void __thiscall CSkill::SendMsg("))
    assert ref_send == exe_send, "Ref/EXE CSkill::SendMsg bodies differ"
    assert re.search(r"case\s+119\s*:\s*if\s*\(\s*!\s*this->m_isVisibleBody\s*\)\s*\{\s*this->m_isVisibleBody\s*=\s*1;\s*switch", ref_send)
    current_send = code(body(current_skill_file, "void CSkill::SendMsg("))
    assert re.search(r"case\s+119\s*:\s*if\s*\(m_isVisibleBody\s*==\s*0\)\s*\{\s*m_isVisibleBody\s*=\s*1;\s*ShowModel\(TrapModelName\(\)\);", current_send)
    print("0x01AC encoder, packet length, Ref/EXE receiver, current route, and all trap job/model branches verified")


if __name__ == "__main__":
    main()
