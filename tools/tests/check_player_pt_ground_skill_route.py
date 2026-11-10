#!/usr/bin/env python3
"""Source parity and canonical PT_GROUNDSKILL request-routing gate."""
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
    "(unsigned int)v45 < 2 || v45 == 4 || v45 >= 16",
    "this->CGameObject::SendMsg(this, 0, 66, 0, 0, 0)",
    "goto LABEL_176",
):
    assert token in ref_ground, f"missing canonical ground proceed anchor: {token}"
retire_pos = ref_full.index("LABEL_176:")
retire_send_pos = ref_full.index("this->CGameObject::SendMsg(this, 0, 13, 0, 0, 0)", retire_pos)
dispatch_pos = ref_ground.index("this->CGameObject::SendMsg(this, 0, 66, 0, 0, 0)")
assert dispatch_pos < ref_full.index("goto LABEL_176", dispatch_pos) < retire_pos < retire_send_pos, \
    "canonical ground proceed must pump 66 then retire through 13"

ref_converter = normalize(extract_definition(
    ROOT / "Ref/GameMode.cpp",
    "void __thiscall CGameMode::ConvertToNumCellCloseAndFindNoRedCell(",
))
exe_converter = normalize(extract_definition(
    ROOT / "HighPriest.exe.c",
    "void __thiscall CGameMode::ConvertToNumCellCloseAndFindNoRedCell(",
))
assert ref_converter == exe_converter, "closest-safe-cell Ref/EXE helper mismatch"

actor_source = ROOT / "src/world/GameActor.cpp"
send_msg = normalize(extract_definition(
    actor_source,
    "void CPlayer::SendMsg(CGameObject* sender,int message,msgparam_t arg1,msgparam_t arg2,msgparam_t arg3)",
))
case65 = send_msg[send_msg.index("case 65:"):send_msg.index("case 66:")]
case66 = send_msg[send_msg.index("case 66:"):send_msg.index("case 90:")]
case90 = send_msg[send_msg.index("case 90:"):send_msg.index("case 103:")]
assert "m_stateId!=6" in case65 and "m_effectState&0x0002" in case65
for allowed_burrow_skill in (137, 212, 529, 530):
    assert f"skill=={allowed_burrow_skill}" in case65, f"ground source burrow exception missing {allowed_burrow_skill}"
assert "skill==592" not in case65, "ground source burrow exception list must not include skill 592"
assert "m_proceedType.type=4" in case65 and "m_skillId=skill" in case65
assert "m_gSkillDx=static_cast<int>(arg2)" in case65 and "m_gSkillDy=static_cast<int>(arg3)" in case65
assert "m_skillAttackRange=mode->m_skillUseInfo.attackRange" in case65 and "m_proceedTargetGid=0" in case65
assert "std::array<int,4> values{{m_skillId,m_gSkillDx,m_gSkillDy,m_skillUseLevel}}" in case66
assert "mode->SendMsg(77,reinterpret_cast<msgparam_t>(&values),0,0)" in case66
assert "m_skillId==125" in case66 and "m_skillId==220" in case66, "ground text skills must keep their special input windows"
assert "m_skillUseLevel=static_cast<int>(arg1)" in case90

mode_source = ROOT / "src/gamemode/GameMode.cpp"
sync_route = extract_definition(mode_source, "void CGameMode::SynchronizePlayerSkillProceed(CPlayer* player,bool retire)")
assert "player->m_proceedType.type==4" in sync_route
assert "m_groundSkillId=player->m_skillId" in sync_route
assert "m_groundSkillLevel=player->m_skillUseLevel" in sync_route
assert "m_groundSkillCellX=player->m_gSkillDx" in sync_route and "m_groundSkillCellY=player->m_gSkillDy" in sync_route
assert "m_hasSkillChase=1;m_hasGroundSkillChase=1;m_hasTouchSkillChase=0" in sync_route, \
    "PT4 player state must be mirrored to mode ground proceed state"

dispatch_route = extract_definition(mode_source, "bool DispatchSourceGroundSkill(const std::array<int,4>& values)")
ordered_dispatch = (
    "int level=values[3]",
    "if(!level)",
    "g_session.GetSkillItemBySkillId(values[0])",
    "level=row?row->level:0",
    "packet.SkillId=static_cast<u16>(values[0])",
    "packet.SkillLevel=static_cast<u16>(level)",
    "packet.X=static_cast<u16>(values[1])",
    "packet.Y=static_cast<u16>(values[2])",
)
positions = [dispatch_route.find(token) for token in ordered_dispatch]
assert all(position >= 0 for position in positions) and positions == sorted(positions), \
    "message 77 must preserve raw values and apply normal-skill zero-level fallback before wire narrowing"
assert "GetTemporarySkillItemBySkillId" not in dispatch_route
assert "GetHomunSkillItemBySkillId" not in dispatch_route and "GetMercSkillItemBySkillId" not in dispatch_route
assert "m_isValid" not in dispatch_route, "zero-level fallback must not add a row-validity gate"
assert "serverInRange" not in dispatch_route and "ServerInRange" not in dispatch_route

print("PT_GROUNDSKILL Ref/EXE parity and canonical 65/66/90 -> PT4 -> 77 source contract pass.")
