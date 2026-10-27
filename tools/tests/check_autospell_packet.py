from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]
binary = (ROOT / "HighPriest.exe").read_bytes()
exe_c = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8")
ref = (ROOT / "Ref/GameModePacket.cpp").read_text(encoding="utf-8")
packet_db = (ROOT / "Ref/RunningServer/packet_db.txt").read_text(encoding="utf-8", errors="replace")
implementation = (ROOT / "src/gamemode/GameModePacket.cpp").read_text(encoding="utf-8")


def function_body(text: str, signature: str) -> str:
    start = text.index(signature)
    brace = text.index("{", start)
    depth = 0
    for index in range(brace, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[start:index + 1]
    raise AssertionError(f"unterminated function: {signature}")


def compact(text: str) -> str:
    return re.sub(r"\s+", "", text)


exe_signature = "void __thiscall CGameMode::Zc_AutoSpellList(CGameMode *this, const char *buf)"
ref_signature = "void __thiscall CGameMode::Zc_AutoSpellList(CGameMode *this, const char *buf)"
exe_start = exe_c.index("//----- (005A7DC0)")
exe_end = exe_c.index("//----- (005A7E30)", exe_start)
exe_body = function_body(exe_c[exe_start:exe_end], exe_signature)
ref_start = ref.index("// Original Signature: public: void __thiscall CGameMode::Zc_AutoSpellList")
ref_end = ref.index("// Original Signature:", ref_start + 80)
ref_body = function_body(ref[ref_start:ref_end], ref_signature)
assert compact(exe_body) == compact(ref_body)

branches = (
    "v3 = buf + 2",
    "v4 = 7",
    "if ( *(_DWORD *)v5 )",
    "v5 += 4",
    "if ( v2 > 0 )",
    "WID_SPELLLISTWND",
    "if ( Window )",
    "if ( *(_DWORD *)v3 )",
    "SendMsg(Window, 0, 31, *(_DWORD *)v3, 0, 0, 0)",
    "v3 += 4",
)
positions = [exe_body.index(branch) for branch in branches]
assert positions == sorted(positions)

assert re.search(r"^0x01cd,30\s*$", packet_db, re.M | re.I)
dispatcher = exe_c[exe_c.index("case 456:", exe_c.index("switch ( CRagConnection::GetPacketType")):]
assert re.search(r"case 461:\s*CGameMode::Zc_AutoSpellList\(this, buf\);", dispatcher)

# Compare the complete machine-code body, excluding alignment NOPs.
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


expected = bytes.fromhex(
    "55 8B EC 8B 45 08 56 57 32 D2 8D 70 02 B9 07 00 00 00 8B C6 83 38 00 74 02 FE C2 83 C0 04 49 75 F3 "
    "84 D2 7E 3A 6A 63 B9 58 87 76 00 E8 6F A4 F3 FF 8B F8 85 FF 74 28 53 BB 07 00 00 00 8B 06 85 C0 74 15 "
    "8B 17 6A 00 6A 00 6A 00 50 6A 1F 6A 00 8B CF FF 92 94 00 00 00 83 C6 04 4B 75 DF 5B 5F 5E 5D C2 04 00"
)
actual = binary[offset_for_va(0x5A7DC0):offset_for_va(0x5A7DC0) + len(expected)]
assert actual == expected, f"PE handler bytes differ: {actual.hex(' ')}"

implementation_body = function_body(implementation, "void HandleAutoSpellList(")
impl_branches = (
    "packet.packetLength < 30",
    "index < 7",
    "packet.data + 2 + index * 4",
    "if (!hasAnySpell)",
    "WID_SPELLLISTWND",
    "if (!window)",
    "window->SendMsg(nullptr, 31",
)
impl_positions = [implementation_body.index(branch) for branch in impl_branches]
assert impl_positions == sorted(impl_positions)

print("0x01CD PE bytes, EXE/Ref branches, dispatch, packet length, and current handler mapping agree")
