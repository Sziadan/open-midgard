from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]
exe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8")
ref = (ROOT / "Ref/GameModePacket.cpp").read_text(encoding="utf-8")
impl = (ROOT / "src/gamemode/GameModePacket.cpp").read_text(encoding="utf-8")
packet_db = (ROOT / "Ref/RunningServer/packet_db.txt").read_text(encoding="utf-8", errors="replace")
binary = (ROOT / "HighPriest.exe").read_bytes()


def body_between(text: str, start: str, end: str) -> str:
    start_at = text.index(start)
    end_at = text.index(end, start_at + len(start))
    return text[start_at:end_at]


exe_body = body_between(exe, "//----- (005A4400)", "//----- (005A4440)")
ref_body = body_between(
    ref,
    "// Original Signature: public: void __thiscall CGameMode::Zc_talkbox_chat_contents",
    "// Original Signature: public: void __thiscall CGameMode::Zc_update_charstat",
)
impl_body = body_between(
    impl,
    "void HandleTalkboxChatContents(CGameMode& mode, const PacketView& packet)",
    "void DecodePosDir(",
)
compact = lambda value: re.sub(r"\s+", "", value)
exe_function = exe_body[exe_body.index("void __thiscall"):]
exe_function = exe_function[:exe_function.index("\n}") + 2]
ref_function = ref_body[ref_body.rindex("\nvoid __thiscall") + 1:]
ref_function = ref_function[:ref_function.index("\n}") + 2]
assert compact(exe_function) == compact(ref_function)
assert "CWorld::GetSkillByAID(this->m_world, *(_DWORD *)(buf + 2))" in exe_body
assert "SkillByAID->CGameObject::SendMsg(SkillByAID, 0, 7, (int)(buf + 6), 0, 0)" in exe_body
exe_dispatch = exe[exe.index("case 401:"):exe.index("case 402:", exe.index("case 401:"))]
assert "case 401:" in exe_dispatch and "CWorld::GetSkillByAID" in exe_dispatch and "SendMsg(SkillByAID, 0, 7" in exe_dispatch
assert "case 401:" in ref and "CWorld::GetSkillByAID(this->m_world, *(unsigned int *)GID)" in ref[ref.index("case 401:"):ref.index("case 401:") + 320]
assert re.search(r"^0x0191,86\s*$", packet_db, re.M)

assert "packet.packetLength < 86" in impl_body
assert "ResolveSkillPacketUnit(mode, ReadLE32(packet.data + 2))" in impl_body
assert "skill->SendMsg(skill, 7, reinterpret_cast<msgparam_t>(packet.data + 6), 0, 0)" in impl_body
assert "router.Register(0x0191, HandleTalkboxChatContents);" in impl

pe_offset = int.from_bytes(binary[0x3C:0x40], "little")
section_count = int.from_bytes(binary[pe_offset + 6:pe_offset + 8], "little")
optional_header = pe_offset + 24
image_base = int.from_bytes(binary[optional_header + 28:optional_header + 32], "little")
section_table = optional_header + int.from_bytes(binary[pe_offset + 20:pe_offset + 22], "little")


def file_offset_for_va(va: int) -> int:
    for index in range(section_count):
        section = section_table + index * 40
        virtual_size = int.from_bytes(binary[section + 8:section + 12], "little")
        virtual_address = int.from_bytes(binary[section + 12:section + 16], "little")
        raw_size = int.from_bytes(binary[section + 16:section + 20], "little")
        raw_offset = int.from_bytes(binary[section + 20:section + 24], "little")
        if image_base + virtual_address <= va < image_base + virtual_address + max(virtual_size, raw_size):
            return raw_offset + va - image_base - virtual_address
    raise AssertionError(f"VA {va:#x} is outside executable sections")


function_va = 0x5A4400
function_code = binary[file_offset_for_va(function_va):file_offset_for_va(function_va + 0x32)]
expected_code = bytes.fromhex(
    "55 8B EC 8B 89 B4 00 00 00 56 8B 75 08 8B 46 02 50 "
    "E8 8A 7E F5 FF 85 C0 74 13 8B 10 6A 00 83 C6 06 6A 00 "
    "56 6A 07 6A 00 8B C8 FF 52 08 5E 5D C2 04 00"
)
assert function_code == expected_code, function_code.hex(" ")
call_va = 0x5A4411
rel32 = int.from_bytes(function_code[call_va - function_va + 1:call_va - function_va + 5], "little", signed=True)
assert call_va + 5 + rel32 == 0x4FC2A0

print("0x0191 Zc_talkbox_chat_contents source-to-implementation comparison passed")
print("Canonical PE bytes at VA 0x5A4400 confirm AID@+2, resolver 0x4FC2A0, message 7, payload@+6, null extras, and the missing-skill return branch.")
print("EXE and Ref function bodies match; dispatch case 401 and fixed packet length 86 are confirmed.")
