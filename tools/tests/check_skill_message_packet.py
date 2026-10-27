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
binary = (ROOT / "HighPriest.exe").read_bytes()

exe_body = body_between(exe, "//----- (0059E660)", "//----- (0059E7A0)")
ref_body = body_between(ref, "// Original Signature: public: void __thiscall CGameMode::Zc_SKILLMSG", "// ================================================\n// Original Signature: public: void __thiscall CGameMode::Zc_Say_Dialog")
impl_body = body_between(impl, "void HandleSkillMessage(", "void HandleActorActionNotify(")

expected_ids = [0x15, 0x16, 0x17, 0x18, 0x19, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x28]
expected_symbols = [
    "MSI_GOSPEL1", "MSI_GOSPEL2", "MSI_GOSPEL3", "MSI_GOSPEL4", "MSI_GOSPEL5",
    "MSI_GOSPEL6", "MSI_GOSPEL7", "MSI_GOSPEL8", "MSI_GOSPEL9", "MSI_GOSPEL10",
    "MSI_FULLSTRIP",
]
case_pattern = re.compile(r"\bcase\s+0x([0-9A-Fa-f]+)\b")
case_ids = lambda body: [int(value, 16) for value in case_pattern.findall(body)]

assert case_ids(exe_body) == expected_ids, case_ids(exe_body)
assert case_ids(ref_body) == expected_ids, case_ids(ref_body)
assert case_ids(impl_body) == expected_ids, case_ids(impl_body)
for body in (exe_body, ref_body):
    branches = re.split(r"\bcase\s+0x[0-9A-Fa-f]+\s*:", body)[1:]
    assert [re.search(r"MsgStr\((MSI_[A-Z0-9_]+)\)", branch).group(1) for branch in branches] == expected_symbols
assert re.search(r"case 533:\s*CGameMode::Zc_SKILLMSG\(this, buf\);", exe)
assert re.search(r"case 533:\s*CGameMode::Zc_SKILLMSG\(this, buf\);", ref)
assert "router.Register(0x0215, HandleSkillMessage);" in impl
assert impl_body.count("g_windowMgr.PushChatEvent(") == 1
assert impl_body.count("return;") == 2, "Expected malformed-packet and default no-op exits"

compact = lambda value: re.sub(r"\s+", "", value)
assert compact(exe_body[exe_body.index("switch ("):]) == compact(ref_body[ref_body.index("switch ("):])
assert re.search(r"^0x0215,6\s*$", (ROOT / "Ref/RunningServer/packet_db.txt").read_text(encoding="utf-8", errors="replace"), re.M)

pe_offset = int.from_bytes(binary[0x3C:0x40], "little")
section_count = int.from_bytes(binary[pe_offset + 6:pe_offset + 8], "little")
optional_header = pe_offset + 24
image_base = int.from_bytes(binary[optional_header + 28:optional_header + 32], "little")
section_table = optional_header + int.from_bytes(binary[pe_offset + 20:pe_offset + 22], "little")
fullstrip = None
for index in range(section_count):
    section = section_table + index * 40
    virtual_size = int.from_bytes(binary[section + 8:section + 12], "little")
    virtual_address = int.from_bytes(binary[section + 12:section + 16], "little")
    raw_size = int.from_bytes(binary[section + 16:section + 20], "little")
    raw_offset = int.from_bytes(binary[section + 20:section + 24], "little")
    if image_base + virtual_address <= 0x6FEDE0 < image_base + virtual_address + max(virtual_size, raw_size):
        file_offset = raw_offset + (0x6FEDE0 - image_base - virtual_address)
        fullstrip = binary[file_offset:file_offset + 256].split(b"\0", 1)[0].decode("cp949")
        break
assert fullstrip == "완벽하게 코팅되어있어서 풀스트립이 통하지 않습니다."

print("0x0215 ZC_SKILLMSG source comparison passed")
print("Executable cases: " + ", ".join(f"0x{value:02X}" for value in case_ids(exe_body)))
print("Ref cases:        " + ", ".join(f"0x{value:02X}" for value in case_ids(ref_body)))
print("Current cases:    " + ", ".join(f"0x{value:02X}" for value in case_ids(impl_body)))
print("Branch map: each ID above resolves its matching MSI_GOSPEL1..10/MSI_FULLSTRIP text, then shares one system chat call (color 10198015, channel 7); other IDs return with no output.")
