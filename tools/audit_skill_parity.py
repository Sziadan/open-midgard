"""Report static routing/reference gaps. Presence never implies runtime parity.

Run: .venv/Scripts/python.exe tools/audit_skill_parity.py
"""
import csv
from collections import Counter
from pathlib import Path
import re

from skill_reference_index import read_function

ROOT = Path(__file__).resolve().parents[1]
# Opcode inventory in plan section 2.4, including the five skill-list variants
# and three guild variants. These are requirements, not inferred port coverage.
OPCODES = (
    0x0114, 0x01DE, 0x0115, 0x011A, 0x0117, 0x011F, 0x01C9,
    0x0120, 0x01AC, 0x013E, 0x01B9, 0x0110, 0x008A, 0x02E1,
    0x0119, 0x0229, 0x0196, 0x043F, 0x01D0, 0x01E1, 0x01CF,
    0x01CD, 0x01D1, 0x01D2, 0x0215, 0x01F3, 0x0284, 0x019B,
    0x01D3, 0x0191, 0x0148, 0x013D, 0x043D, 0x010F, 0x010E,
    0x0111, 0x0235, 0x029D, 0x0162, 0x0145, 0x01B4,
)


def packet_routes():
    source = (ROOT / "src/gamemode/GameModePacket.cpp").read_text(encoding="utf-8")
    profile = read_function(ROOT / "src/network/MapSendProfile.cpp", "MakePacketVer23ReceiveProfile")
    fields = {field: int(value, 16) for field, value in re.findall(
        r"profile\.(\w+)\s*=\s*(0x[0-9a-fA-F]+);", profile.text)}
    routes = {}
    registration = re.compile(r"router.Register\((0x[0-9a-fA-F]+),\s*(\w+)\)|"
        r"RegisterHandlerIfValid\(router, receiveProfile\.(\w+),\s*(\w+)\)")
    for match in registration.finditer(source):
        opcode = int(match.group(1), 16) if match.group(1) else fields.get(match.group(3))
        if opcode is not None:
            routes[opcode] = match.group(2) or match.group(4)
    return routes


def main():
    with (ROOT / "docs/skill/generated/effect_handler_index.tsv").open(encoding="utf-8") as stream:
        cases = list(csv.DictReader(stream, delimiter="\t"))
    counts = Counter(row["tier"] for row in cases)
    routes = packet_routes()
    gaps = [(opcode, routes.get(opcode, "MISSING")) for opcode in OPCODES
        if routes.get(opcode, "MISSING") in ("MISSING", "HandleIgnorePacket")]
    lines = [
        "Static skill parity audit (not runtime verification)",
        "Reference dispatch case labels: " + str(len(cases)),
        ", ".join(f"{tier}: {counts[tier]}" for tier in ("OnProcess", "OnProcess2", "OnProcess3")),
        f"Packet handlers registered and not ignored: {len(OPCODES) - len(gaps)}/{len(OPCODES)}",
        "Registration does not establish a 1:1 handler body or receiver implementation.",
        "Open packet routes:",
    ]
    lines.extend(f"  0x{opcode:04X}: {handler}" for opcode, handler in gaps)
    lines.extend([
        "SKILL_MATRIX.md: " + ("present (inspect runtime evidence)" if (ROOT / "docs/skill/SKILL_MATRIX.md").exists() else "MISSING"),
        "Effect bodies/primitives: incomplete; see PROGRESS.md and PARITY_LOG.md.",
        "Live/replay per-skill acceptance: not established by this static audit.",
    ])
    report = "\n".join(lines) + "\n"
    (ROOT / "docs/skill/generated/PARITY_AUDIT.txt").write_text(report, encoding="utf-8", newline="\n")
    print(report, end="")


if __name__ == "__main__":
    main()
