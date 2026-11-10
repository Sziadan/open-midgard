from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path
from generate_source_command_list_tables import validate_provenance, rows_by_table

ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / "docs/packets/evidence/PK09.21-command-hover-preparation.json"
BINARY = ROOT / "HighPriest.exe"
GENERATED = ROOT / "src/core/SourceCommandHoverHelp.generated.inc"
EXPECTED_ROWS = 45
TABLE_EVIDENCE = ROOT / "docs/packets/evidence/PK09.21-friend-greeting-command-ui-data.json"
MSGSTR_CALL_VA = 0x00491AF4
MSGSTR_VA = 0x00514350


def fail(message: str) -> None:
    raise ValueError(message)


def read_pe_region(data: bytes, va: int) -> bytes:
    if data[:2] != b"MZ":
        fail("selected binary is not a PE image (missing MZ header)")
    pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe_offset:pe_offset + 4] != b"PE\0\0":
        fail("selected binary is not a PE image (missing PE signature)")
    section_count = struct.unpack_from("<H", data, pe_offset + 6)[0]
    optional_size = struct.unpack_from("<H", data, pe_offset + 20)[0]
    optional = pe_offset + 24
    if struct.unpack_from("<H", data, optional)[0] != 0x10B:
        fail("selected binary is not PE32")
    image_base = struct.unpack_from("<I", data, optional + 28)[0]
    section = optional + optional_size
    rva = va - image_base
    for index in range(section_count):
        off = section + index * 40
        virtual_address, raw_size, raw_pointer = struct.unpack_from("<III", data, off + 12)
        if virtual_address <= rva < virtual_address + raw_size:
            file_offset = raw_pointer + (rva - virtual_address)
            end = raw_pointer + raw_size
            if end > len(data):
                fail("selected PE section extends beyond file")
            return data[file_offset:end]
    fail(f"VA 0x{va:08x} is not backed by a PE section")


def require_pe_bytes(data: bytes, va: int, expected: bytes) -> None:
    if read_pe_region(data, va)[:len(expected)] != expected:
        fail(f"selected PE bytes disagree at 0x{va:08x}")


def load_evidence() -> tuple[dict, bytes]:
    # read_text normalizes CRLF/LF so the provenance fingerprint is stable
    # across checkouts with different line-ending settings.
    normalized = EVIDENCE.read_text(encoding="utf-8").encode("utf-8")
    return json.loads(normalized.decode("utf-8")), normalized


def validate(doc: dict, binary: bytes) -> list[dict]:
    actual_sha = hashlib.sha256(binary).hexdigest()
    expected_sha = doc.get("selected_binary_sha256", "")
    if actual_sha.lower() != expected_sha.lower():
        fail(f"selected binary SHA-256 mismatch: evidence {expected_sha!r}, actual {actual_sha}")
    if doc.get("status") != "selected_pe_static_hover_mapping_only":
        fail(f"unexpected hover evidence status: {doc.get('status')!r}")
    function = doc["function"]
    toggle = function["toggle_rows"]
    actions = function["action_rows"]
    counts = function["table_counts"]
    if len(toggle) != counts["toggle"] or len(actions) != counts["action"]:
        fail("hover table counts do not match row arrays")
    rows = toggle + actions
    if len(rows) != EXPECTED_ROWS:
        fail(f"expected {EXPECTED_ROWS} hover rows, found {len(rows)}")
    if len(toggle)!=23 or len(actions)!=22:
        fail("hover tables must match the selected23 toggle/22 action rows")
    tables=json.loads(TABLE_EVIDENCE.read_text(encoding="utf-8"))
    grouped=rows_by_table(tables);validate_provenance(tables,binary)
    for name, actual in (("toggle",toggle),("action",actions)):
        for expected, row in zip(grouped[name],actual,strict=True):
            if int(row["row"])!=expected["index"] or row["talktype_value"]!=expected["selected_value"]:
                fail(f"hover {name} row disagrees with the selected caption/type table")
            if row["alias"].encode("ascii")!=bytes.fromhex(expected["literal_hex"]):
                fail(f"hover {name} caption disagrees with the selected table")

    dispatch = function["dispatch"]
    lookup_base = int(dispatch["lookup_byte_table_va"], 16)
    jump_base = int(dispatch["jump_pointer_table_va"], 16)
    msgstr_call_rel = MSGSTR_VA - (MSGSTR_CALL_VA + 5)

    for table_name, table_rows in (("toggle", toggle), ("action", actions)):
        for row in table_rows:
            alias = row["alias"]
            value = row["talktype_value"]
            if not isinstance(value, int):
                fail(f"{alias}: TALKTYPE value is not an integer")
            evidence = row["pe_dispatch"]
            lookup_va = int(evidence["lookup_byte_va"], 16)
            expected_index = value - 6
            if lookup_va != lookup_base + expected_index:
                fail(f"{alias}: lookup byte VA disagrees with TALKTYPE-6")
            slot = evidence["jump_slot_index"]
            lookup_value = read_pe_region(binary, lookup_va)[0]
            if lookup_value != slot:
                fail(f"{alias}: selected PE lookup byte is {lookup_value}, expected jump slot {slot}")
            target = struct.unpack_from("<I", read_pe_region(binary, jump_base + slot * 4), 0)[0]
            block_va = int(evidence["block_va"], 16)
            if target != block_va:
                fail(f"{alias}: jump slot {slot} targets 0x{target:08x}, expected 0x{block_va:08x}")

            expected_symbols = row["source_msgstring_symbols"]
            pushes = row["selected_pe_msgstr"]
            if len(expected_symbols) != len(pushes):
                fail(f"{alias}: selected PE MsgStr push count disagrees with source symbols")
            for symbol, push in zip(expected_symbols, pushes, strict=True):
                push_va = int(push["block_va"], 16)
                region = read_pe_region(binary, push_va)
                if region[0] == 0x68:
                    immediate = struct.unpack_from("<I", region, 1)[0]
                elif region[0] == 0x6A:
                    immediate = struct.unpack_from("<b", region, 1)[0]
                else:
                    fail(f"{alias}/{symbol}: expected immediate push opcode at 0x{push_va:08x}")
                if immediate != push["id"]:
                    fail(f"{alias}/{symbol}: immediate ID {immediate} != evidence ID {push['id']}")
                require_pe_bytes(binary, MSGSTR_CALL_VA,
                                 b"\xe8" + struct.pack("<i", msgstr_call_rel))

    no_help = function["no_help_default"]
    taekwon = next((row for row in actions if row["talktype_value"] == no_help["talktype_value"]), None)
    if not taekwon or taekwon["alias"] != no_help["alias"] or taekwon["selected_pe_msgstr"]:
        fail("no-help default does not match the empty /taekwon row")
    specials = function["special_cases"]
    miss_row = next(row for row in toggle if row["alias"] == "/miss")
    showname_row = next(row for row in toggle if row["alias"] == "/showname")
    set2_row = next(row for row in toggle if row["alias"] == "/set2")
    if [item["id"] for item in miss_row["selected_pe_msgstr"]] != [317, 318]:
        fail("/miss selected-PE push order no longer matches true=OFF, false=ON")
    if [item["id"] for item in showname_row["selected_pe_msgstr"]] != [722, 723]:
        fail("/showname selected-PE push order no longer matches true=ON, false=OFF")
    if specials["/miss"]["true"]["selected_pe_id"] != 317 or specials["/miss"]["false"]["selected_pe_id"] != 318:
        fail("/miss true/false selected-PE branch IDs changed")
    if specials["/showname"]["true"]["selected_pe_id"] != 722 or specials["/showname"]["false"]["selected_pe_id"] != 723:
        fail("/showname true/false selected-PE branch IDs changed")
    if [item["id"] for item in set2_row["selected_pe_msgstr"]] != [735]:
        fail("/set2 shared MsgStr fallback push changed")
    if specials["/set2"]["other_services"]["selected_pe_id"] != 735:
        fail("/set2 shared MsgStr fallback ID changed")
    return rows


def render(doc: dict, rows: list[dict], evidence_bytes: bytes) -> str:
    condition_names = {"always": "Always", "miss": "Miss", "showname": "ShowName", "set2": "Set2"}
    lines = [
        "// Generated by tools/generate_source_command_hover_help.py; do not edit.",
        f"// Evidence SHA-256: {hashlib.sha256(evidence_bytes).hexdigest()}",
        f"// Selected binary SHA-256: {doc['selected_binary_sha256']}",
        "",
        "enum class SourceCommandHelpCondition { Always, Miss, ShowName, Set2 };",
        "struct SourceCommandHelpEntry { int type; int first; int second; SourceCommandHelpCondition condition; };",
        "constexpr SourceCommandHelpEntry kSourceCommandHelpEntries[] = {",
    ]
    for row in rows:
        value = row["talktype_value"]
        alias = row["alias"]
        if alias == "/miss":
            first, second, condition = 317, 318, "miss"
        elif alias == "/showname":
            first, second, condition = 722, 723, "showname"
        elif alias == "/set2":
            first, second, condition = 735, -1, "set2"
        elif alias == "/taekwon":
            first, second, condition = -1, -1, "always"
        else:
            pushes = row["selected_pe_msgstr"]
            if len(pushes) != 1:
                fail(f"{alias}: expected exactly one direct-help MsgStr immediate")
            first, second, condition = pushes[0]["id"], -1, "always"
        lines.append(f"    {{{value}, {first}, {second}, SourceCommandHelpCondition::{condition_names[condition]}}}, // {alias} {row['talktype']}")
    lines.extend(["};", ""])
    return "\n".join(lines)


def sync(expected: str, check: bool) -> bool:
    if check:
        if not GENERATED.is_file():
            print(f"--check: missing generated file: {GENERATED.relative_to(ROOT)}", file=sys.stderr)
            return False
        if GENERATED.read_text(encoding="utf-8") != expected:
            print(f"--check: stale generated file: {GENERATED.relative_to(ROOT)}", file=sys.stderr)
            return False
        return True
    GENERATED.parent.mkdir(parents=True, exist_ok=True)
    GENERATED.write_text(expected, encoding="utf-8", newline="\n")
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate selected-PE command hover help descriptors.")
    parser.add_argument("--check", action="store_true", help="validate evidence, PE provenance, and output without writing")
    args = parser.parse_args()
    try:
        doc, evidence_bytes = load_evidence()
        binary = BINARY.read_bytes()
        rows = validate(doc, binary)
        expected = render(doc, rows, evidence_bytes)
        ok = sync(expected, args.check)
        if args.check and ok:
            print("source command hover help: 45 rows, selected PE mapping, MsgStr pushes, and generated file are current")
        return 0 if ok else 1
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError, struct.error) as exc:
        print(f"source command hover help generation failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
