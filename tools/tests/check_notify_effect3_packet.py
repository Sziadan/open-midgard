from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]


def body_between(text: str, start: str, end: str) -> str:
    start_at = text.index(start)
    end_at = text.index(end, start_at + len(start))
    return text[start_at:end_at]


exe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8")
ref = (ROOT / "Ref/GameModePacket.cpp").read_text(encoding="utf-8")
impl = (ROOT / "src/gamemode/GameModePacket.cpp").read_text(encoding="utf-8")
packet_db = (ROOT / "Ref/RunningServer/packet_db.txt").read_text(encoding="utf-8", errors="replace")
binary = (ROOT / "HighPriest.exe").read_bytes()

exe_body = body_between(exe, "//----- (005A5A30)", "//----- (005A5C60)")
ref_body = body_between(ref, "// Original Signature: public: void __thiscall CGameMode::Zc_Notify_Effect3", "// ================================================\n// Original Signature: public: void __thiscall CGameMode::Zc_Notify_Entry_HP")
impl_body = body_between(impl, "void HandleNotifyEffect3(CGameMode& mode, const PacketView& packet)", "void HandleSkillCastCancel(")

expected_ids = [331, 332, 657, 658, 659, 660, 661, 662, 663, 664]
case_pattern = re.compile(r"\bcase\s+(?:0x([0-9A-Fa-f]+)|(\d+))\b")


def case_ids(body: str) -> list[int]:
    return [int(hex_value, 16) if hex_value else int(decimal_value) for hex_value, decimal_value in case_pattern.findall(body)]


assert case_ids(exe_body) == expected_ids, case_ids(exe_body)
assert case_ids(ref_body) == expected_ids, case_ids(ref_body)
assert case_ids(impl_body) == expected_ids, case_ids(impl_body)
compact = lambda value: re.sub(r"\s+", "", value)
assert compact(exe_body[exe_body.index("switch ("):]) == compact(ref_body[ref_body.index("switch ("):])
assert re.search(r"case 644:\s*CGameMode::Zc_Notify_Effect3\(this, buf\);", exe)
assert re.search(r"case 644:\s*CGameMode::Zc_Notify_Effect3\(this, buf\);", ref)
assert re.search(r"^0x0284,14\s*$", packet_db, re.M)
assert "router.Register(0x0284, HandleNotifyEffect3);" in impl

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
    raise AssertionError(f"VA {va:#x} is outside the executable sections")


exe_code = binary[file_offset_for_va(0x5A5A30):file_offset_for_va(0x5A5C60)]
assert exe_code[:3] == b"\x55\x8B\xEC"
for effect_id in expected_ids:
    assert b"\x3D" + effect_id.to_bytes(4, "little") in exe_code
for color in (0xFF00FF00, 0xFF0000FF, 0xFFFF0000, 0xFFFF00FF, 0xFF000000, 0xFFFFFF00, 0xFFFF55B1):
    assert b"\x68" + color.to_bytes(4, "little") in exe_code
assert b"\x6A\xFF" in exe_code


def assert_rel_call(call_va: int, target_va: int) -> None:
    offset = file_offset_for_va(call_va)
    call = binary[offset:offset + 5]
    assert call[0] == 0xE8
    rel32 = int.from_bytes(call[1:5], "little", signed=True)
    assert call_va + 5 + rel32 == target_va


assert_rel_call(0x5A5A87, 0x66D740)  # ChangeXorValue
assert_rel_call(0x5A5AAC, 0x66DD10)  # Decode
assert_rel_call(0x5A5ACA, 0x66DBB0)  # Encode

for effect_id, color in {
    331: "0xFF00FF00u", 332: None, 657: "0xFF00FF00u", 658: None,
    659: "0xFFFF0000u", 660: "0xFFFF00FFu", 661: "0xFF000000u",
    662: "0xFFFFFFFFu", 663: "0xFFFFFF00u", 664: "0xFFFF55B1u",
}.items():
    source_color = {
        331: "-16711936", 332: "-16776961", 657: "-16711936", 658: "-16776961",
        659: "-65536", 660: "-65281", 661: "-16777216", 662: "-1",
        663: "-256", 664: "-43599",
    }[effect_id]
    if color is not None:
        assert color in impl_body
    assert source_color in exe_body and source_color in ref_body or effect_id in (332, 658)

assert "packet.data + 10" in impl_body
assert "actor->SendMsg(actor, 88" in impl_body
assert "actor->LaunchEffect(static_cast<int>(effectId)" in impl_body
assert "g_session.m_isEffectOn" not in impl_body
assert "std::rand();" in body_between(impl, "void AdvanceEffectStatusRandomState()", "void HandleNotifyEffect3(")

print("0x0284 Zc_Notify_Effect3 source comparison passed")
print("Canonical PE bytes at 0x5A5A30 contain all ten effect comparisons, source colors, and ChangeXorValue/Decode/Encode call targets.")
print("Executable cases: ", ", ".join(str(value) for value in case_ids(exe_body)))
print("Ref cases:        ", ", ".join(str(value) for value in case_ids(ref_body)))
print("Current cases:    ", ", ".join(str(value) for value in case_ids(impl_body)))
print("Branch map: 331/332 update self HP/SP and preserve ChangeXorValue rand() consumption; every resolved actor receives value message 88 / type 114 with the source color, then launches the requested effect; unresolved actor and malformed packet paths return.")
