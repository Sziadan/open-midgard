#!/usr/bin/env python3
"""Source comparison gate for CPlayer::SetState and its pre-engage route."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
REF = (ROOT / "Ref/Player.cpp").read_text(encoding="utf-8", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
IMPL = (ROOT / "src/world/GameActor.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "src/world/GameActor.h").read_text(encoding="utf-8")
MODE = (ROOT / "Ref/Mode.cpp").read_text(encoding="utf-8", errors="replace")


def body(text: str, marker: str) -> tuple[str, str]:
    start = text.rfind(marker)
    assert start >= 0, f"function not found: {marker}"
    opening = text.find("{", start)
    semicolon = text.find(";", start)
    assert opening >= 0 and (semicolon < 0 or opening < semicolon), f"definition not found: {marker}"
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[start:opening].strip(), text[opening + 1:pos]
    raise AssertionError(f"unclosed function: {marker}")


def normalized(value: str) -> str:
    return re.sub(r"\s+", " ", value).strip()


marker = "void __thiscall CPlayer::SetState(CPlayer *this, int stateId)"
ref_sig, ref = body(REF, marker)
exe_sig, exe = body(EXE, marker)
assert normalized(ref_sig) == normalized(exe_sig), "CPlayer::SetState Ref/EXE signatures differ"
assert normalized(ref) == normalized(exe), "complete CPlayer::SetState Ref/EXE bodies differ"
print(f"PASS complete Ref/EXE body equality ({len(normalized(ref))} normalized characters)")

source_requirements = [
    r"m_stateId\s*=\s*this->m_stateId[\s\S]*?if\s*\(\s*m_stateId\s*!=\s*3\s*\)",
    r"if\s*\(\s*stateId\s*!=\s*1\s*\)[\s\S]*?m_destCellX\s*=\s*-1[\s\S]*?m_destCellZ\s*=\s*-1",
    r"m_isPreengageStateOfMove\s*&&\s*\(\s*m_stateId\s*==\s*2\s*\|\|\s*m_stateId\s*==\s*5\s*\|\|\s*m_stateId\s*==\s*6\s*\)[\s\S]*?m_preengageXOfMove\s*=\s*-1[\s\S]*?m_preengageYOfMove\s*=\s*-1[\s\S]*?m_isPreengageStateOfMove\s*=\s*0[\s\S]*?SendMsg\(this,\s*0,\s*91",
    r"if\s*\(\s*stateId\s*==\s*3\s*\)[\s\S]*?CMode::SendMsg\(GameMode,\s*5,\s*0,\s*0,\s*0\)[\s\S]*?PlaceEffect\(GameMode,\s*&this->m_pos,\s*372,\s*300,\s*0\)[\s\S]*?v4\s*=\s*3[\s\S]*?g_se_go\s*=\s*0",
    r"CPc::SetState\(this,\s*v4\)",
]
for pattern in source_requirements:
    assert re.search(pattern, ref), f"Ref CPlayer::SetState source branch missing: {pattern}"
    assert re.search(pattern, exe), f"EXE CPlayer::SetState source branch missing: {pattern}"
print("PASS source branch order: reject previous state 3; clear non-move destinations; cancel eligible pre-engage; place effect 372 on state 3; delegate to CPc")

# Message 91's movement-ack updates are a called dependency, not an old UI.
send_marker = "void __thiscall CPlayer::SendMsg(CPlayer *this, CGameObject *sender, int message, int arg1, int *arg2, int arg3)"
_, ref_send = body(REF, send_marker)
_, exe_send = body(EXE, send_marker)
assert normalized(ref_send) == normalized(exe_send), "CPlayer::SendMsg Ref/EXE bodies differ"
send91 = re.search(r"case\s+91\s*:(.*?)(?=\n\s*case\s+\d+\s*:|\n\s*default\s*:)", ref_send, re.S)
assert send91, "CPlayer::SendMsg message 91 missing"
assert re.search(r"g_checkBOT\s*=\s*0[\s\S]*?if\s*\(\s*!this->m_isWaitingMoveAck\s*\|\|\s*timeGetTime\(\)\s*>\s*this->m_moveReqTick\s*\+\s*1000\s*\)[\s\S]*?m_isWaitingMoveAck\s*=\s*1[\s\S]*?m_moveReqTick\s*=\s*v19[\s\S]*?CMode::SendMsg\(GameMode,\s*151", send91.group(1)), "source message-91 movement acknowledgment route changed"
mode_body = body(MODE, "int __thiscall CMode::SendMsg(CMode *this, int messageId, int val1, int val2, int val3)")[1]
mode_exe = body(EXE, "int __thiscall CMode::SendMsg(CMode *this, int messageId, int val1, int val2, int val3)")[1]
assert normalized(mode_body) == normalized(mode_exe), "complete CMode::SendMsg Ref/EXE bodies differ"
assert not re.search(r"messageId\s*==\s*(?:5|151)\b|case\s+(?:5|151)\s*:", mode_body), "CMode message 5/151 is no longer a source no-op"
print("PASS called dependency: message 91 only updates gated move-ack fields and sends no-op base CMode message 151")

impl_marker = "void CPlayer::SetState(int state)"
assert impl_marker in IMPL and re.search(r"void SetState\(int state\) override;", HEADER), "current CPlayer::SetState override missing"
_, impl = body(IMPL, impl_marker)
impl_requirements = [
    r"previousState\s*==\s*3[\s\S]*?return",
    r"state\s*!=\s*1[\s\S]*?m_destCellX\s*=\s*-1[\s\S]*?m_destCellZ\s*=\s*-1",
    r"m_isPreengageStateOfMove[\s\S]*?previousState\s*==\s*2\s*\|\|\s*previousState\s*==\s*5\s*\|\|\s*previousState\s*==\s*6[\s\S]*?m_preengageXOfMove\s*=\s*-1[\s\S]*?m_preengageYOfMove\s*=\s*-1[\s\S]*?m_isPreengageStateOfMove\s*=\s*0[\s\S]*?!m_isWaitingMoveAck\s*\|\|\s*timeGetTime\(\)\s*>\s*m_moveReqTick\s*\+\s*1000[\s\S]*?m_isWaitingMoveAck\s*=\s*1[\s\S]*?m_moveReqTick\s*=\s*timeGetTime\(\)",
    r"state\s*==\s*3[\s\S]*?PlaceEffect\(m_pos,\s*372,\s*300,\s*0\)[\s\S]*?nextState\s*=\s*3",
    r"CPc::SetState\(nextState\)",
]
cursor = 0
for pattern in impl_requirements:
    match = re.search(pattern, impl[cursor:])
    assert match, f"current CPlayer::SetState branch missing or out of order: {pattern}"
    cursor += match.end()
assert "g_checkBOT" not in impl and not re.search(r"\bg_se_go\s*=", impl), "anti-cheat or legacy full-screen post-process state entered gameplay port"
assert "SendMsg(GameMode, 5" not in impl and "SendMsg(151" not in impl, "source-confirmed no-op base mode messages should not acquire fabricated behavior"
print("PASS current implementation: source-ordered branch guards, move-ack gate, effect 372 launch, and CPc delegation")
print("Scope exclusions: anti-cheat g_checkBOT and legacy renderer full-screen g_se_go; CMode messages 5/151 are source no-ops")
