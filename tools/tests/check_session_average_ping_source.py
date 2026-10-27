#!/usr/bin/env python3
"""Compare Ref/EXE rolling ping averaging and its modern time-sync route."""
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


ref_recalc = normalize(extract_definition(
    ROOT / "Ref/Session.cpp",
    "void __thiscall CSession::RecalcAveragePingTime(CSession *this, unsigned int pingTime)",
))
exe_recalc = normalize(extract_definition(
    ROOT / "HighPriest.exe.c",
    "void __thiscall CSession::RecalcAveragePingTime(CSession *this, unsigned int pingTime)",
))
assert ref_recalc == exe_recalc, "RecalcAveragePingTime Ref/EXE bodies differ"
print(f"RecalcAveragePingTime Ref/EXE equal: chars={len(ref_recalc)} sha256={hashlib.sha256(ref_recalc.encode()).hexdigest()}")

for token in (
    "std::list<unsigned long>::insert(",
    "m_pingTimeList._Head",
    "m_pingTimeList._Size > 3",
    "m_pingTimeList._Head->_Next",
    "v5 += Value",
    "m_averagePingTime = v5 / this->m_pingTimeList._Size",
):
    assert token in ref_recalc, f"missing Ref rolling-average operation: {token}"

ref_notify = normalize(extract_definition(
    ROOT / "Ref/GameModePacket.cpp",
    "void __thiscall CGameMode::Zc_Notify_Time(CGameMode *this, const char *buf)",
))
exe_notify = normalize(extract_definition(
    ROOT / "HighPriest.exe.c",
    "void __thiscall CGameMode::Zc_Notify_Time(CGameMode *this, const char *buf)",
))
assert ref_notify == exe_notify, "Zc_Notify_Time Ref/EXE bodies differ"
print(f"Zc_Notify_Time Ref/EXE equal: chars={len(ref_notify)} sha256={hashlib.sha256(ref_notify.encode()).hexdigest()}")

for token in (
    "if ( this->m_numNotifyTime )",
    "CSession::UpdateServerTime(&g_session, *(_DWORD *)(buf + 2))",
    "this->m_receiveSyneRequestTime = 1",
    "this->m_numNotifyTime = m_numNotifyTime + 1",
    "v7 = timeGetTime()",
    "CSession::RecalcAveragePingTime(&g_session, v7 - this->m_syncRequestTime)",
):
    assert token in ref_notify, f"missing Ref notifier operation: {token}"

current_recalc = normalize(extract_definition(
    ROOT / "src/session/Session.cpp",
    "void CSession::RecalcAveragePingTime(u32 pingTime)",
))
assert "m_pingTimeList.push_front(pingTime)" in current_recalc
assert "while (m_pingTimeList.size() > 3)" in current_recalc
assert "m_pingTimeList.pop_back()" in current_recalc
assert "for (const u32 sample : m_pingTimeList)" in current_recalc
assert "m_averagePingTime = totalPingTime / static_cast<u32>(m_pingTimeList.size())" in current_recalc

current_notify = normalize(extract_definition(
    ROOT / "src/gamemode/GameModePacket.cpp",
    "void HandleNotifyTime(CGameMode& mode, const PacketView& packet)",
))
ordered = (
    "if (mode.m_numNotifyTime != 0)",
    "g_session.UpdateServerTime(serverTick)",
    "mode.m_receiveSyneRequestTime = 1",
    "++mode.m_numNotifyTime",
    "const u32 receiveTick = timeGetTime()",
    "g_session.RecalcAveragePingTime(receiveTick - mode.m_syncRequestTime)",
)
positions = [current_notify.find(token) for token in ordered]
assert all(position >= 0 for position in positions), "missing modern time-sync/ping route operation"
assert positions == sorted(positions), "modern time-sync/ping route order changed"
assert "m_averagePingTime(0)" in (ROOT / "src/session/Session.cpp").read_text(encoding="utf-8")
assert "u32 GetAveragePingTime() const" in (ROOT / "src/session/Session.h").read_text(encoding="utf-8")

print("The modern notifier feeds an exact newest-three integer average in source order.")
