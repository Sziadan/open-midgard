from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / "docs/packets/evidence/PK09.21-friend-greeting-command-ui-data.json"
BINARY = ROOT / "HighPriest.exe"
HEADER = ROOT / "src/core/SourceCommandListTables.h"
GENERATED = ROOT / "src/core/SourceCommandListTables.generated.inc"
HELP_EVIDENCE = ROOT / "docs/packets/evidence/PK09.21-command-help-preparation.json"
HELP_GENERATED = ROOT / "src/core/SourceChatHelpMessages.generated.inc"
EXPECTED = {"toggle": 23, "action": 22, "window": 17, "brazil": 17, "keys": 17}


def fail(message: str) -> None:
    raise ValueError(message)


def read_pe_region(data: bytes, va: int) -> bytes:
    """Return only file-backed bytes from this VA to the end of its section."""
    if data[:2] != b"MZ":
        fail("selected binary is not a PE image (missing MZ header)")
    pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe_offset:pe_offset + 4] != b"PE\0\0":
        fail("selected binary is not a PE image (missing PE signature)")
    section_count = struct.unpack_from("<H", data, pe_offset + 6)[0]
    optional_size = struct.unpack_from("<H", data, pe_offset + 20)[0]
    optional = pe_offset + 24
    magic = struct.unpack_from("<H", data, optional)[0]
    if magic != 0x10B:
        fail(f"selected binary must be PE32, found optional-header magic 0x{magic:x}")
    image_base = struct.unpack_from("<I", data, optional + 28)[0]
    section = optional + optional_size
    rva = va - image_base
    for index in range(section_count):
        off = section + index * 40
        virtual_size, virtual_address, raw_size, raw_pointer = struct.unpack_from("<IIII", data, off + 8)
        if virtual_address <= rva < virtual_address + raw_size:
            file_offset = raw_pointer + (rva - virtual_address)
            section_end = raw_pointer + raw_size
            if section_end > len(data):
                fail("selected PE section extends beyond the file")
            return data[file_offset:section_end]
    fail(f"VA 0x{va:08x} is not backed by a PE section")


def read_pe_va(data: bytes, va: int) -> bytes:
    region = read_pe_region(data, va)
    end = region.find(b"\0")
    if end < 0:
        fail(f"literal VA 0x{va:08x} has no terminator in its PE section")
    return region[:end]


def require_pe_bytes(data: bytes, va: int, expected: bytes) -> None:
    if read_pe_region(data, va)[:len(expected)] != expected:
        fail(f"selected PE bytes disagree at 0x{va:08x}")


def validate_provenance(doc: dict, binary: bytes) -> None:
    expected_sha = doc.get("selected_binary_sha256")
    actual_sha = hashlib.sha256(binary).hexdigest()
    if not expected_sha or actual_sha.lower() != expected_sha.lower():
        fail(f"selected binary SHA-256 mismatch: evidence {expected_sha!r}, actual {actual_sha}")
    inv_sha = doc.get("source_inventory", {}).get("sha256")
    if inv_sha and inv_sha.lower() != actual_sha.lower():
        fail(f"source_inventory SHA-256 mismatch: evidence {inv_sha}, actual {actual_sha}")

    provenance = [("root_verified_pairs", doc.get("root_verified_pairs", []))]
    provenance.append(("root_verified_keys", doc.get("root_verified_keys", [])))
    for group, rows in provenance:
        for row in rows:
            va_text, hex_text = row["literal_va"], row["literal_hex"]
            expected = bytes.fromhex(hex_text)
            actual = read_pe_va(binary, int(va_text, 16))
            if actual != expected:
                fail(f"{group} index {row.get('index')} literal provenance mismatch at {va_text}: "
                     f"evidence {expected.hex()}, binary {actual.hex()}")

    # Validate the numeric half of every caption/type pair as well as the text.
    # WHO alone stores preserved EDI instead of an immediate (root-reviewed
    # def/use span 663E9A..666837). Do not treat it as a zero or another row.
    for row in doc["root_verified_pairs"]:
        store = bytes.fromhex(row["store_bytes_hex"])
        require_pe_bytes(binary, int(row["store_va"], 16), store)
        if store[:3] == b"\xc7\x45\xf8" and len(store) == 7:
            value = struct.unpack_from("<i", store, 3)[0]
        elif (row["table"], row["index"], row["store_va"].lower(), store) == (
                "action", 6, "0x666837", b"\x89\x7d\xf8"):
            require_pe_bytes(binary, 0x663E9A, b"\xbf\x06\x00\x00\x00")
            value = 6
        else:
            fail(f"unreviewed command value store: {row['store_va']}")
        if value != row["selected_value"]:
            fail(f"command value mismatch at {row['store_va']}")

    bindings = doc["source_inventory"]["pointer_store_verification"]["groups"]
    tables = {"m_cmdOnOffTable": "toggle", "m_cmdExeTable": "action",
              "brazil_entries": "brazil", "standard_entries": "window"}
    for group, table in (*tables.items(), ("m_keyWinTable", "keys")):
        rows = doc["root_verified_keys"] if table == "keys" else [
            row for row in doc["root_verified_pairs"] if row["table"] == table]
        if len(bindings[group]) != len(rows):
            fail(f"pointer binding cardinality mismatch: {group}")
        for row, binding in zip(rows, bindings[group], strict=True):
            if int(binding["index"]) != row["index"]:
                fail(f"pointer binding order mismatch: {group}")
            va = int(row["literal_va"], 16)
            if int(binding["label_VA"], 16) != va or int(binding["stored_immediate"], 16) != va:
                fail(f"pointer binding value mismatch: {group} row {row['index']}")
            prefix = b"\xc7\x45\xf8" if table == "keys" else b"\xc7\x45\xfc"
            require_pe_bytes(binary, int(binding["pointer_store"], 16), prefix + struct.pack("<I", va))


def rows_by_table(doc: dict) -> dict[str, list[dict]]:
    pairs = doc.get("root_verified_pairs", [])
    grouped = {name: [] for name in ("toggle", "action", "window", "brazil")}
    for row in pairs:
        table = row.get("table")
        if table not in grouped:
            fail(f"unexpected root_verified_pairs table: {table!r}")
        grouped[table].append(row)
    for name, expected in EXPECTED.items():
        rows = doc.get("root_verified_keys", []) if name == "keys" else grouped[name]
        if len(rows) != expected:
            fail(f"cardinality mismatch for {name}: expected {expected} rows, found {len(rows)}")
        indices = [row.get("index") for row in rows]
        if indices != list(range(1, expected + 1)):
            fail(f"index/order mismatch for {name}: expected 1..{expected}, found {indices}")
    return grouped


def cpp_literal(hex_text: str) -> str:
    # Hex escapes preserve CP1252 and '?' bytes independent of the source encoding.
    return '"' + "".join(f"\\x{byte:02x}" for byte in bytes.fromhex(hex_text)) + '"'


def make_entry(caption_hex: str, value: int) -> str:
    return f"    {{{cpp_literal(caption_hex)}, {value}}},"


def render_header() -> str:
    return """#pragma once

namespace ro::chat {

struct CommandListEntry {
    const char* caption;
    int value;
};

#include \"SourceCommandListTables.generated.inc\"

} // namespace ro::chat
"""


def render_generated(doc: dict, grouped: dict[str, list[dict]], json_sha: str) -> str:
    keys = doc["root_verified_keys"]
    # Shortcut values are the selected window-manager key IDs from the matching
    # standard window rows, whose evidence gives the numeric values explicitly.
    standard_windows = grouped["window"]
    arrays = [
        ("SourceCommandToggles", grouped["toggle"], "literal_hex", "selected_value"),
        ("SourceCommandActions", grouped["action"], "literal_hex", "selected_value"),
        ("SourceCommandWindows", standard_windows, "literal_hex", "selected_value"),
        ("SourceCommandWindowsBrazil", grouped["brazil"], "literal_hex", "selected_value"),
    ]
    lines = [
        "// Generated by tools/generate_source_command_list_tables.py; do not edit.",
        f"// Authoritative evidence SHA-256: {json_sha}",
        f"// Selected binary SHA-256: {doc['selected_binary_sha256']}",
        "",
    ]
    for name, rows, caption_field, value_field in arrays:
        lines.append(f"inline constexpr CommandListEntry {name}[] = {{")
        lines.extend(make_entry(row[caption_field], int(row[value_field])) for row in rows)
        lines.extend(["};", ""])
    lines.append("inline constexpr CommandListEntry SourceCommandWindowKeys[] = {")
    for key, window in zip(keys, standard_windows, strict=True):
        lines.append(make_entry(key["literal_hex"], int(window["selected_value"])))
    lines.extend(["};", ""])
    return "\n".join(lines)


def render_help(doc: dict, binary: bytes, json_sha: str) -> str:
    if doc["binary_sha256"].lower() != hashlib.sha256(binary).hexdigest():
        fail("help bundle selected binary SHA-256 mismatch")
    rows = doc["items"]
    if len(rows) != 74 or [row["order"] for row in rows] != list(range(1,75)):
        fail("help bundle must contain70 ordered messages and4 tail alternatives")
    addresses = [int(row["pe_push_va"],16) for row in rows]
    if addresses != sorted(set(addresses)):
        fail("help bundle must preserve selected instruction order")
    for row, va in zip(rows,addresses,strict=True):
        region = read_pe_region(binary,va)
        if region[0] == 0x68:
            value = struct.unpack_from("<I",region,1)[0]
        elif region[0] == 0x6a:
            value = struct.unpack_from("<b",region,1)[0]
        else:
            fail(f"help ID is not an immediate push at0x{va:x}")
        if value != row["id"]:
            fail(f"help MsgStr ID disagrees at0x{va:x}")
        call = int(row["pe_msgstr_call_va"],16)
        relative = 0x514350-(call+5)
        require_pe_bytes(binary,call,b"\xe8"+struct.pack("<i",relative))
    if [row["id"] for row in rows[70:]] != [921,922,760,789]:
        fail("help random-tail alternatives need renewed source control-flow review")
    lines = ["// Generated by tools/generate_source_command_list_tables.py; do not edit.",
             f"// Help evidence SHA-256 (normalized LF): {json_sha}",
             f"// Selected binary SHA-256: {doc['binary_sha256']}",
             "constexpr int kSourceHelpMessageIds[] = {"]
    for row in rows[:70]:
        lines.append(f"    {row['id']}, // {row['symbol']}")
    lines.extend(["};", "constexpr int kSourceHelpTailMessages[] = {921,922,760,789};", ""])
    return "\n".join(lines)


def sync(path: Path, expected: str, check: bool) -> bool:
    if check:
        if not path.is_file():
            print(f"--check: missing generated file: {path.relative_to(ROOT)}", file=sys.stderr)
            return False
        current = path.read_text(encoding="utf-8")
        if current != expected:
            print(f"--check: stale generated file: {path.relative_to(ROOT)}", file=sys.stderr)
            return False
    else:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(expected, encoding="utf-8", newline="\n")
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate immutable source command-list tables.")
    parser.add_argument("--check", action="store_true", help="validate evidence and generated files without writing")
    args = parser.parse_args()
    try:
        source_bytes = EVIDENCE.read_bytes()
        doc = json.loads(source_bytes.decode("utf-8"))
        grouped = rows_by_table(doc)
        binary = BINARY.read_bytes()
        validate_provenance(doc, binary)
        json_sha = hashlib.sha256(source_bytes).hexdigest()
        ok = sync(HEADER, render_header(), args.check)
        ok = sync(GENERATED, render_generated(doc, grouped, json_sha), args.check) and ok
        help_text = HELP_EVIDENCE.read_text(encoding="utf-8")
        help_doc = json.loads(help_text)
        help_sha = hashlib.sha256(help_text.encode("utf-8")).hexdigest()
        ok = sync(HELP_GENERATED,render_help(help_doc,binary,help_sha),args.check) and ok
        if args.check and ok:
            print("source command-list tables: evidence, binary provenance, and generated files are current")
        return 0 if ok else 1
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError, struct.error) as exc:
        print(f"source command-list table generation failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
