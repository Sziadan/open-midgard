#!/usr/bin/env python3
"""Source and modern-route gate for PT_TOUCH_SKILL and skill-unit targeting."""
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


def branch(body: str, begin: str, end: str) -> str:
    start = body.index(begin)
    finish = body.index(end, start)
    return normalize(body[start:finish])


ref_proceed = normalize(extract_definition(
    ROOT / "Ref/Player.cpp",
    "void __thiscall CPlayer::ProcessProceed(CPlayer *this)",
))
exe_proceed = normalize(extract_definition(
    ROOT / "HighPriest.exe.c",
    "void __thiscall CPlayer::ProcessProceed(CPlayer *this)",
))
assert ref_proceed == exe_proceed, "ProcessProceed Ref/EXE full body mismatch"
ref_touch = branch(ref_proceed, "case PT_TOUCH_SKILL:", "default:")
exe_touch = branch(exe_proceed, "case PT_TOUCH_SKILL:", "default:")
assert ref_touch == exe_touch, "complete PT_TOUCH_SKILL Ref/EXE branch mismatch"
print(f"PT_TOUCH_SKILL Ref/EXE equal: chars={len(ref_touch)} sha256={hashlib.sha256(ref_touch.encode()).hexdigest()}")

for token in (
    "GetSkillByAID(GameMode->m_world, this->m_proceedTargetGid)",
    "GetServerCoor(GameMode, SkillByAID->m_pos.x, SkillByAID->m_pos.z",
    "GetClientCoor(GameMode, fdx, fdz",
    "ConvertToNumCellCloseAndFindNoRedCell",
    "(unsigned int)v56 >= 2 && v56 != 4 && v56 < 16",
    "CalcDist(this, *(float *)&tActorPosX, this->m_pos.y, *(float *)&dx2)",
    "v57 <= (double)sy + 5.5 || v59",
    "CGameActor::GetJob(SkillByAID) - 141",
    "if ( !this->m_skillId )",
    "SendMsg(this, 0, 29, 0, 0, 0)",
    "SendMsg(this, 0, 42, 0, 0, 0)",
    "SendMsg(this, 0, 13, 0, 0, 0)",
    "CGameActor::GetJob(SkillByAID) == 141",
    "CGameActor::GetJob(SkillByAID) == 143",
    "CGameActor::GetJob(SkillByAID) == 152",
    "SendMsg(this, 0, 91, v60, v61, 0)",
):
    assert token in ref_touch, f"missing canonical PT_TOUCH_SKILL source anchor: {token}"

ref_click = normalize(extract_definition(
    ROOT / "Ref/GameMode.cpp",
    "int __thiscall CGameMode::ProcessObjClick(CGameMode *this, CActorPickInfo *actorInfo, int processType)",
))
exe_click = normalize(extract_definition(
    ROOT / "HighPriest.exe.c",
    "int __thiscall CGameMode::ProcessObjClick(CGameMode *this, CActorPickInfo *actorInfo, int processType)",
))
assert ref_click == exe_click, "ProcessObjClick Ref/EXE full body mismatch"
skill_unit_click = branch(ref_click,
    "case 2: if ( actorInfo->m_job != 141",
    "case 3:")
assert "m_skillUseInfo.m_skillUseType != SUT_TO_SKILL" in skill_unit_click
for job in (141, 143, 152):
    assert f"actorInfo->m_job != {job}" in skill_unit_click
for skill_id in (215, 216, 217, 218):
    assert f"v126 != {skill_id}" in skill_unit_click
assert "0, 103, actorInfo->m_gid" in skill_unit_click
print(f"ProcessObjClick Ref/EXE equal: chars={len(ref_click)} sha256={hashlib.sha256(ref_click.encode()).hexdigest()}")

ref_player_send = normalize(extract_definition(
    ROOT / "Ref/Player.cpp",
    "void __thiscall CPlayer::SendMsg(CPlayer *this, CGameObject *sender, int message, int arg1, int *arg2, int arg3)",
))
exe_player_send = normalize(extract_definition(
    ROOT / "HighPriest.exe.c",
    "void __thiscall CPlayer::SendMsg(CPlayer *this, CGameObject *sender, int message, int arg1, int *arg2, int arg3)",
))
assert ref_player_send == exe_player_send, "CPlayer::SendMsg Ref/EXE full body mismatch"
assert "case 103:" in ref_player_send and "this->m_proceedType = PT_TOUCH_SKILL" in ref_player_send
assert "this->m_stateId != 6 && !IsEffectStateBurrow(this->m_effectState)" in ref_player_send
assert "this->m_skillAttackRange = v55" in ref_player_send
assert "this->m_proceedTargetGid = v56" in ref_player_send

ref_mode_send = normalize(extract_definition(
    ROOT / "Ref/Mode.cpp",
    "int __thiscall CMode::SendMsg(CMode *this, int messageId, int val1, int val2, int val3)",
))
exe_mode_send = normalize(extract_definition(
    ROOT / "HighPriest.exe.c",
    "int __thiscall CMode::SendMsg(CMode *this, int messageId, int val1, int val2, int val3)",
))
assert ref_mode_send == exe_mode_send, "CMode::SendMsg Ref/EXE full body mismatch"
assert "messageId == 2" in ref_mode_send and "messageId == 75" in ref_mode_send
assert "150" not in re.findall(r"messageId\s*==\s*(\d+)", ref_mode_send), "message 150 is no longer a source no-op"

mode_source = ROOT / "src/gamemode/GameMode.cpp"
input_route = extract_definition(mode_source, "bool TryRequestPendingSkillFromScreenPoint(")
pump_route = extract_definition(mode_source, "void PumpSkillChaseRequest(")
clear_route = extract_definition(mode_source, "void ClearSkillChase(")
world_header = (ROOT / "src/world/World.h").read_text(encoding="utf-8", errors="replace")
world_source = extract_definition(ROOT / "src/world/World.cpp", "bool CWorld::FindHoveredSkillScreen(")
touch_pump = pump_route[
    pump_route.index("if (mode.m_hasTouchSkillChase)"):
    pump_route.index("if (!mode.m_hasSkillChase || mode.m_skillChaseTargetGid == 0)")
]
assert "bool FindHoveredSkillScreen(" in world_header
assert "mode.m_world->FindHoveredSkillScreen" in input_route
assert "skillUseMode == ShortcutSkillUseMode::SkillTarget" in input_route
assert "hoveredSkill->m_job == 141" in input_route
assert "hoveredSkill->m_job == 143" in input_route
assert "hoveredSkill->m_job == 152" in input_route
assert "skillId >= 215 && skillId <= 218" in input_route
assert "BeginTouchSkillChase(mode, hoveredSkill->m_aid, skillId, skillLevel, skillRange)" in input_route
assert "skill->m_isVisible" in world_source and "ProjectPoint(g_renderer, viewMatrix, skill->m_pos" in world_source
assert "mode.m_touchSkillTargetAid = 0;" in clear_route

ordered = (
    "if (mode.m_hasTouchSkillChase)",
    "mode.m_world->GetSkillByAID(targetAid)",
    "mode.GetServerCoor(target->m_pos.x, target->m_pos.z, skillTileX, skillTileY)",
    "mode.GetClientCoor(skillTileX, skillTileY, targetWorldX, targetWorldZ)",
    "static_cast<double>(5 * skillRange) * normalizedDirectionX",
    "kTouchRefNeighborOrder",
    "player->m_stateId >= 2 && player->m_stateId != 4 && player->m_stateId < 16",
    "distance <= static_cast<double>(5 * skillRange) + 5.5 || destinationIsPlayerCell",
    "if (specialSkillUnitJob && skillId == 0)",
    "SendUseSkillToIdPacket(skillId, skillLevel, targetAid)",
    "player->m_destCellX == approachTileX && player->m_destCellZ == approachTileY",
    "player->m_stateId == 2 && !specialSkillUnitJob",
    "player->m_destCellX = approachTileX",
    "timeGetTime() > player->m_moveReqTick + 1000",
    "SendMoveRequestPacket(approachTileX, approachTileY)",
)
positions = [touch_pump.find(token) for token in ordered]
assert all(position >= 0 for position in positions), "missing current PT_TOUCH_SKILL route operation"
assert positions == sorted(positions), "current PT_TOUCH_SKILL operation order changed"
assert "ClearSkillChase(mode);" in touch_pump
special_zero_branch = touch_pump[
    touch_pump.index("if (specialSkillUnitJob && skillId == 0)"):
    touch_pump.index("} else {", touch_pump.index("if (specialSkillUnitJob && skillId == 0)"))
]
assert "return;" in special_zero_branch and "SendAttackRequestPacket" not in special_zero_branch
assert "{{1, 0}}, {{1, 1}}, {{1, -1}}, {{-1, 0}}, {{-1, 1}}, {{-1, -1}}, {{0, 1}}, {{0, -1}}" in normalize(touch_pump)

print("PT_TOUCH_SKILL source branch, skill-unit click entry and current persistent modern route anchors/order pass.")
