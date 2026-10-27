from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]
exe_c = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8")
ref_packet = (ROOT / "Ref/GameModePacket.cpp").read_text(encoding="utf-8")
impl_packet = (ROOT / "src/gamemode/GameModePacket.cpp").read_text(encoding="utf-8")
impl_session = (ROOT / "src/session/Session.cpp").read_text(encoding="utf-8")
packet_db = (ROOT / "Ref/RunningServer/packet_db.txt").read_text(encoding="utf-8", errors="replace")
binary = (ROOT / "HighPriest.exe").read_bytes()


def body_between(text: str, start: str, end: str) -> str:
    first = text.index(start)
    last = text.index(end, first + len(start))
    return text[first:last]


def function_body(text: str, signature: str) -> str:
    start = text.index("\n" + signature) + 1
    brace = text.index("{", start)
    end = text.index("\n}", brace) + 2
    return text[start:end]


exe_handler = function_body(
    body_between(exe_c, "//----- (005A75B0)", "//----- (005A7710)"),
    "void __thiscall CGameMode::Zc_GuildSkillInfo(CGameMode *this, const char *buf)",
)
ref_handler = function_body(
    body_between(
        ref_packet,
        "// Original Signature: public: void __thiscall CGameMode::Zc_GuildSkillInfo",
        "// Original Signature: public: void __thiscall CGameMode::Zc_GuildZenyAck",
    ),
    "void __thiscall CGameMode::Zc_GuildSkillInfo(CGameMode *this, const char *buf)",
)
assert re.sub(r"\s+", "", exe_handler) == re.sub(r"\s+", "", ref_handler)
for branch in (
    "this->m_isReqUpgradeSkillLevel = 0",
    "std::list<SKILL_INFO>::erase(&g_session.m_guildSkillList",
    "(*((__int16 *)buf + 1) - 6) / 0x25u",
    "v5 = buf + 14",
    "v5 += 37",
    "skillInfo.SKID >= 10000 && skillInfo.SKID < 10016",
    "g_session.m_guildSkillPoint = *((__int16 *)v4 + 2)",
    "CSession::InvalidateUI(&g_session)",
    "if ( g_windowMgr.m_guildSkillWnd )",
    "SendMsg(g_windowMgr.m_guildSkillWnd, 0, 23",
):
    assert branch in exe_handler, branch
assert re.search(r"^0x0162,-1\s*$", packet_db, re.M | re.I)
dispatch_start = exe_c.index("switch ( CRagConnection::GetPacketType(v5, buf) )")
dispatch_body = exe_c[dispatch_start:dispatch_start + 200000]
assert re.search(r"case 354:\s*CGameMode::Zc_GuildSkillInfo\(this, buf\);", dispatch_body)

# Compare the complete function bytes from the canonical PE image.
pe = int.from_bytes(binary[0x3C:0x40], "little")
section_count = int.from_bytes(binary[pe + 6:pe + 8], "little")
optional = pe + 24
image_base = int.from_bytes(binary[optional + 28:optional + 32], "little")
section_table = optional + int.from_bytes(binary[pe + 20:pe + 22], "little")


def offset_for_va(va: int) -> int:
    for index in range(section_count):
        section = section_table + index * 40
        virtual_size = int.from_bytes(binary[section + 8:section + 12], "little")
        virtual_address = int.from_bytes(binary[section + 12:section + 16], "little")
        raw_size = int.from_bytes(binary[section + 16:section + 20], "little")
        raw_offset = int.from_bytes(binary[section + 20:section + 24], "little")
        if image_base + virtual_address <= va < image_base + virtual_address + max(virtual_size, raw_size):
            return raw_offset + va - image_base - virtual_address
    raise AssertionError(f"VA {va:#x} is outside executable sections")


expected_handler = bytes.fromhex(
    "55 8B EC 6A FF 68 68 83 6C 00 64 A1 00 00 00 00 50 64 89 25 00 00 00 00 "
    "83 EC 44 56 57 33 FF 89 B9 F8 00 00 00 8B 35 28 26 7B 00 8B 06 3B C6 "
    "89 45 F0 74 23 8D 45 EC 57 50 8D 4D F0 E8 8F 25 01 00 8B 08 8D 55 E8 "
    "51 52 B9 24 26 7B 00 E8 3E C0 F0 FF 39 75 F0 75 DD 8B 75 08 B8 4D 91 "
    "CF BA 0F BF 4E 02 83 E9 06 F7 E1 2B CA D1 E9 03 CA C1 E9 05 3B CF 0F "
    "8E A3 00 00 00 53 8A 5D 0B 83 C6 0E 89 4D F0 C7 45 B0 01 00 00 00 89 "
    "7D CC 88 5D D8 89 7D DC 89 7D E0 89 7D E4 0F BF 46 FE 0F BF 16 33 C9 "
    "89 45 BC 66 8B 4E F8 8B 46 FA 89 4D B4 33 C9 8A 4E 1C 89 7D FC 89 4D "
    "C4 8D 4D B0 89 55 C0 89 45 B8 E8 18 DA F6 FF 89 45 CC 8B 45 B4 3D 10 "
    "27 00 00 7C 17 3D 20 27 00 00 7D 10 8D 55 B0 6A 01 52 B9 18 1F 7B 00 "
    "E8 04 14 0D 00 8B 45 DC C7 45 FC FF FF FF FF 50 E8 2C 1B 0F 00 8B 45 "
    "F0 83 C4 04 83 C6 25 48 89 7D DC 89 7D E0 89 7D E4 89 45 F0 0F 85 6B "
    "FF FF FF 8B 75 08 5B 0F BF 4E 04 89 0D 8C 32 7B 00 B9 18 1F 7B 00 E8 "
    "90 9D 0B 00 8B 0D E4 88 76 00 3B CF 74 0F 8B 11 57 57 57 57 6A 17 57 "
    "FF 92 94 00 00 00 8B 4D F4 5F 5E 64 89 0D 00 00 00 00 8B E5 5D C2 04 "
    "00 90 90 90 90 90"
)
actual_handler = binary[offset_for_va(0x5A75B0):offset_for_va(0x5A7710)]
assert actual_handler == expected_handler, actual_handler.hex(" ")

# Extract the name pointer store order from the EXE's InitGuildTable machine
# code, then read each NUL-terminated name from the executable itself.
init_start = offset_for_va(0x515F7A)
init_end = offset_for_va(0x51603E)
cursor = init_start
name_pointers = []
while cursor < init_end and len(name_pointers) < 16:
    if binary[cursor] == 0xC7:
        modrm = binary[cursor + 1]
        if modrm == 0x01:
            immediate = cursor + 2
            instruction_size = 6
        elif modrm in (0x40, 0x41, 0x42):
            immediate = cursor + 3
            instruction_size = 7
        else:
            raise AssertionError(f"unexpected initializer store {binary[cursor:cursor+7].hex(' ')}")
        pointer = int.from_bytes(binary[immediate:immediate + 4], "little")
        if 0x705C00 <= pointer <= 0x705DFF:
            name_pointers.append(pointer)
        cursor += instruction_size
    elif binary[cursor] == 0x8B and binary[cursor + 1] in (0x0D, 0x15):
        cursor += 6
    elif binary[cursor] == 0xA1:
        cursor += 5
    else:
        break
assert len(name_pointers) == 16, [hex(pointer) for pointer in name_pointers]
exe_names = []
for pointer in name_pointers:
    string_offset = offset_for_va(pointer)
    exe_names.append(binary[string_offset:binary.index(b"\0", string_offset)].decode("ascii"))
assert exe_names == [
    "GD_APPROVAL", "GD_KAFRACONTRACT", "GD_GUARDRESEARCH", "GD_GUARDUP",
    "GD_EXTENSION", "GD_GLORYGUILD", "GD_LEADERSHIP", "GD_GLORYWOUNDS",
    "GD_SOULCOLD", "GD_HAWKEYES", "GD_BATTLEORDER", "GD_REGENERATION",
    "GD_RESTORE", "GD_EMERGENCYCALL", "GD_DEVELOPMENT", "GD_ITEMEMERGENCYCALL",
]
source_names = re.search(r"kGuildSkillIdNames\[16\]\s*=\s*\{(.*?)\};", impl_packet, re.S).group(1)
assert re.findall(r'"([A-Z_]+)"', source_names) == exe_names

current_handler = body_between(
    impl_packet,
    "void HandleGuildSkillInfo(CGameMode& mode, const PacketView& packet)",
    "void HandleShowImage(",
)
for branch in (
    "mode.m_isReqUpgradeSkillLevel = 0",
    "g_session.ClearGuildSkillItems()",
    "(sourceLength - 6) / 37",
    "offset = 6",
    "offset += 37",
    "packet.data + offset",
    "offset + 36",
    "skillInfo.SKID >= 10000 && skillInfo.SKID < 10016",
    "g_session.SetGuildSkillItem(skillInfo)",
    "ReadLE16(packet.data + 4)",
):
    assert branch in current_handler, branch
assert "router.Register(0x0162, HandleGuildSkillInfo);" in impl_packet
for branch in (
    "m_guildSkillItems.clear()",
    "existing.SKID == skillInfo.SKID",
    "it->level = skillInfo.level",
    "it->spcost = skillInfo.spcost",
    "it->upgradable = skillInfo.upgradable",
):
    assert branch in impl_session, branch

print("0x0162 Zc_GuildSkillInfo executable/Ref function comparison passed.")
print("Canonical PE handler bytes at 0x5A75B0 match the audited range; packet-db length is variable (-1), dispatcher ID 354.")
print("Record formula, 37-byte stride, all field offsets, SKID filter, points write, and ordered EXE-derived guild skill names match current source.")
print("Guild-skill window invalidation/message 23 remains an explicit unported UI dependency.")
