from __future__ import annotations

import argparse
import hashlib
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / "HighPriest.exe"
SOURCE = ROOT / "Ref/Session2.cpp"
GENERATED = ROOT / "src/core/SourcePetIllustrationPaths.generated.inc"

EXPECTED_BINARY_SHA256 = "9ABC36C88376387D85C878C0C879D9AF8EF66EB1F5152638085DC6995E3F4742"
FUNCTION_VA = 0x680AA0
FUNCTION_END_VA = 0x680E2F  # exclusive; ends after GetPetIllustName's final ret 0xc
FUNCTION_SHA256 = "d172ca3bc29082138fd3fda5dd885dcebfffde43799bb9dbf51393f79785e341"

DEFAULT_VA = 0x724858
DIRECT_TARGETS = {
    1110: 0x680C39,
    1208: 0x680D39,
    1519: 0x680DF0,
    1815: 0x680E1A,
    1879: 0x680E05,
    1963: 0x680DF0,
}
DISPATCHES = (
    # lower job, upper job, normalization base, byte-map VA, pointer-table VA
    (1002, 1109, 1002, 0x680E80, 0x680E30),
    (1113, 1200, 1113, 0x680F14, 0x680EEC),
    (1245, 1385, 1245, 0x680F80, 0x680F6C),
)


def fail(message: str) -> None:
    raise ValueError(message)


def import_pe_readers():
    sys.path.insert(0, str(ROOT / "tools"))
    from generate_source_command_list_tables import read_pe_region, read_pe_va
    return read_pe_region, read_pe_va


def parse_switch(source: str) -> dict[int, int]:
    marker = "void __thiscall CSession::GetPetIllustName(CSession *this, int job, int IsAccEquiped, char *fName)"
    begin = source.find(marker)
    end = source.find("// ================================================", begin + len(marker))
    if begin < 0 or end < 0:
        fail("could not isolate Ref CSession::GetPetIllustName body")
    body = source[begin:end]
    if "// Decompiled from address: 0x680aa0" not in source[max(0, begin - 256):begin]:
        fail("source function address is not 0x680aa0")

    labels = {
        label: int(literal_va, 16)
        for label, literal_va in re.findall(
            r"(?m)^\s*(\$?\w+):\s*sprintf\(fName,\s*&byte_([0-9A-Fa-f]+)\);", body
        )
    }
    cases = list(re.finditer(r"case\s+(\d+)\s*:", body))
    rows: dict[int, int] = {}
    for index, match in enumerate(cases):
        item_id = int(match.group(1))
        stop = cases[index + 1].start() if index + 1 < len(cases) else body.find("default:", match.end())
        if stop < 0:
            stop = len(body)
        clause = body[match.end():stop]
        direct = re.search(r"sprintf\(fName,\s*&byte_([0-9A-Fa-f]+)\);", clause)
        if direct:
            literal_va = int(direct.group(1), 16)
        else:
            goto = re.search(r"goto\s+(\$?\w+)\s*;", clause)
            if not goto or goto.group(1) not in labels:
                fail(f"could not resolve source illustration case {item_id}")
            literal_va = labels[goto.group(1)]
        if item_id in rows:
            fail(f"duplicate source illustration case {item_id}")
        rows[item_id] = literal_va

    default = re.search(r"default:\s*goto\s+(\$?\w+)\s*;", body)
    if not default or default.group(1) not in labels:
        fail("source switch has no resolvable default literal")
    # Six source-level direct comparisons do not appear as `case` labels in
    # the decompiler output. Resolve them explicitly instead of guessing from
    # neighboring job numbers.
    direct_blocks = {
        job: re.search(
            rf"if\s*\(\s*job\s*==\s*{job}\s*\)(.*?)(?:\n\s*\}})", body, re.DOTALL
        )
        for job in (1110, 1208, 1815)
    }
    for job, match in direct_blocks.items():
        if not match:
            fail(f"source function lost direct illustration job {job}")
        literal = re.search(r"sprintf\(fName,\s*&byte_([0-9A-Fa-f]+)\);", match.group(1))
        if not literal:
            fail(f"source function has no direct illustration literal for job {job}")
        rows[job] = int(literal.group(1), 16)
    if not re.search(r"if\s*\(\s*job\s*==\s*1879\s*\)\s*\{\s*\$L155325:", body):
        fail("source function lost direct illustration job 1879")
    rows[1879] = labels["$L155325"]
    if not re.search(r"if\s*\(\s*job\s*==\s*1519\s*\)\s*goto\s+LABEL_46", body):
        fail("source function lost direct illustration job 1519")
    if not re.search(r"if\s*\(\s*job\s*!=\s*1963\s*\)\s*\{\s*\$L155335:", body):
        fail("source function lost direct illustration job 1963")
    rows[1519] = labels["LABEL_46"]
    rows[1963] = labels["LABEL_46"]

    rows[-1] = labels[default.group(1)]
    if len(rows) != 38 or rows[-1] != DEFAULT_VA:
        fail(f"source switch cardinality/default changed: {len(rows) - 1} jobs, default 0x{rows[-1]:08x}")
    return rows


def pointer_at_target(binary: bytes, target: int, read_pe_region) -> int:
    code = read_pe_region(binary, target)
    if len(code) < 16 or code[:3] not in (b"\x8b\x45\x10", b"\x8b\x4d\x10", b"\x8b\x55\x10"):
        fail(f"unrecognized GetPetIllustName literal-return block at 0x{target:08x}")
    expected_push = {b"\x8b\x45\x10": 0x50, b"\x8b\x4d\x10": 0x51, b"\x8b\x55\x10": 0x52}[code[:3]]
    if code[3] != 0x68 or code[8] != expected_push or code[9] != 0xE8:
        fail(f"selected PE literal/pointer instruction mismatch at 0x{target:08x}")
    if code[14:21] != b"\x83\xc4\x08\x5d\xc2\x0c\x00":
        fail(f"selected PE literal-return epilogue mismatch at 0x{target:08x}")
    return struct.unpack_from("<I", code, 4)[0]


def dispatch_target(binary: bytes, job: int, read_pe_region) -> int:
    for low, high, base, map_va, table_va in DISPATCHES:
        if low <= job <= high:
            index = read_pe_region(binary, map_va)[job - base]
            targets = read_pe_region(binary, table_va)
            if 4 * index + 4 > len(targets):
                fail(f"selected PE dispatch index out of bounds for job {job}")
            return struct.unpack_from("<I", targets, 4 * index)[0]
    return DIRECT_TARGETS.get(job, 0x680DDB)


def validate_pe(binary: bytes, read_pe_region, read_pe_va, rows: dict[int, int]) -> None:
    digest = hashlib.sha256(binary).hexdigest().upper()
    if digest != EXPECTED_BINARY_SHA256:
        fail(f"selected PE SHA-256 mismatch: {digest}")

    function = read_pe_region(binary, FUNCTION_VA)[:FUNCTION_END_VA - FUNCTION_VA]
    if len(function) != FUNCTION_END_VA - FUNCTION_VA:
        fail("selected PE GetPetIllustName function extent is truncated")
    if hashlib.sha256(function).hexdigest() != FUNCTION_SHA256:
        fail("selected PE GetPetIllustName function bytes do not match reviewed bounds/hash")
    if function[:3] != bytes.fromhex("558bec") or function[-3:] != bytes.fromhex("c20c00"):
        fail("selected PE GetPetIllustName prologue/epilogue mismatch")

    # Check every job which the UI admits, resolving the PE's indexed switch
    # tables and direct comparisons back to the literal pointer in its block.
    # This catches a source-to-table mismatch even if all path strings still
    # happen to exist somewhere in the binary.
    for job in range(1000, 1986):
        target = dispatch_target(binary, job, read_pe_region)
        actual_va = pointer_at_target(binary, target, read_pe_region)
        expected_va = rows.get(job, rows[-1])
        if actual_va != expected_va:
            fail(f"selected PE job {job} returns 0x{actual_va:08x}, source maps to 0x{expected_va:08x}")

    for job, literal_va in rows.items():
        raw = read_pe_va(binary, literal_va)
        if not raw:
            fail(f"selected PE literal for job {job} is empty at 0x{literal_va:08x}")


def byte_string(data: bytes) -> str:
    # Three-digit octal escapes preserve each raw CP949 byte independent of the
    # Python, editor, or compiler source-code page.
    return '"' + "".join(f"\\{byte:03o}" for byte in data) + '"'


def render(binary: bytes, read_pe_va, rows: dict[int, int]) -> str:
    lines = [
        "// Generated by tools/generate_source_pet_illustration_paths.py; do not edit.",
        f"// Source: Ref/Session2.cpp CSession::GetPetIllustName; PE SHA-256 {EXPECTED_BINARY_SHA256}.",
        f"// Selected PE function VA 0x{FUNCTION_VA:08X}, bytes [{FUNCTION_VA:08X},{FUNCTION_END_VA:08X}), SHA-256 {FUNCTION_SHA256}.",
        "// Values are the selected PE's exact NUL-terminated path bytes (CP949); job -1 is the source switch default.",
        "struct SourcePetIllustrationPath { int job; const char* path; };",
        "constexpr SourcePetIllustrationPath kSourcePetIllustrationPaths[] = {",
    ]
    for job in [*sorted(key for key in rows if key >= 0), -1]:
        literal_va = rows[job]
        raw = read_pe_va(binary, literal_va)
        lines.append(f"    {{{job}, {byte_string(raw)}}}, // VA 0x{literal_va:08X}")
    lines.extend(["};", ""])
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate selected-PE pet illustration paths.")
    parser.add_argument("--check", action="store_true", help="validate source/PE provenance and generated file without writing")
    args = parser.parse_args()
    try:
        read_pe_region, read_pe_va = import_pe_readers()
        binary = BINARY.read_bytes()
        rows = parse_switch(SOURCE.read_text(encoding="utf-8"))
        validate_pe(binary, read_pe_region, read_pe_va, rows)
        expected = render(binary, read_pe_va, rows)
        if args.check:
            if not GENERATED.exists() or GENERATED.read_text(encoding="ascii") != expected:
                fail(f"{GENERATED.relative_to(ROOT)} is stale; run {Path(__file__).name}")
            print("source pet illustration paths: source switch, selected PE provenance, and generated file are current")
        else:
            GENERATED.write_text(expected, encoding="ascii", newline="\n")
            print(f"wrote {GENERATED.relative_to(ROOT)}")
        return 0
    except (OSError, ValueError, KeyError, TypeError, UnicodeError, struct.error) as exc:
        print(f"source pet illustration path generation failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
