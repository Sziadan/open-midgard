#!/usr/bin/env python3
"""Compare Ref/EXE delayed player pre-move and its modern route."""
from __future__ import annotations

import hashlib
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def body(path: Path, signature: str) -> str:
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


def normalize(source: str) -> str:
    source = re.sub(r"//[^\n]*", "", source)
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.S)
    return re.sub(r"\s+", " ", source).strip()


ref_onprocess = normalize(body(ROOT / "Ref/Player.cpp", "unsigned __int8 __thiscall CPlayer::OnProcess(CPlayer *this)"))
exe_onprocess = normalize(body(ROOT / "HighPriest.exe.c", "unsigned __int8 __thiscall CPlayer::OnProcess(CPlayer *this)"))
assert ref_onprocess == exe_onprocess, "CPlayer::OnProcess Ref/EXE bodies differ"
print(f"CPlayer::OnProcess Ref/EXE equal: chars={len(ref_onprocess)} sha256={hashlib.sha256(ref_onprocess.encode()).hexdigest()}")

source_order = (
    "if ( this->m_preMoveOn && timeGetTime() >",
    "GetServerCoor(GameMode, this->m_pos.x, this->m_pos.z, &ms.sx, &ms.sy, &ms.cellX, &ms.cellY)",
    "CSession::GetServerTime(&g_session)",
    "ms.startTime = ServerTime",
    "ms.dx = m_destCellX",
    "ms.dy = m_destCellZ",
    "SendMsg(this, 0, 0, (int)&ms, 0, 0)",
    "CPlayer::ProcessProceed(this)",
)
positions = [ref_onprocess.find(token) for token in source_order]
assert all(position >= 0 for position in positions), "missing source pre-move operation"
assert positions == sorted(positions), "source CPlayer::OnProcess pre-move order changed"

ref_send = normalize(body(
    ROOT / "Ref/Player.cpp",
    "void __thiscall CPlayer::SendMsg(CPlayer *this, CGameObject *sender, int message, int arg1, int *arg2, int arg3)",
))
exe_send = normalize(body(
    ROOT / "HighPriest.exe.c",
    "void __thiscall CPlayer::SendMsg(CPlayer *this, CGameObject *sender, int message, int arg1, int *arg2, int arg3)",
))
assert ref_send == exe_send, "CPlayer::SendMsg Ref/EXE bodies differ"
msg0 = ref_send[ref_send.index("case 0:"):ref_send.index("case 8:")]
msg9 = ref_send[ref_send.index("case 9:"):ref_send.index("case 10:")]
for token in (
    "this->m_isWaitingMoveAck = 0",
    "this->m_preMoveOn = 0",
    "this->m_moveStartTime = *(_DWORD *)v36",
    "CGameActor::FindPath(",
    "this->m_moveDestX = *(_DWORD *)(v36 + 20)",
    "this->m_moveDestY = *(_DWORD *)(v36 + 24)",
    "this->m_pathStartCell = 0",
):
    assert token in msg0, f"missing source SendMsg(0) operation: {token}"
for token in (
    "this->m_preMoveOn = 1",
    "Time = timeGetTime()",
    "this->m_preMoveStartTick = Time",
    "this->m_destCellX = v34",
    "this->m_destCellZ = v35",
):
    assert token in msg9, f"missing source SendMsg(9) operation: {token}"

actor_source = ROOT / "src/world/GameActor.cpp"
current_begin = normalize(body(actor_source, "void CPlayer::BeginPreMove(int destCellX, int destCellZ)"))
current_premove = normalize(body(actor_source, "void CPlayer::ProcessPreMove()"))
current_click = normalize(body(ROOT / "src/gamemode/GameMode.cpp", "bool TryRequestMoveFromScreenPoint("))
current_update = (ROOT / "src/gamemode/GameMode.cpp").read_text(encoding="utf-8", errors="replace")
update_start = current_update.index("void CGameMode::OnUpdate()")
update_end = current_update.find("\nvoid CGameMode::", update_start + 10)
update_body = current_update[update_start:update_end if update_end >= 0 else None]

for token in (
    "m_preMoveOn = 1",
    "m_preMoveStartTick = timeGetTime()",
    "m_destCellX = destCellX",
    "m_destCellZ = destCellZ",
):
    assert token in current_begin, f"missing modern SendMsg(9) equivalent: {token}"
assert current_click.index("TryRequestMoveToCell(mode, attrX, attrY)") < current_click.index("localPlayer->BeginPreMove(attrX, attrY)")

current_order = (
    "if (!m_preMoveOn)",
    "g_session.GetAveragePingTime() >> 1",
    "attr->m_width / 2",
    "static_cast<int>(std::floor((mapX - serverX) * 16.0)) % 16",
    "g_session.GetServerTime()",
    "m_isWaitingMoveAck = 0",
    "m_preMoveOn = 0",
    "g_pathFinder.FindPath(moveStartTime,",
    "m_moveDestX = requestedDestX",
    "m_moveDestY = requestedDestY",
    "m_pathStartCell = 0",
)
positions = [current_premove.find(token) for token in current_order]
assert all(position >= 0 for position in positions), f"missing modern pre-move route operation: {[(token, position) for token, position in zip(current_order, positions) if position < 0]}"
assert positions == sorted(positions), f"modern pre-move operation order changed: {list(zip(current_order, positions))}"
assert current_premove.count("g_pathFinder.FindPath(moveStartTime,") == 2
fallback_pos = current_premove.index("getSourceServerCoor(startX, startY, startCellX, startCellY)")
second_path_pos = current_premove.index("g_pathFinder.FindPath(moveStartTime,", current_premove.index("g_pathFinder.FindPath(moveStartTime,") + 1)
assert fallback_pos < second_path_pos < current_premove.index("m_moveDestY = requestedDestY", second_path_pos)
for token in (
    "localPlayer->ProcessPreMove()",
    "m_world->m_player->ProcessState()",
    "PumpSkillChaseRequest(*this)",
):
    assert token in update_body, f"missing modern OnUpdate stage: {token}"
assert update_body.index("localPlayer->ProcessPreMove()") < update_body.index("m_world->m_player->ProcessState()")
assert update_body.index("localPlayer->ProcessPreMove()") < update_body.index("PumpSkillChaseRequest(*this)")

print("The modern ordinary-click route arms and processes the Ref delayed pre-move before skill pursuit.")
