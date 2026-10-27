#!/usr/bin/env python3
"""Compare actor message 61 position update to the canonical source."""
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


def branch(function: str, case_id: int) -> str:
    switch = re.search(r"switch\s*\(\s*(?:message|msg)\s*\)\s*\{", function)
    assert switch, "message switch missing"
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
            end = labels[index + 1][1] if index + 1 < len(labels) else len(function)
            return function[end_label:end]
    raise AssertionError(f"case {case_id} missing")


marker = "void __thiscall CGameActor::SendMsg(CGameActor *this, int sender, int message, int arg1, float arg2, int arg3)\n{"
ref = body(REF, marker)
exe = body(EXE, marker)
impl = body(IMPL, "void CGameActor::SendMsg(CGameObject* src, int msg, msgparam_t par1, msgparam_t par2, msgparam_t par3)\n{")
assert re.sub(r"\s+", " ", ref) == re.sub(r"\s+", " ", exe), "Ref and executable SendMsg bodies differ"
print("PASS full Ref/executable SendMsg body equality")

source = branch(exe, 61)
current = branch(impl, 61)
assert re.search(r"v185 = arg1;\s*v186 = LODWORD\(arg2\);\s*this->m_moveDestX = arg1;\s*this->m_moveDestY = v186;\s*CGameMode::GetClientCoor\(GameMode, v185, v186, &this->m_pos\.x, &this->m_pos\.z\);", source)
assert re.search(r"m_moveDestX = cellX;\s*m_moveDestY = cellY;\s*if \(CGameMode\* gameMode = g_modeMgr\.GetCurrentGameMode\(\)\)\s*\{\s*gameMode->GetClientCoor\(cellX, cellY, m_pos\.x, m_pos\.z\);", current)
assert not re.search(r"SetState|StartMoveToCell|FindPath", current)
print("PASS message 61 stores cell and updates client X/Z without starting motion")
