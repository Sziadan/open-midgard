from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]
exe_c = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8")
ref_packet = (ROOT / "Ref/GameModePacket.cpp").read_text(encoding="utf-8")
ref_actor = (ROOT / "Ref/GameActor.cpp").read_text(encoding="utf-8")
binary = (ROOT / "HighPriest.exe").read_bytes()
impl_packet = (ROOT / "src/gamemode/GameModePacket.cpp").read_text(encoding="utf-8")
impl_actor = (ROOT / "src/world/GameActor.cpp").read_text(encoding="utf-8")


def body_between(text: str, start: str, end: str) -> str:
    start_at = text.index(start)
    end_at = text.index(end, start_at + len(start))
    return text[start_at:end_at]


def function_body(text: str, function: str) -> str:
    start = text.index("\nvoid __thiscall " + function) + 1
    brace = text.index("{", start)
    end = text.index("\n}", brace) + 2
    return text[start:end]


exe_handler = function_body(
    body_between(exe_c, "//----- (005A6AB0)", "//----- (005A6AF0)"),
    "CGameMode::Zc_ChangeGuild",
)
ref_handler = function_body(
    body_between(
        ref_packet,
        "// Original Signature: public: void __thiscall CGameMode::Zc_ChangeGuild",
        "// Original Signature: public: void __thiscall CGameMode::Zc_Change_Chatroom",
    ),
    "CGameMode::Zc_ChangeGuild",
)
assert re.sub(r"\s+", "", exe_handler) == re.sub(r"\s+", "", ref_handler)
assert "*(_DWORD *)(buf + 2)" in exe_handler
assert "*(_DWORD *)(buf + 6)" in exe_handler
assert "*((__int16 *)buf + 5)" in exe_handler
current_handler = body_between(impl_packet, "void HandleChangeGuild(CGameMode& mode, const PacketView& packet)", "void DecodePosDir(")
assert "packet.packetLength < 12" in current_handler
assert "ResolveSkillPacketActor(mode, ReadLE32(packet.data + 2))" in current_handler
assert "ReadLE32(packet.data + 6)" in current_handler
assert "static_cast<s16>(ReadLE16(packet.data + 10))" in current_handler
assert "router.Register(0x01B4, HandleChangeGuild);" in impl_packet

exe_setter = function_body(
    body_between(exe_c, "//----- (0055D450)", "//----- (0055D520)"),
    "CGameActor::SetGuildInfo",
)
ref_setter = function_body(
    body_between(
        ref_actor,
        "// Original Signature: public: virtual void __thiscall CGameActor::SetGuildInfo",
        "// Original Signature: public: virtual void __thiscall CGameActor::SetHeadPalette",
    ),
    "CGameActor::SetGuildInfo",
)
assert re.sub(r"\s+", "", exe_setter) == re.sub(r"\s+", "", ref_setter)
for source_branch in (
    "if ( gdid >= 1 && (emblemVersion >= 1 || CGameActor::GetJob(this) == 722) )",
    "CSession::IsPc(&g_session, Job)",
    "CSession::IsGuardian(&g_session, v5)",
    "this->m_gdid = gdid",
    "this->m_emblemVersion = emblemVersion",
    "CSession::GetEmblemFileName(&g_session, emblemResName, gdid, emblemVersion)",
    "CResMgr::IsExist(v6, emblemResName)",
):
    assert source_branch in exe_setter, source_branch
current_setter = body_between(impl_actor, "void CGameActor::SetGuildInfo(int guildId, int emblemVersion)", "void CGameActor::ClearChatBubbleText()")
for mapped_behavior in (
    "guildId < 1 || (emblemVersion < 1 && m_job != 722)",
    "m_isPc != 0",
    "m_job == 1285 || m_job == 1286 || m_job == 1287 || m_job == 722",
    "m_gdid = guildId",
    "m_emblemVersion = emblemVersion",
):
    assert mapped_behavior in current_setter, mapped_behavior

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


handler_code = binary[file_offset_for_va(0x5A6AB0):file_offset_for_va(0x5A6AE2)]
expected_handler = bytes.fromhex(
    "55 8B EC 8B 89 B4 00 00 00 56 8B 75 08 8B 46 02 50 "
    "E8 BA 56 F5 FF 85 C0 74 13 0F BF 4E 0A 8B 10 51 "
    "8B 4E 06 51 8B C8 FF 92 98 00 00 00 5E 5D C2 04 00"
)
assert handler_code == expected_handler, handler_code.hex(" ")
assert 0x5A6AB0 + 0x16 + int.from_bytes(handler_code[18:22], "little", signed=True) == 0x4FC180

setter_code = binary[file_offset_for_va(0x55D450):file_offset_for_va(0x55D514)]
assert setter_code.startswith(bytes.fromhex("55 8B EC 81 EC 00 01 00 00"))
for encoded in (
    "83 FB 01", "83 FF 01", "3D D2 02 00 00",  # gates: guild id, version, job 722
    "89 9E BC 02 00 00", "89 BE C0 02 00 00",  # state writes
    "E8 35 0D 11 00", "E8 09 48 FC FF", "E8 02 52 FC FF",  # emblem name/existence
    "68 8B 00 00 00", "FF 52 14",  # missing-resource message 139
):
    assert bytes.fromhex(encoded) in setter_code, encoded

dispatch_start = exe_c.index("case 436:")
dispatch = exe_c[dispatch_start:dispatch_start + 240]
assert "CGameMode::Zc_ChangeGuild(this, buf);" in dispatch
assert re.search(r"^0x01b4,12\s*$", (ROOT / "Ref/RunningServer/packet_db.txt").read_text(encoding="utf-8", errors="replace"), re.M | re.I)
print("0x01B4 Zc_ChangeGuild and CGameActor::SetGuildInfo source audit passed")
print("EXE and Ref bodies match; PE bytes confirm packet offsets, actor lookup, setter gates, state writes, emblem lookup, and missing-resource message 139.")
print("Current route/state comparison passes; current client does not yet implement the source emblem-file lookup and missing-resource message 139 pipeline.")
