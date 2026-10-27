#!/usr/bin/env python3
"""Compare actor messages 5/123 movement update to their shared canonical branch."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
REF = (ROOT / "Ref/GameActor.cpp").read_text(encoding="utf-8", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF_PC = (ROOT / "Ref/Pc.cpp").read_text(encoding="utf-8", errors="replace")
IMPL = (ROOT / "src/world/GameActor.cpp").read_text(encoding="utf-8")


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


def case(function: str, case_id: int) -> tuple[int, int, list[tuple[int, int, int]]]:
    switch = re.search(r"switch\s*\(\s*(?:message|msg)\s*\)\s*\{", function)
    assert switch, "actor message switch missing"
    offset = switch.end()
    depth = 1
    labels = []
    for match in re.finditer(r"[^\n]*(?:\n|$)", function[offset:]):
        line = match.group(0)
        if depth == 1:
            label = re.match(r"\s*case\s+(\d+)\s*:", line)
            if label:
                labels.append((int(label.group(1)), offset + match.start(), offset + match.end()))
        depth += line.count("{") - line.count("}")
        if depth == 0:
            break
    for index, item in enumerate(labels):
        if item[0] == case_id:
            return index, item[2], labels
    raise AssertionError(f"case {case_id} missing")


marker = "void __thiscall CGameActor::SendMsg(CGameActor *this, int sender, int message, int arg1, float arg2, int arg3)\n{"
ref_actor = body(REF, marker)
exe_actor = body(EXE, marker)
impl_actor = body(IMPL, "void CGameActor::SendMsg(CGameObject* src, int msg, msgparam_t par1, msgparam_t par2, msgparam_t par3)\n{")
normalize = lambda value: re.sub(r"\s+", " ", value)
assert normalize(ref_actor) == normalize(exe_actor), "Ref/EXE SendMsg bodies differ"

def shared_branch(function: str) -> str:
    index, after_first, labels = case(function, 5)
    assert labels[index + 1][0] == 123, "message 5 no longer aliases message 123"
    start = labels[index + 1][2]
    end = labels[index + 2][1]
    return function[start:end]


ref_branch = shared_branch(ref_actor)
exe_branch = shared_branch(exe_actor)
impl_branch = shared_branch(impl_actor)
assert normalize(ref_branch) == normalize(exe_branch), "shared message 5/123 Ref/EXE branches differ"

source = normalize(exe_branch)
source_order = [
    "this->m_speed = *(_DWORD *)(arg1 + 40)",
    "this->m_moveStartTime = v163[3]",
    "v163[1] = v164",
    "this->m_headType = v163[11]",
    "this->gap34)(this, v163[1], v163[2])",
    "CGameActor::SetAttrState(this, v163[14], v163[16], v163[15], 0)",
    "this->m_pkState = v163[17]",
    "this->CGameActor::SetGuildInfo(this, v163[24], v163[25])",
    "this->CGameActor::SetPropensityInfo(this, v163[26], v163[27])",
    "CGameActor::FindPath(this, v163[4], v163[5], v163[6], v163[7], v163[8], v163[9], v163[10])",
    "this->m_moveDestX = v163[8]",
    "this->m_moveDestY = v163[9]",
    "this->CGameActor::SetState(this, 1)",
    "this->m_pathStartCell = 0",
]
source_positions = [source.index(token) for token in source_order]
assert source_positions == sorted(source_positions), "source movement update operation order changed"
assert "IsEffectStateStar2(v163[16])" in source
assert "m_headType = 100" in source

# The separate source CPc and CGrannyPc message-123 overrides both apply the
# party aura. Current source folds these classes into CGameActor::SendMsg, so
# it must preserve that route behind an explicit runtime-class guard.
for class_name in ("CPc", "CGrannyPc"):
    marker = f"void __thiscall {class_name}::SendMsg({class_name} *this, CGameObject *sender, int message, int *arg1, int arg2, int arg3)\n{{"
    pc_ref = body(REF_PC, marker)
    pc_exe = body(EXE, marker)
    assert normalize(pc_ref) == normalize(pc_exe), f"{class_name} Ref/EXE message bodies differ"
    assert re.search(r"case\s+123\s*:[\s\S]*?MakePartyEffect", pc_ref), f"{class_name} message-123 spawn branch must update party effect"

impl = normalize(impl_branch)
impl_order = [
    "m_speed = static_cast<u32>(moveArgs[10])",
    "m_moveStartTime = static_cast<u32>(moveArgs[3])",
    "moveArgs[1] = resolvedJob",
    "m_headType = moveArgs[11]",
    "m_job = resolvedJob",
    "SetAttrState(moveArgs[14], effectState, moveArgs[15], false)",
    "m_pkState = moveArgs[17]",
    "SetGuildInfo(moveArgs[24], moveArgs[25])",
    "pcActor->m_honor = moveArgs[26]",
    "bool foundPath = findPath(m_moveStartTime, static_cast<int>(m_speed))",
    "m_moveDestX = moveArgs[8]",
    "m_moveDestY = moveArgs[9]",
    "SetState(1)",
    "m_pathStartCell = 0",
    "if (msg == 123 && (dynamic_cast<CPc*>(this) != nullptr || dynamic_cast<CGrannyPc*>(this) != nullptr))",
    "MakePartyEffect(moveArgs[0])",
]
impl_positions = [impl.index(token) for token in impl_order]
assert impl_positions == sorted(impl_positions), "implementation operation order changed"
for index in (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 14, 15, 16, 17, 21, 24, 25, 26, 27):
    assert f"moveArgs[{index}]" in impl_branch, f"source payload index {index} missing"
assert "originalJob == 4047 && (effectState & star2Mask) != 0" in impl_branch
assert "lastCellTime <= serverTime" in impl_branch
assert "10 * static_cast<int>(m_speed) / 13" in impl_branch
assert re.search(r"if\s*\(msg\s*==\s*123\s*&&\s*\(dynamic_cast<CPc\*>\(this\)\s*!=\s*nullptr\s*\|\|\s*dynamic_cast<CGrannyPc\*>\(this\)\s*!=\s*nullptr\)\)\s*\{\s*MakePartyEffect\(moveArgs\[0\]\);", impl_branch), "party effect must be limited to PC/GrannyPC spawn message 123, not the shared message-5 or base actor routes"
assert "if (foundPath)" in impl_branch and "else {\n            SetState" not in impl_branch
print("PASS messages 5/123 shared Ref/EXE branch, state/job fields, helper replan, success path, and no failure-state mutation")
