from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]
exe_c = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8")
ref_packet = (ROOT / "Ref/GameModePacket.cpp").read_text(encoding="utf-8")
ref_mode = (ROOT / "Ref/GameMode2.cpp").read_text(encoding="utf-8")
impl_packet = (ROOT / "src/gamemode/GameModePacket.cpp").read_text(encoding="utf-8")
impl_mode = (ROOT / "src/gamemode/GameMode.cpp").read_text(encoding="utf-8")
packet_db = (ROOT / "Ref/RunningServer/packet_db.txt").read_text(encoding="utf-8", errors="replace")
binary = (ROOT / "HighPriest.exe").read_bytes()


def body_between(text: str, start: str, end: str) -> str:
    start_at = text.index(start)
    end_at = text.index(end, start_at + len(start))
    return text[start_at:end_at]


def function_body(text: str, signature: str) -> str:
    start = text.index("\n" + signature) + 1
    brace = text.index("{", start)
    end = text.index("\n}", brace) + 2
    return text[start:end]


exe_function = function_body(
    body_between(exe_c, "//----- (005A10D0)", "//----- (005A1120)"),
    "void __thiscall CGameMode::Zc_show_image(CGameMode *this, const char *buf)",
)
ref_function = function_body(
    body_between(
        ref_packet,
        "// Original Signature: public: void __thiscall CGameMode::Zc_show_image",
        "// Original Signature: public: void __thiscall CGameMode::Zc_show_image2",
    ),
    "void __thiscall CGameMode::Zc_show_image(CGameMode *this, const char *buf)",
)
assert re.sub(r"\s+", "", exe_function) == re.sub(r"\s+", "", ref_function)
for source_branch in (
    "strncpy(imageName, buf + 2, 0x10u)",
    "v3 = *((unsigned __int8 *)buf + 18)",
    "if ( v3 == 255 )",
    "this->CMode::SendMsg(this, 113, (int)imageName, v3, 0)",
):
    assert source_branch in exe_function, source_branch
assert re.search(r"^0x0145,19\s*$", packet_db, re.M | re.I)

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


packet_code = binary[file_offset_for_va(0x5A10D0):file_offset_for_va(0x5A111A)]
expected_packet_code = bytes.fromhex(
    "55 8B EC 83 EC 14 56 57 8B 7D 08 8B F1 6A 10 8D 4D EC "
    "8D 47 02 50 51 E8 D4 7A 10 00 33 C0 83 C4 0C 8A 47 12 "
    "C6 45 FC 00 3D FF 00 00 00 75 03 83 C8 FF 8B 16 6A 00 "
    "50 8D 45 EC 50 6A 71 8B CE FF 52 14 5F 5E 8B E5 5D C2 04 00"
)
assert packet_code == expected_packet_code, packet_code.hex(" ")

# DrawIllustImage exists in the image but is never reached from the canonical
# OnRun render loop: prove both source-level call absence and executable xrefs.
exe_on_run = body_between(exe_c, "//----- (0056B6F0)", "//----- (0056B8F0)")
assert "DrawIllustImage" not in exe_on_run
assert exe_c.count("CGameMode::DrawIllustImage(") == 2  # declaration + definition; no caller
draw_illust_va = 0x56C740
draw_illust_pointer = draw_illust_va.to_bytes(4, "little")
assert binary.count(draw_illust_pointer) == 0, "unexpected absolute pointer/reference to DrawIllustImage"
text_start = 0x1000
text_end = 0x2CC000
relative_callers = []
for offset in range(text_start, text_end - 4):
    if binary[offset] == 0xE8:
        destination = 0x400000 + offset + 5 + int.from_bytes(binary[offset + 1:offset + 5], "little", signed=True)
        if destination == draw_illust_va:
            relative_callers.append(0x400000 + offset)
assert not relative_callers, [hex(address) for address in relative_callers]

send_mode = body_between(ref_mode, "ENUM_SKILL_USE_TYPE __thiscall CGameMode::SendMsg(", "// ================================================\n// Original Signature:")
send_case_start = send_mode.index("case 0x71u:")
send_case = send_mode[send_case_start:send_mode.index("case 0x72u:", send_case_start)]
exe_send_start = exe_c.index("//----- (005833A0)")
exe_send_end = exe_c.index("//----- (", exe_send_start + len("//----- (005833A0)"))
exe_send_mode = exe_c[exe_send_start:exe_send_end]
exe_case_start = exe_send_mode.index("case 0x71u:")
exe_case = exe_send_mode[exe_case_start:exe_send_mode.index("case 0x72u:", exe_case_start)]
for source_branch in (
    "this->m_showImageInfo.type = HIDWORD(val1)",
    "std::string::assign(p_imageName",
    "std::string::append(p_imageName, aBmp_2",
    "return SUT_NOSKILL",
):
    assert source_branch in exe_case and source_branch in send_case, source_branch

current_packet = body_between(impl_packet, "void HandleShowImage(CGameMode& mode, const PacketView& packet)", "void DecodePosDir(")
assert "packet.packetLength < 19" in current_packet
assert "std::memcpy(imageName, packet.data + 2, 16)" in current_packet
assert "int imageType = packet.data[18]" in current_packet
assert "imageType = -1" in current_packet
assert "mode.SendMsg(113" in current_packet
assert "router.Register(0x0145, HandleShowImage);" in impl_packet

current_state = body_between(impl_mode, "void UpdateShowImageInfo(CGameMode& mode, const char* imageName, int type)", "msgresult_t CGameMode::SendMsg(")
current_send = body_between(impl_mode, "msgresult_t CGameMode::SendMsg(", "void CGameMode::")
assert "mode.m_showImageInfo.type = type" in current_state
assert "mode.m_showImageInfo.name = imageName" in current_state
assert "name.find(\".bmp\")" in current_state and "name.find(\".BMP\")" in current_state
assert "mode.m_showImageInfo.name += \".bmp\"" in current_state
assert "case 113:" in current_send and "UpdateShowImageInfo(*this" in current_send

print("0x0145 Zc_show_image and message-113 state branch comparison passed")
print("Canonical PE bytes at 0x5A10D0 confirm the 16-byte name, type byte, 255-to-minus-one mapping, and message 113 dispatch.")
print("EXE/Ref case 0x71 stores the image name/type and normalizes the BMP suffix; current source maps those effects.")
print("DrawIllustImage at VA 0x56C740 has no EXE OnRun call or direct code/data xref; Ref/dhxj's invocation is not part of target-executable behavior.")
