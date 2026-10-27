#!/usr/bin/env python3
"""Compare actor messages 6/122 cell/subcell movement initialization."""
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


def labels(function: str) -> list[tuple[int, int, int]]:
    switch = re.search(r"switch\s*\(\s*(?:message|msg)\s*\)\s*\{", function)
    assert switch, "actor message switch missing"
    offset = switch.end()
    depth = 1
    result = []
    for match in re.finditer(r"[^\n]*(?:\n|$)", function[offset:]):
        line = match.group(0)
        if depth == 1:
            label = re.match(r"\s*case\s+(\d+)\s*:", line)
            if label:
                result.append((int(label.group(1)), offset + match.start(), offset + match.end()))
        depth += line.count("{") - line.count("}")
        if depth == 0:
            break
    return result


def shared_branch(function: str) -> tuple[str, str]:
    cases = labels(function)
    index = next(i for i, entry in enumerate(cases) if entry[0] == 6)
    assert cases[index + 1][0] == 122, "message 6 no longer aliases message 122"
    return function[cases[index + 1][2]:cases[index + 2][1]], function


marker = "void __thiscall CGameActor::SendMsg(CGameActor *this, int sender, int message, int arg1, float arg2, int arg3)\n{"
ref_actor, exe_actor, impl_actor = body(REF, marker), body(EXE, marker), body(
    IMPL,
    "void CGameActor::SendMsg(CGameObject* src, int msg, msgparam_t par1, msgparam_t par2, msgparam_t par3)\n{",
)
normalize = lambda value: re.sub(r"\s+", " ", value)
assert normalize(ref_actor) == normalize(exe_actor), "Ref/EXE SendMsg bodies differ"
ref_branch, _ = shared_branch(ref_actor)
exe_branch, _ = shared_branch(exe_actor)
impl_branch, _ = shared_branch(impl_actor)
assert normalize(ref_branch) == normalize(exe_branch), "shared message 6/122 Ref/EXE branches differ"

source = normalize(exe_branch)
source_order = [
    "this->m_gid = *(_DWORD *)arg1",
    "v155[1] = v156",
    "this->m_headType = v155[11]",
    "this->gap34)(this, v155[1], v155[2])",
    "CGameActor::SetAttrState(this, v155[14], v155[16], v155[15], 0)",
    "this->m_pkState = v155[17]",
    "this->CGameActor::SetGuildInfo(this, v155[24], v155[25])",
    "this->CGameActor::SetPropensityInfo(this, v155[26], v155[27])",
    "this->m_speed = v155[10]",
    "this->m_moveStartTime = v155[3]",
    "CGameMode::GetClientCoor(v157, v155[4], v155[5], &this->m_pos.x, &this->m_pos.z)",
    "v155[6] - 8",
    "v155[7] - 8",
    "this->m_pos.y = C3dAttr::GetHeight",
    "CGameActor::FindPath(this, v155[4], v155[5], v155[6], v155[7], v155[8], v155[9], v155[10])",
    "this->m_moveDestX = v155[8]",
    "this->m_moveDestY = v162",
    "this->CGameActor::SetState(this, 1)",
    "this->m_pathStartCell = 0",
]
source_positions = [source.index(token) for token in source_order]
assert source_positions == sorted(source_positions), "source position/path order changed"
assert "goto LABEL_391" in source and "LABEL_391: this->CGameActor::SetState(this, 0);" in normalize(exe_actor)
assert "IsEffectStateStar2(v155[16])" in source

impl = normalize(impl_branch)
impl_order = [
    "m_gid = static_cast<u32>(moveArgs[0])",
    "moveArgs[1] = resolvedJob",
    "m_headType = moveArgs[11]",
    "m_job = resolvedJob",
    "SetAttrState(moveArgs[14], effectState, moveArgs[15], false)",
    "m_pkState = moveArgs[17]",
    "SetGuildInfo(moveArgs[24], moveArgs[25])",
    "m_speed = static_cast<u32>(moveArgs[10])",
    "m_moveStartTime = static_cast<u32>(moveArgs[3])",
    "gameMode->GetClientCoor(moveArgs[4], moveArgs[5], m_pos.x, m_pos.z)",
    "moveArgs[6] - 8",
    "moveArgs[7] - 8",
    "m_pos.y = gameMode->m_world->m_attr->GetHeight(m_pos.x, m_pos.z)",
    "bool foundPath = findPath(m_moveStartTime, static_cast<int>(m_speed))",
    "m_moveDestX = moveArgs[8]",
    "m_moveDestY = moveArgs[9]",
    "SetState(1)",
    "m_pathStartCell = 0",
    "SetState(0)",
]
impl_positions = [impl.index(token) for token in impl_order]
assert impl_positions == sorted(impl_positions), "implementation position/path order changed"
for index in (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 14, 15, 16, 17, 21, 24, 25, 26, 27):
    assert f"moveArgs[{index}]" in impl_branch, f"source payload index {index} missing"
assert "originalJob == 4047 && (effectState & star2Mask) != 0" in impl_branch
assert "static_cast<double>(gameMode->m_world->m_attr->m_zoom) * 0.0625" in impl_branch
assert "lastCellTime <= serverTime" in impl_branch
assert "10 * static_cast<int>(m_speed) / 13" in impl_branch
print("PASS messages 6/122 shared Ref/EXE branch, subcell offsets, actor state, helper replan, and success/failure transitions")
