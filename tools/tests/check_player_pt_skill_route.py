#!/usr/bin/env python3
"""Source gate for the Ref PT_SKILL branch and current modern route."""
from __future__ import annotations

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


ref_player = (ROOT / "Ref/Player.cpp").read_text(encoding="utf-8", errors="replace")
exe_player = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
ref_full = extract_definition(ROOT / "Ref/Player.cpp", "void __thiscall CPlayer::ProcessProceed(CPlayer *this)")
exe_full = extract_definition(ROOT / "HighPriest.exe.c", "void __thiscall CPlayer::ProcessProceed(CPlayer *this)")
assert normalize(ref_full) == normalize(exe_full), "ProcessProceed Ref/EXE full body mismatch"

def skill_branch(text: str) -> str:
    start = text.index("case PT_SKILL:")
    end = text.index("case PT_GROUNDSKILL:", start)
    return normalize(text[start:end])


ref_skill = skill_branch(ref_player)
exe_skill = skill_branch(exe_player)
assert ref_skill == exe_skill, "PT_SKILL Ref/EXE branch mismatch"
for token in (
    "m_proceedTargetGid == g_session.m_aid",
    "ConvertToNumCellCloseAndFindNoRedCell",
    "(unsigned int)v36 >= 2 && v36 != 4 && v36 < 16",
    "GameActorByAID->m_actorType == 1",
    "v37 = v37 - 20.0",
    "5 * this->m_skillAttackRange",
    "v37 <= (double)sy + 5.5 || v39",
    "SendMsg(this, 0, 42, 0, 0, 0)",
    "m_stateId != 2",
    "atan2(*(float *)&tActorPosX - this->m_pos.x",
):
    assert token in ref_skill, f"missing canonical PT_SKILL source anchor: {token}"

ref_converter = normalize(extract_definition(
    ROOT / "Ref/GameMode.cpp",
    "void __thiscall CGameMode::ConvertToNumCellCloseAndFindNoRedCell(",
))
exe_converter = normalize(extract_definition(
    ROOT / "HighPriest.exe.c",
    "void __thiscall CGameMode::ConvertToNumCellCloseAndFindNoRedCell(",
))
assert ref_converter == exe_converter, "closest-safe-cell Ref/EXE helper mismatch"
for token in (
    "5 * numCell",
    "dPos.x = v10 * dPos.x",
    "dPos.x = v12 * dPos.x",
    "dPos.y = v12 * (v10 * dPos.y)",
    "floor((dPos.x + x2) / x1b + (double)(m_attr->m_width / 2))",
    "floor(v15 + (double)(this->m_world->m_attr->m_height / 2))",
    "v21 == 1",
    "v21 == 5",
    "if ( ++i >= 8 )",
):
    assert token in ref_converter, f"missing closest-safe-cell source anchor: {token}"

exe_calc_dist_3d = normalize(extract_definition(
    ROOT / "HighPriest.exe.c",
    "double __thiscall CRenderObject::CalcDist(CRenderObject *this, float x, float y, float z)",
))
for token in (
    "v4 = x - this->m_pos.x",
    "v5 = z - this->m_pos.z",
    "dy = y - this->m_pos.y",
    "input = v5 * v5 + v4 * v4",
    "v6 = _sqrt(input)",
    "inputa = v6 * v6 + dy * dy",
    "return _sqrt(inputa)",
):
    assert token in exe_calc_dist_3d, f"missing canonical 3D CalcDist operation: {token}"

mode_source = (ROOT / "src/gamemode/GameMode.cpp").read_text(encoding="utf-8", errors="replace")
input_route = extract_definition(ROOT / "src/gamemode/GameMode.cpp", "bool TryRequestPendingSkillFromScreenPoint(")
pump_route = extract_definition(ROOT / "src/gamemode/GameMode.cpp", "void PumpSkillChaseRequest(")
begin_chase = extract_definition(ROOT / "src/gamemode/GameMode.cpp", "void BeginSkillChase(")
assert "BeginSkillChase(mode, targetGid, skillId, skillLevel, skillRange);" in input_route
assert "const PLAYER_SKILL_INFO* skillInfo = g_session.GetSkillItemBySkillId(static_cast<int>(skillId))" in input_route
assert "const int skillRange = skillInfo ? skillInfo->attackRange : 0" in input_route
assert "mode.m_skillChaseRange = skillRange;" in begin_chase
assert "(std::max)(1, skillRange)" not in begin_chase
assert "SendUseSkillToIdPacket" not in input_route[input_route.index("if (skillUseMode == ShortcutSkillUseMode::ActorTarget)"):]

ordered_current = (
    "mode.GetServerCoor(player->m_pos.x, player->m_pos.z, sourceTileX, sourceTileY)",
    "const float deltaX = player->m_pos.x - target->m_pos.x",
    "static_cast<double>(5 * skillRange) * normalizedDirectionX",
    "static_cast<double>(5 * skillRange) * (inverseLength * deltaZ)",
    "std::floor(static_cast<double>(approachWorldX / zoom)",
    "kRefNeighborOrder",
    "cell.flag != 1 && cell.flag != 5",
    "player->m_stateId >= 2 && player->m_stateId != 4 && player->m_stateId < 16",
    "const float dx = player->m_pos.x - target->m_pos.x",
    "const float dz = player->m_pos.z - target->m_pos.z",
    "const float dy = player->m_pos.y - target->m_pos.y",
    "const float horizontalDistanceSquared = dx * dx + dz * dz",
    "const double horizontalDistance = std::sqrt(static_cast<double>(horizontalDistanceSquared))",
    "const float distanceSquared = static_cast<float>(horizontalDistance * horizontalDistance + dy * dy)",
    "double distance = std::sqrt(static_cast<double>(distanceSquared))",
    "distance -= 20.0f",
    "5 * skillRange) + 5.5 || destinationIsPlayerCell",
    "const bool sent = SendUseSkillToIdPacket(skillId, skillLevel, mode.m_skillChaseTargetGid)",
    "player->m_destCellX == chaseTileX && player->m_destCellZ == chaseTileY",
    "player->m_stateId == 2",
    "std::atan2(static_cast<long double>(target->m_pos.x - player->m_pos.x)",
    "timeGetTime() > player->m_moveReqTick + 1000",
    "player->m_isWaitingMoveAck = 1",
    "player->m_moveReqTick = timeGetTime()",
    "SendMoveRequestPacket(moveTileX, moveTileY)",
)
positions = [pump_route.find(token) for token in ordered_current]
assert all(position >= 0 for position in positions), "missing current PT_SKILL route operation"
assert positions == sorted(positions), "current PT_SKILL source operations changed order"
dispatch_pos = pump_route.find("const bool sent = SendUseSkillToIdPacket(skillId, skillLevel, mode.m_skillChaseTargetGid)")
assert dispatch_pos >= 0 and pump_route.find("ClearSkillChase(mode);", dispatch_pos) > dispatch_pos
assert "IsAttackTargetWithinRange" not in pump_route
assert "ResolveAttackChaseCell" not in pump_route
assert "m_isLeftButtonHeld" not in pump_route and "m_hasHeldMoveTarget" not in pump_route
assert "normalizedDirectionZ" not in pump_route
assert "const float normalizedDirectionX = static_cast<float>(inverseLength * deltaX)" in pump_route
assert "const float projectedOffsetX = static_cast<float>(static_cast<double>(5 * skillRange) * normalizedDirectionX)" in pump_route
assert "const float projectedOffsetZ = static_cast<float>(static_cast<double>(5 * skillRange) * (inverseLength * deltaZ))" in pump_route
assert "TryRequestMoveToCell" not in pump_route, "source PT_SKILL must not route through generalized path candidate selection"

normalized_pump = normalize(pump_route)
expected_neighbors = "{{1, 0}}, {{1, 1}}, {{1, -1}}, {{-1, 0}}, {{-1, 1}}, {{-1, -1}}, {{0, 1}}, {{0, -1}}"
assert expected_neighbors in normalized_pump, "current closest-safe-cell neighbor order changed"
expected_sector_chain = re.compile(
    r"if\s*\(angle\s*<\s*25\.0\)\s*\{\s*moveTileY\s*\+=\s*5;\s*\}"
    r"\s*else if\s*\(angle\s*<\s*70\.0\)\s*\{\s*moveTileX\s*\+=\s*5;\s*moveTileY\s*\+=\s*5;\s*\}"
    r"\s*else if\s*\(angle\s*<\s*115\.0\)\s*\{\s*moveTileX\s*\+=\s*5;\s*\}"
    r"\s*else if\s*\(angle\s*<\s*160\.0\)\s*\{\s*moveTileX\s*\+=\s*5;\s*moveTileY\s*-\=\s*5;\s*\}"
    r"\s*else if\s*\(angle\s*<\s*205\.0\)\s*\{\s*moveTileY\s*-\=\s*5;\s*\}"
    r"\s*else if\s*\(angle\s*<\s*250\.0\)\s*\{\s*moveTileX\s*-\=\s*5;\s*moveTileY\s*\+=\s*5;\s*\}"
    r"\s*else if\s*\(angle\s*<\s*295\.0\)\s*\{\s*moveTileX\s*-\=\s*5;\s*\}"
    r"\s*else if\s*\(angle\s*<\s*340\.0\)\s*\{\s*moveTileX\s*-\=\s*5;\s*moveTileY\s*-\=\s*5;\s*\}"
    r"\s*else\s*\{\s*moveTileY\s*\+=\s*5;\s*\}"
)
assert expected_sector_chain.search(pump_route), "current monster approach sector branch table changed"

print("Ref/EXE PT_SKILL branch and closest-safe-cell helper match; current targeted-skill route anchors/order pass.")
