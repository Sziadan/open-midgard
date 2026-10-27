#!/usr/bin/env python3
"""Source and production-body gate for modern PT_GROUNDSKILL routing."""
from __future__ import annotations

import hashlib
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def extract_definition(path: Path, signature: str) -> str:
    text = path.read_text(encoding="utf-8", errors="replace")
    for match in re.finditer(re.escape(signature), text):
        cursor = match.end()
        while cursor < len(text) and text[cursor] not in "{;":
            cursor += 1
        if cursor >= len(text) or text[cursor] != "{":
            continue
        depth = 0
        for end in range(cursor, len(text)):
            if text[end] == "{":
                depth += 1
            elif text[end] == "}":
                depth -= 1
                if depth == 0:
                    return text[cursor : end + 1]
    raise AssertionError(f"definition not found: {path}::{signature}")


def normalize(body: str) -> str:
    body = re.sub(r"//[^\n]*", "", body)
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
    return re.sub(r"\s+", " ", body).strip()


def ground_branch(body: str) -> str:
    start = body.index("case PT_GROUNDSKILL:")
    end = body.index("case PT_TOUCH_SKILL:", start)
    return normalize(body[start:end])


ref_full = normalize(extract_definition(
    ROOT / "Ref/Player.cpp",
    "void __thiscall CPlayer::ProcessProceed(CPlayer *this)",
))
exe_full = normalize(extract_definition(
    ROOT / "HighPriest.exe.c",
    "void __thiscall CPlayer::ProcessProceed(CPlayer *this)",
))
assert ref_full == exe_full, "ProcessProceed Ref/EXE full body mismatch"
ref_ground = ground_branch(ref_full)
exe_ground = ground_branch(exe_full)
assert ref_ground == exe_ground, "complete PT_GROUNDSKILL Ref/EXE branch mismatch"
print(f"PT_GROUNDSKILL Ref/EXE equal: chars={len(ref_ground)} sha256={hashlib.sha256(ref_ground.encode()).hexdigest()}")

for token in (
    "GetServerCoor(GameMode, this->m_pos.x, this->m_pos.z, &sx, &dx2)",
    "GetClientCoor(GameMode, tActorPosX, dy, (float *)&fdx, (float *)&fdz)",
    "ConvertToNumCellCloseAndFindNoRedCell",
    "this->m_skillAttackRange",
    "(unsigned int)v45 < 2 || v45 == 4 || v45 >= 16",
    "CalcDist(this, *(float *)&fdx, this->m_pos.y, *(float *)&fdz)",
    "5 * this->m_skillAttackRange",
    "v46 <= (double)sy + 5.5 || v48",
    "SendMsg(this, 0, 66, 0, 0, 0)",
    "m_stateId != 2",
    "SendMsg(this, 0, 91, v66, v68, 0)",
):
    assert token in ref_ground, f"missing canonical ground-skill branch anchor: {token}"

ref_converter = normalize(extract_definition(
    ROOT / "Ref/GameMode.cpp",
    "void __thiscall CGameMode::ConvertToNumCellCloseAndFindNoRedCell(",
))
exe_converter = normalize(extract_definition(
    ROOT / "HighPriest.exe.c",
    "void __thiscall CGameMode::ConvertToNumCellCloseAndFindNoRedCell(",
))
assert ref_converter == exe_converter, "closest-safe-cell Ref/EXE helper mismatch"

mode_source = ROOT / "src/gamemode/GameMode.cpp"
input_route = extract_definition(mode_source, "bool TryRequestPendingSkillFromScreenPoint(")
pump_route = extract_definition(mode_source, "void PumpSkillChaseRequest(")
clear_route = extract_definition(mode_source, "void ClearSkillChase(")
assert "mode.m_groundSkillCellX = attrX;" in input_route
assert "mode.m_groundSkillCellY = attrY;" in input_route
assert "const PLAYER_SKILL_INFO* skillInfo = g_session.GetSkillItemBySkillId(static_cast<int>(skillId))" in input_route
assert "mode.m_groundSkillRange = skillInfo ? skillInfo->attackRange : 0;" in input_route
assert "mode.m_hasGroundSkillChase = 1;" in input_route
assert "mode.m_hasSkillChase = 1;" in input_route, "ground intent must block concurrent attack chase"
assert "SendUseSkillToPosPacket" not in input_route
assert "mode.m_hasGroundSkillChase = 0;" in clear_route

ordered_current = (
    "if (mode.m_hasGroundSkillChase)",
    "const u16 groundSkillId = static_cast<u16>(mode.m_groundSkillId & 0xFFFF)",
    "const int skillRange = mode.m_groundSkillRange",
    "mode.GetServerCoor(player->m_pos.x, player->m_pos.z, sourceTileX, sourceTileY)",
    "mode.GetClientCoor(mode.m_groundSkillCellX, mode.m_groundSkillCellY, targetWorldX, targetWorldZ)",
    "const float deltaX = player->m_pos.x - targetWorldX",
    "static_cast<double>(5 * skillRange) * normalizedDirectionX",
    "static_cast<double>(5 * skillRange) * (inverseLength * deltaZ)",
    "std::floor(static_cast<double>(approachWorldX / zoom)",
    "kGroundRefNeighborOrder",
    "cell.flag != 1 && cell.flag != 5",
    "player->m_stateId >= 2 && player->m_stateId != 4 && player->m_stateId < 16",
    "const float dy = 0.0f",
    "distance <= static_cast<double>(5 * skillRange) + 5.5 || destinationIsPlayerCell",
    "const bool sent = SendUseSkillToPosPacket(\n                groundSkillId",
    "player->m_destCellX == approachTileX && player->m_destCellZ == approachTileY",
    "player->m_stateId == 2",
    "player->m_destCellX = approachTileX",
    "timeGetTime() > player->m_moveReqTick + 1000",
    "SendMoveRequestPacket(approachTileX, approachTileY)",
    "if (!mode.m_hasSkillChase || mode.m_skillChaseTargetGid == 0)",
)
positions = [pump_route.find(token) for token in ordered_current]
assert all(position >= 0 for position in positions), "missing current PT_GROUNDSKILL operation"
assert positions == sorted(positions), "current ground-skill operation order changed"
ground_dispatch_pos = pump_route.find("const bool sent = SendUseSkillToPosPacket(")
assert ground_dispatch_pos >= 0 and pump_route.find("ClearSkillChase(mode);", ground_dispatch_pos) > ground_dispatch_pos
assert "SendUseSkillToPosPacket" in pump_route
assert "WID_TALKBOXTRAPINPUTWND" not in pump_route and "WID_GRAFFISTRBOXWND" not in pump_route

normalized_pump = normalize(pump_route)
expected_neighbors = "{{1, 0}}, {{1, 1}}, {{1, -1}}, {{-1, 0}}, {{-1, 1}}, {{-1, -1}}, {{0, 1}}, {{0, -1}}"
assert expected_neighbors in normalized_pump, "ground closest-safe-cell neighbor order changed"

print("PT_GROUNDSKILL source branch and current persistent modern route anchors/order pass.")
