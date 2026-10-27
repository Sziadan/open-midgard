#!/usr/bin/env python3
"""Verify the CPlayer::ProcessState source thunk maps to inherited current actor processing."""
from pathlib import Path
import re
import struct

ROOT = Path(__file__).resolve().parents[2]
REF = (ROOT / "Ref/Player.cpp").read_text(encoding="utf-8", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
EXE_BIN = (ROOT / "HighPriest.exe").read_bytes()
PC_REF = (ROOT / "Ref/Pc.cpp").read_text(encoding="utf-8", errors="replace")
CURRENT_H = (ROOT / "src/world/GameActor.h").read_text(encoding="utf-8")
CURRENT_CPP = (ROOT / "src/world/GameActor.cpp").read_text(encoding="utf-8")


def body(text: str, marker: str) -> tuple[str, str]:
    start = text.rfind(marker)
    assert start >= 0, f"definition missing: {marker}"
    opening = text.find("{", start)
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[start:opening].strip(), text[opening + 1:pos]
    raise AssertionError(f"unclosed function: {marker}")


normalize = lambda value: re.sub(r"\s+", " ", value).strip()
marker = "unsigned __int8 __thiscall CPlayer::ProcessState(CPlayer *this)"
ref_sig, ref = body(REF, marker)
assert normalize(ref_sig) == normalize(marker), "CPlayer::ProcessState Ref signature differs"
assert re.search(re.escape(marker) + r"\s*;\s*// idb", EXE), "canonical EXE declaration missing"
assert re.fullmatch(r"\s*return\s+CPc::ProcessState\(this\)\s*;\s*", ref), \
    "CPlayer::ProcessState is no longer a direct CPc thunk"

pe = struct.unpack_from("<I", EXE_BIN, 0x3C)[0]
section_count = struct.unpack_from("<H", EXE_BIN, pe + 6)[0]
optional_size = struct.unpack_from("<H", EXE_BIN, pe + 20)[0]
optional = pe + 24
image_base = struct.unpack_from("<I", EXE_BIN, optional + 28)[0]
section_table = optional + optional_size


def read_va(va: int, size: int) -> bytes:
    rva = va - image_base
    for index in range(section_count):
        section = section_table + index * 40
        virtual_size, virtual_address, raw_size, raw_pointer = struct.unpack_from("<IIII", EXE_BIN, section + 8)
        if virtual_address <= rva < virtual_address + max(virtual_size, raw_size):
            offset = raw_pointer + rva - virtual_address
            return EXE_BIN[offset:offset + size]
    raise AssertionError(f"VA {va:#x} does not map to a PE section")


thunk_va = 0x5E5630
target_va = 0x5D9EB0
thunk = read_va(thunk_va, 5)
assert thunk[0] == 0xE9, f"expected EXE relative-jump thunk, got {thunk.hex()}"
actual_target = thunk_va + 5 + struct.unpack_from("<i", thunk, 1)[0]
assert actual_target == target_va, f"EXE thunk target is {actual_target:#x}, expected CPc::ProcessState {target_va:#x}"
print("PASS Ref thunk body and canonical EXE bytes at 0x5E5630 jump directly to CPc::ProcessState at 0x5D9EB0")

pc_marker = "unsigned __int8 __thiscall CPc::ProcessState(CPc *this)"
pc_ref = body(PC_REF, pc_marker)[1]
assert normalize(pc_ref) and "switch" in pc_ref, "CPc source process-state implementation missing"

player_decl = re.search(r"class CPlayer\s*:\s*public\s+CPc\s*\{(?P<body>.*?)\n\};", CURRENT_H, re.S)
assert player_decl, "current CPlayer must inherit CPc"
assert not re.search(r"\bProcessState\s*\(\s*\)\s*(?:const\s*)?(?:override\s*)?;", player_decl.group("body")), \
    "current CPlayer added a ProcessState override that needs a separate parity audit"
pc_decl = re.search(r"class CPc\s*:\s*public\s+CGameActor\s*\{(?P<body>.*?)\n\};", CURRENT_H, re.S)
assert pc_decl and not re.search(r"\bProcessState\s*\(", pc_decl.group("body")), \
    "current CPc should inherit the consolidated actor ProcessState"
actor_marker = "u8 CGameActor::ProcessState()"
actor = body(CURRENT_CPP, actor_marker)[1]
assert re.search(r"case 6:[\s\S]*?if\s*\(m_isPc\)\s*ProcessMotion\(\)", actor), \
    "current inherited route is missing the PC sit-state behavior"
print("PASS current CPlayer -> CPc -> CGameActor inherited route and PC state-6 behavior")
