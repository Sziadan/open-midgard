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


def top_level_case_bodies(function: str) -> dict[int, str]:
    switch = re.search(r"switch\s*\(\s*(?:message|msg)\s*\)\s*\{", function)
    if not switch:
        raise AssertionError("message switch not found")
    offset = switch.end()
    depth = 1
    labels: list[tuple[int, int, int]] = []
    for line_match in re.finditer(r"[^\n]*(?:\n|$)", function[offset:]):
        line = line_match.group(0)
        if depth == 1:
            label = re.match(r"\s*case\s+(\d+)\s*:", line)
            if label:
                labels.append((int(label.group(1)), offset + line_match.start(), offset + line_match.end()))
        depth += line.count("{") - line.count("}")
        if depth == 0:
            break
    return {
        case_id: function[end_label:(labels[index + 1][1] if index + 1 < len(labels) else len(function))]
        for index, (case_id, _, end_label) in enumerate(labels)
    }


ref = body(REF, "void __thiscall CGameActor::SendMsg(CGameActor *this, int sender, int message, int arg1, float arg2, int arg3)\n{")
exe = body(EXE, "void __thiscall CGameActor::SendMsg(CGameActor *this, int sender, int message, int arg1, float arg2, int arg3)\n{")
actor = body(ACTOR, "void CGameActor::SendMsg(CGameObject* src, int msg, msgparam_t par1, msgparam_t par2, msgparam_t par3)\n{")
able = body(ACTOR, "void CAbleToMakeEffect::SendMsg(CGameObject* sender, int msg, msgparam_t par1, msgparam_t par2, msgparam_t)\n{")

ref_cases = top_level_cases(ref)
exe_cases = top_level_cases(exe)
ref_bodies = top_level_case_bodies(ref)
actor_cases = top_level_cases(actor)
able_cases = top_level_cases(able)
assert ref_cases == exe_cases, f"Ref/EXE case mismatch: Ref-only={sorted(ref_cases - exe_cases)}, EXE-only={sorted(exe_cases - ref_cases)}"

able_inherited = ref_cases & able_cases - actor_cases
actor_lifecycle_inherited = {48}
inherited = able_inherited | (actor_lifecycle_inherited - actor_cases)
out_of_scope_ui = {7, 32, 33, 54, 55}
ui_evidence = {
    7: "UITransBalloonText",
    32: "UIChatRoomTitle",
    33: "m_chatTitle",
    54: "m_merchantShopTitle",
    55: "UIMerchantShopTitle",
}
for case_id, ui_class in ui_evidence.items():
    assert case_id in out_of_scope_ui and ui_class in ref_bodies[case_id], f"case {case_id} UI scope evidence changed"
assert ref_cases == exe_cases, "canonical source case inventory drifted"
current_only = actor_cases - ref_cases
assert current_only == {34, 35}, f"current-only actor IDs changed: {sorted(current_only)}"
assert actor_lifecycle_inherited.isdisjoint(actor_cases | out_of_scope_ui)
assert len(ref_cases) == 79, f"expected 79 canonical actor message IDs, found {len(ref_cases)}"
assert len(actor_cases) == 58, f"expected 58 current actor message IDs, found {len(actor_cases)}"
unmapped = ref_cases - actor_cases - able_cases - actor_lifecycle_inherited - out_of_scope_ui
assert len(unmapped) == 17, f"expected 17 in-scope actor IDs still unmapped, found {len(unmapped)}"
print(f"Ref and canonical EXE top-level message IDs: {len(ref_cases)}")
print(f"Current CGameActor::SendMsg direct IDs: {len(actor_cases)}")
print(f"Current-only actor UI extension IDs: {sorted(current_only)}")
print(f"IDs delegated to the current CAbleToMakeEffect::SendMsg: {sorted(able_inherited)}")
print(f"Actor lifecycle callbacks represented outside SendMsg: {sorted(actor_lifecycle_inherited - actor_cases)}")
print(f"UI-only source IDs excluded by plan scope: {sorted(out_of_scope_ui)}")
print(f"Remaining in-scope source IDs without a direct/inherited current case: {len(unmapped)}")
print("Remaining IDs:", sorted(unmapped))
print("Inventory only; this is not a branch-parity pass. Compare each source arm and its inherited behavior before marking WP2.2 complete.")
