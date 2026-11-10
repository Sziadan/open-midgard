from __future__ import annotations

import argparse
import hashlib
import re
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / "HighPriest.exe"
UI_SOURCE = ROOT / "Ref/UIFrameWnd2.cpp"
SESSION_SOURCE = ROOT / "Ref/Session2.cpp"
GENERATED = ROOT / "src/core/SourcePetSummaryText.generated.inc"
EXPECTED_BINARY_SHA256 = "9ABC36C88376387D85C878C0C879D9AF8EF66EB1F5152638085DC6995E3F4742"


@dataclass(frozen=True)
class Literal:
    name: str
    va: int
    source_symbol: str
    function: str


UI_LITERALS = (
    Literal("modified", 0x6F41D0, "byte_6F41D0", "UIItemCollectionWnd::SendMsg"),
    Literal("unmodified", 0x6F41BC, "byte_6F41BC", "UIItemCollectionWnd::SendMsg"),
    Literal("levelPrefix", 0x6F41B4, "byte_6F41B4", "UIItemCollectionWnd::SendMsg"),
    Literal("fullnessPrefix", 0x6F41A8, "byte_6F41A8", "UIItemCollectionWnd::SendMsg"),
    Literal("relationshipPrefix", 0x6F419C, "byte_6F419C", "UIItemCollectionWnd::SendMsg"),
    Literal("equipped", 0x6F4188, "byte_6F4188", "UIItemCollectionWnd::SendMsg"),
    Literal("unequipped", 0x6F4174, "byte_6F4174", "UIItemCollectionWnd::SendMsg"),
)

HUNGER_LITERALS = (
    Literal("hunger0", 0x7017CC, "unk_7017CC", "CSession::GetPetHungryStateText"),
    Literal("hunger1", 0x7017C4, "unk_7017C4", "CSession::GetPetHungryStateText"),
    Literal("hunger2", 0x7017BC, "unk_7017BC", "CSession::GetPetHungryStateText"),
    Literal("hunger3", 0x7017B4, "unk_7017B4", "CSession::GetPetHungryStateText"),
    Literal("hunger4", 0x7017A8, "unk_7017A8", "CSession::GetPetHungryStateText"),
    Literal("hungerUnknown", 0x724D5C, "unk_724D5C", "CSession::GetPetHungryStateText"),
)

FRIENDLY_LITERALS = (
    Literal("friendly0", 0x70179C, "unk_70179C", "CSession::GetPetFriendlyStateText"),
    Literal("friendly1", 0x701794, "unk_701794", "CSession::GetPetFriendlyStateText"),
    Literal("friendly2", 0x7017BC, "unk_7017BC", "CSession::GetPetFriendlyStateText"),
    Literal("friendly3", 0x70178C, "unk_70178C", "CSession::GetPetFriendlyStateText"),
    Literal("friendly4", 0x701784, "unk_701784", "CSession::GetPetFriendlyStateText"),
    Literal("friendlyUnknown", 0x724D64, "unk_724D64", "CSession::GetPetFriendlyStateText"),
)

ALL_LITERALS = (*UI_LITERALS, *HUNGER_LITERALS, *FRIENDLY_LITERALS)


def fail(message: str) -> None:
    raise ValueError(message)


def import_pe_readers():
    sys.path.insert(0, str(ROOT / "tools"))
    from generate_source_command_list_tables import read_pe_region, read_pe_va
    return read_pe_region, read_pe_va


def function_body(source: str, marker: str, va: int, label: str) -> str:
    provenance = source.lower().find(f"decompiled from address: 0x{va:x}")
    if provenance < 0:
        fail(f"source provenance for {label} is not VA 0x{va:08X}")
    begin = source.find(marker, provenance)
    if begin < 0:
        fail(f"could not find source function {label}")
    header = source[max(0, provenance - 256):begin]
    if f"decompiled from address: 0x{va:x}" not in header.lower():
        fail(f"source provenance for {label} is not VA 0x{va:08X}")
    end = source.find("// ================================================", begin + len(marker))
    if end < 0:
        fail(f"could not isolate source function {label}")
    return source[begin:end]


def validate_source(ui_text: str, session_text: str) -> None:
    ui = function_body(
        ui_text,
        "int __thiscall UIItemCollectionWnd::SendMsg(",
        0x479E70,
        "UIItemCollectionWnd::SendMsg",
    )
    # Restrict the source checks to the message-29 case, since the same UI method
    # contains ordinary item-detail resource literals as well.
    case29 = ui.find("case 29:")
    case34 = ui.find("case 34:", case29)
    if case29 < 0 or case34 < 0:
        fail("source SendMsg has no bounded message-29 case")
    message29 = ui[case29:case34]
    if "ITEM_INFO::GetDescription(&this->m_itemInfo)" not in message29:
        fail("message 29 no longer begins from the retained item's description")
    if "this->m_petLevel == -1" not in message29 or "UIWindowMgr::MakeTopLayer" not in ui[case34:]:
        fail("message-29 pet sentinel/top-layer behavior changed")
    for item in UI_LITERALS:
        if item.source_symbol not in message29:
            fail(f"message 29 no longer references {item.source_symbol}")

    hungry = function_body(
        session_text,
        "void __thiscall CSession::GetPetHungryStateText(CSession *this, int hungry, char *buf)",
        0x681550,
        "CSession::GetPetHungryStateText",
    )
    friendly = function_body(
        session_text,
        "void __thiscall CSession::GetPetFriendlyStateText(CSession *this, int friendly, char *buf)",
        0x681600,
        "CSession::GetPetFriendlyStateText",
    )
    for body, literals, label in (
        (hungry, HUNGER_LITERALS, "hunger"),
        (friendly, FRIENDLY_LITERALS, "relationship"),
    ):
        actual = re.findall(r"\(const char \*\)&(unk_[0-9A-Fa-f]+)", body)
        expected = [item.source_symbol for item in literals]
        if len(actual) != len(expected) or set(actual) != set(expected):
            fail(f"source {label} state-string routing changed: {actual}")
        if "strcpy(buf, v3);" not in body:
            fail(f"source {label} state helper no longer copies selected literal to output")


def byte_string(data: bytes) -> str:
    return '"' + "".join(f"\\{byte:03o}" for byte in data) + '"'


def render(binary: bytes, read_pe_va) -> str:
    lines = [
        "// Generated by tools/generate_source_pet_summary_text.py; do not edit.",
        f"// Selected HighPriest.exe SHA-256 {EXPECTED_BINARY_SHA256}.",
        "// Source literals: Ref/UIFrameWnd2.cpp UIItemCollectionWnd::SendMsg @ 0x00479E70 (message 29);",
        "// Ref/Session2.cpp CSession::GetPetHungryStateText @ 0x00681550 and GetPetFriendlyStateText @ 0x00681600.",
        "// Pet fullness/relationship text here is hardcoded source CP949, not MsgStr/UI localization.",
        "namespace ro::pet::summary_text {",
    ]
    for item in ALL_LITERALS:
        raw = read_pe_va(binary, item.va)
        if not raw:
            fail(f"selected PE literal {item.source_symbol} is empty at 0x{item.va:08X}")
        lines.append(f"constexpr char {item.name}[] = {byte_string(raw)}; // {item.source_symbol}, VA 0x{item.va:08X}")
    lines.extend(["} // namespace ro::pet::summary_text", ""])
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate source pet-summary CP949 text literals.")
    parser.add_argument("--check", action="store_true", help="validate source/PE provenance and generated file without writing")
    args = parser.parse_args()
    try:
        read_pe_region, read_pe_va = import_pe_readers()
        binary = BINARY.read_bytes()
        digest = hashlib.sha256(binary).hexdigest().upper()
        if digest != EXPECTED_BINARY_SHA256:
            fail(f"selected PE SHA-256 mismatch: {digest}")
        validate_source(UI_SOURCE.read_text(encoding="utf-8"), SESSION_SOURCE.read_text(encoding="utf-8"))
        expected = render(binary, read_pe_va)
        if args.check:
            if not GENERATED.exists() or GENERATED.read_text(encoding="ascii") != expected:
                fail(f"{GENERATED.relative_to(ROOT)} is stale; run {Path(__file__).name}")
            print("source pet-summary text: source references, selected PE provenance, and generated file are current")
        else:
            GENERATED.write_text(expected, encoding="ascii", newline="\n")
            print(f"wrote {GENERATED.relative_to(ROOT)}")
        return 0
    except (OSError, ValueError, KeyError, TypeError, UnicodeError) as exc:
        print(f"source pet-summary text generation failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
