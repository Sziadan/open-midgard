#!/usr/bin/env python3
"""Source gate for CGameActor::MakeSpecialAttackEffect."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
REF = (ROOT / "Ref/GameActor.cpp").read_text(encoding="utf-8", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
IMPL = (ROOT / "src/world/GameActor.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "src/world/GameActor.h").read_text(encoding="utf-8")


def body(text: str, marker: str, *, last: bool = False) -> tuple[str, str]:
    start = (text.rfind if last else text.find)(marker)
    if start < 0:
        raise AssertionError(f"function not found: {marker}")
    opening = text.find("{", start)
    semicolon = text.find(";", start)
    if opening < 0 or (semicolon >= 0 and semicolon < opening):
        raise AssertionError(f"found declaration, not definition: {marker}")
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[start:opening].strip(), text[opening + 1:pos]
    raise AssertionError(f"unclosed function: {marker}")


marker = "void __thiscall CGameActor::MakeSpecialAttackEffect(CGameActor *this, unsigned int targetGid, float hideTime)"
ref_sig, ref_body = body(REF, marker)
exe_sig, exe_body = body(EXE, marker, last=True)
normalize = lambda text: re.sub(r"\s+", " ", text).strip()
assert normalize(ref_sig) == normalize(exe_sig), "Ref/EXE signatures differ"
assert normalize(ref_body) == normalize(exe_body), "complete Ref/EXE bodies differ"
print(f"PASS complete Ref/EXE body equality ({len(normalize(ref_body))} normalized characters)")

# Exact job routes mechanically expanded from the EXE's ordered if/switch
# predicates. The boundary IDs below are important: 1016 bypasses the nested
# `if (v6)` Mandra block, while 1282 explicitly jumps to the arrow label.
MANDRA = {1020, 1068, 1704, 1709, 1781, 1884, 1886, *range(1957, 1962)}
ARROW = {
    1016, 1189, 1253, 1258, 1276, 1282, 1285, 1410, 1412, 1420,
    1453, 1455, 1473, 1495, 1498, 1500, 1531, 1550, 1555,
    *range(1664, 1668), 1688, 1865, *range(6017, 6027),
}
assert not (MANDRA & ARROW), "source route sets overlap"

for anchor in (
    "if ( Job != 1189 )", "v6 = Job - 1016", "if ( v6 )",
    "v7 = v6 - 4", "if ( v7 && v7 != 48 )", "if ( Job == 1282 )",
    "v10 = Job - 1709", "v11 = v10 - 72", "if ( v11 != 84 )",
    "if ( Job >= 6017 && Job <= 6026 )",
    "CGameMode::MakeMandraAttackEffect(GameMode, targetGid, v12, 0.0)",
    "CGameMode::MakeArrowEffect(GameMode, &this->m_pos, targetGid, hideTime, v13)",
):
    assert anchor in ref_body, f"source branch/call anchor missing: {anchor}"
    assert anchor in exe_body, f"EXE branch/call anchor missing: {anchor}"

print(f"PASS source route expansion: {len(MANDRA)} Mandra IDs, {len(ARROW)} arrow IDs")

# Before the implementation exists, this gate records a clean prerequisite.
# Once added, it mechanically extracts each case list and validates each route
# body and the precise arguments, preventing another function from advancing
# on label-only or successful-build evidence.
impl_marker = "void CGameActor::MakeSpecialAttackEffect(unsigned int targetGid, float hideTime)"
if impl_marker not in IMPL:
    assert "MakeSpecialAttackEffect" not in HEADER, "declaration exists without definition"
    print("PASS current implementation is absent; source comparison is the prerequisite")
else:
    _, impl_body = body(IMPL, impl_marker)
    cases = {int(value) for value in re.findall(r"case\s+(\d+)\s*:", impl_body)}
    mandra_call = impl_body.find("MakeMandraAttackEffect")
    arrow_call = impl_body.find("MakeArrowEffect")
    assert mandra_call >= 0 and arrow_call >= 0, "both source routes must be implemented"
    mandra_cases = {int(value) for value in re.findall(r"case\s+(\d+)\s*:", impl_body[:arrow_call])}
    arrow_cases = {int(value) for value in re.findall(r"case\s+(\d+)\s*:", impl_body[arrow_call:])}
    assert mandra_cases == MANDRA, f"Mandra case mismatch: {sorted(mandra_cases ^ MANDRA)}"
    assert arrow_cases == ARROW, f"arrow case mismatch: {sorted(arrow_cases ^ ARROW)}"
    assert cases == MANDRA | ARROW, f"unexpected or missing implementation cases: {sorted(cases ^ (MANDRA | ARROW))}"
    assert re.search(r"MakeMandraAttackEffect\([^;]*targetGid[^;]*job[^;]*0\.0f?\)", impl_body), "Mandra factory arguments differ from source"
    assert re.search(r"MakeArrowEffect\([^;]*m_pos[^;]*targetGid[^;]*hideTime[^;]*job\)", impl_body), "arrow factory arguments differ from source"
    print("PASS implementation case sets and route arguments match source")
