#!/usr/bin/env python3
"""Inventory CEffectPrim::OnProcess source cases and current split dispatch routes."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffectPrim.cpp").read_text(encoding="utf-8", errors="replace")
CURRENT = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")


def body(text: str, marker: str) -> str:
    start = text.index(marker)
    opening = text.index("{", start)
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[opening + 1:pos]
    raise AssertionError(f"unclosed function body: {marker}")


def check(label: str, condition: bool) -> None:
    assert condition, f"FAIL {label}"
    print(f"PASS {label}")


exe_marker = "unsigned __int8 __thiscall CEffectPrim::OnProcess(CEffectPrim *this)\n{"
ref_marker = "unsigned __int8 __thiscall CEffectPrim::OnProcess(CEffectPrim *this)\n{"
current_marker = "u8 CEffectPrim::OnProcess()"
exe_body = body(EXE, exe_marker)
ref_body = body(REF, ref_marker)
current_body = body(CURRENT, current_marker)
exe_switch_start = exe_body.index("switch ( this->m_type )", exe_body.index("LABEL_10:"))
source_dispatch = body(exe_body[exe_switch_start:], "switch ( this->m_type )")
source_outer = exe_body[:exe_switch_start]
source_cases = re.findall(r"\bcase\s+(0x[\da-fA-F]+|\d+)\s*:", source_dispatch)
source_ids = {int(value, 0) for value in source_cases}
outer_cases = {int(value, 0) for value in re.findall(r"\bcase\s+(0x[\da-fA-F]+|\d+)\s*:", source_outer)}
source_copy_ids = outer_cases - {0, 0x10}
current_copy_switch = body(current_body, "switch (static_cast<int>(m_type))")
current_copy_ids = {int(value, 0) for value in re.findall(r"\bcase\s+(0x[\da-fA-F]+|\d+)\s*:", current_copy_switch)}

enum_values = {name: int(value, 16) for name, value in re.findall(r"\b(PP_[A-Z0-9_]+)\s*=\s*0x([0-9A-Fa-f]+)", HEADER)}
type_aliases = {name: enum_values[enum_name] for name, enum_name in re.findall(
    r"constexpr int (kPrimType\w+)\s*=\s*(PP_\w+)", HEADER) if enum_name in enum_values}
current_dispatch = body(CURRENT, "bool CEffectPrim::ProcessPrimitiveFrame()")
current_explicit_ids = set()
for name in re.findall(r"\bkPrimType\w+", current_dispatch):
    if name in type_aliases:
        current_explicit_ids.add(type_aliases[name])
for name in re.findall(r"\bPP_[A-Z0-9_]+\b", current_dispatch):
    if name in enum_values:
        current_explicit_ids.add(enum_values[name])
for value in re.findall(r"static_cast<EFFECTPRIMID>\((0x[\da-fA-F]+|\d+)\)", current_dispatch):
    current_explicit_ids.add(int(value, 0))

check("Ref OnProcess body and canonical EXE body are whitespace-normalized identical",
      re.sub(r"\s+", " ", ref_body) == re.sub(r"\s+", " ", exe_body))
check("source has 128 main-dispatch labels plus 17 outer-gate labels, covering 128 unique ids",
      len(source_cases) == 128 and len(outer_cases) == 17 and len(source_ids) == 128)
check("current outer position-copy IDs match the source's 15 pre-gate cases",
      len(source_copy_ids) == 15 and current_copy_ids == source_copy_ids)
check("current wrapper retains null, frame-skip, failure, single-tick and inclusive-duration gates",
      "if (!m_master)" in current_body
      and "m_gi[0].life == 99" in current_body
      and "if (!ProcessPrimitiveFrame())" in current_body
      and current_body.count("++m_stateCnt;") == 1
      and "return m_stateCnt <= m_duration;" in current_body)
check("current split dispatch has an explicit route inventory for review",
      len(current_explicit_ids) == 59 and {0x04, 0x0E, 0x11, 0x14}.issubset(current_explicit_ids)
      and "bool CEffectPrim::ProcessPrimitiveFrame()" in CURRENT)
check("case 0x04/0x0E inline circle/sphere mutations are routed unconditionally",
      re.search(r"case 4:.*?ProcessAlpha\(this\);", source_dispatch, re.S) is not None
      and re.search(r"case 0xE:.*?ProcessAlpha\(this\);", source_dispatch, re.S) is not None
      and "if (m_type == PP_2DCIRCLE) {\n        UpdateReferenceScreenCircle(this);" in current_dispatch
      and "if (m_type == PP_3DSPHERE) {\n        UpdateReferenceSphere(this);" in current_dispatch)
check("case 0x11 no-op and case 0x14 Y increment match source and current helper branches",
      re.search(r"case 0x11:\s*goto \$L145961;", source_dispatch) is not None
      and re.search(r"case 0x14:\s*this->m_pos.y = this->m_pos.y \+ 5\.0;\s*goto \$L145961;", source_dispatch) is not None
      and re.search(r"if \(m_type == PP_3DSHINELIGHT\)\s*\{\s*return true;\s*\}", current_dispatch) is not None
      and re.search(r"if \(m_type == PP_3DLINE\)\s*\{\s*m_pos.y \+= 5\.0f;\s*return true;\s*\}", current_dispatch) is not None)
check("current unmatched route has a generic fallback while source reports illegal primitive ids",
      "alpha > 0.0f" in current_dispatch
      and "Illegal Effect PrimitiveId" in source_dispatch)

not_explicit = sorted(source_ids - current_explicit_ids)
print(f"Source-only explicit selectors requiring branch mapping ({len(not_explicit)}): "
      + " ".join(f"0x{value:02X}" for value in not_explicit))
print("Inventory is not a parity pass: contextual UsesReference* routes and generic fallback still require case-by-case branch comparison.")
