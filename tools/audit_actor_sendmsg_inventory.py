#!/usr/bin/env python3
"""Print the top-level message case inventory for CGameActor::SendMsg."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
REF = (ROOT / "Ref/GameActor.cpp").read_text(encoding="utf-8", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
ACTOR = (ROOT / "src/world/GameActor.cpp").read_text(encoding="utf-8")


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
    raise AssertionError(f"unclosed function: {marker}")


def top_level_cases(function: str) -> set[int]:
    switch = re.search(r"switch\s*\(\s*(?:message|msg)\s*\)\s*\{", function)
    if not switch:
        raise AssertionError("message switch not found")
    offset = switch.end()
    depth = 1
    cases: set[int] = set()
    for line_match in re.finditer(r"[^\n]*(?:\n|$)", function[offset:]):
        line = line_match.group(0)
        if depth == 1:
            label = re.match(r"\s*case\s+(\d+)\s*:", line)
            if label:
                cases.add(int(label.group(1)))
        depth += line.count("{") - line.count("}")
        if depth == 0:
            break
    return cases


ref = body(REF, "void __thiscall CGameActor::SendMsg(CGameActor *this, int sender, int message, int arg1, float arg2, int arg3)\n{")
exe = body(EXE, "void __thiscall CGameActor::SendMsg(CGameActor *this, int sender, int message, int arg1, float arg2, int arg3)\n{")
actor = body(ACTOR, "void CGameActor::SendMsg(CGameObject* src, int msg, msgparam_t par1, msgparam_t par2, msgparam_t par3)\n{")
able = body(ACTOR, "void CAbleToMakeEffect::SendMsg(CGameObject* sender, int msg, msgparam_t par1, msgparam_t par2, msgparam_t)\n{")

ref_cases = top_level_cases(ref)
exe_cases = top_level_cases(exe)
actor_cases = top_level_cases(actor)
able_cases = top_level_cases(able)
assert ref_cases == exe_cases, f"Ref/EXE case mismatch: Ref-only={sorted(ref_cases - exe_cases)}, EXE-only={sorted(exe_cases - ref_cases)}"

able_inherited = ref_cases & able_cases - actor_cases
actor_lifecycle_inherited = {48}
inherited = able_inherited | (actor_lifecycle_inherited - actor_cases)
unmapped = ref_cases - actor_cases - able_cases - actor_lifecycle_inherited
print(f"Ref and canonical EXE top-level message IDs: {len(ref_cases)}")
print(f"Current CGameActor::SendMsg direct IDs: {len(actor_cases)}")
print(f"IDs delegated to the current CAbleToMakeEffect::SendMsg: {sorted(able_inherited)}")
print(f"Actor lifecycle callbacks represented outside SendMsg: {sorted(actor_lifecycle_inherited - actor_cases)}")
print(f"Remaining source IDs without a direct/inherited current case: {len(unmapped)}")
print("Remaining IDs:", sorted(unmapped))
print("Inventory only; this is not a branch-parity pass. Compare each source arm and its inherited behavior before marking WP2.2 complete.")
