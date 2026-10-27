#!/usr/bin/env python3
"""Compare actor message 3 spawn-state initialization to Ref and the canonical EXE."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
REF = (ROOT / "Ref/GameActor.cpp").read_text(encoding="utf-8", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
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


def case(function: str, case_id: int) -> str:
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
    for index, (label, _, end_label) in enumerate(labels):
        if label == case_id:
            return function[end_label:(labels[index + 1][1] if index + 1 < len(labels) else len(function))]
    raise AssertionError(f"case {case_id} missing")


marker = "void __thiscall CGameActor::SendMsg(CGameActor *this, int sender, int message, int arg1, float arg2, int arg3)\n{"
ref_actor = body(REF, marker)
exe_actor = body(EXE, marker)
impl_actor = body(IMPL, "void CGameActor::SendMsg(CGameObject* src, int msg, msgparam_t par1, msgparam_t par2, msgparam_t par3)\n{")
normalize = lambda value: re.sub(r"\s+", " ", value)
assert normalize(ref_actor) == normalize(exe_actor), "Ref/EXE SendMsg bodies differ"
ref_case = case(ref_actor, 3)
exe_case = case(exe_actor, 3)
impl_case = case(impl_actor, 3)
assert normalize(ref_case) == normalize(exe_case), "message 3 Ref/EXE branches differ"

# Map every actor payload field and preserve source write/call order.
expected_source = [
    "CGameMode::GetClientCoor",
    "this->m_pos.y = C3dAttr::GetHeight",
    "45 * (v149[5] + 4)",
    "this->m_roty",
    "this->m_xSize = v149[7]",
    "this->m_ySize = v149[8]",
    "this->m_moveDestX = v149[3]",
    "this->m_moveDestY = v149[4]",
    "this->m_headType = v149[9]",
    "v149[20] == 7",
    "SetAttrState(this, v149[13], v149[15], v149[14], 0)",
    "SetState(this, 0)",
    "SetGuildInfo(this, v149[23], v149[24])",
]
source_text = normalize(exe_case)
positions = [source_text.index(token) for token in expected_source]
assert positions == sorted(positions), "source field/call order changed"

expected_impl = [
    "gameMode->GetClientCoor(actorArgs[3], actorArgs[4], m_pos.x, m_pos.z)",
    "m_pos.y = gameMode->m_world->m_attr->GetHeight(m_pos.x, m_pos.z)",
    "m_roty = static_cast<float>(45 * (actorArgs[5] + 4))",
    "m_xSize = actorArgs[7]",
    "m_ySize = actorArgs[8]",
    "m_moveDestX = actorArgs[3]",
    "m_moveDestY = actorArgs[4]",
    "m_headType = actorArgs[9]",
    "actorArgs[20] == 7",
    "SetAttrState(actorArgs[13], actorArgs[15], actorArgs[14], false)",
    "SetState(0)",
    "SetGuildInfo(actorArgs[23], actorArgs[24])",
]
impl_text = normalize(impl_case)
impl_positions = [impl_text.index(token) for token in expected_impl]
assert impl_positions == sorted(impl_positions), "implementation field/call order changed"
for index in (3, 4, 5, 7, 8, 9, 13, 14, 15, 20, 23, 24):
    assert f"actorArgs[{index}]" in impl_case, f"actor payload field {index} missing"
assert "m_headType = 100" in impl_case
print("PASS message 3 Ref/EXE branch equality; position, facing, sizes, destination, head override, attribute state, idle state, and guild fields mapped in source order")
