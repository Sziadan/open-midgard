#!/usr/bin/env python3
"""Compare actor message 0 path scheduling and its source FindPath helper."""
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
            end = labels[index + 1][1] if index + 1 < len(labels) else len(function)
            return function[end_label:end]
    raise AssertionError(f"case {case_id} missing")


actor_marker = "void __thiscall CGameActor::SendMsg(CGameActor *this, int sender, int message, int arg1, float arg2, int arg3)\n{"
ref_actor = body(REF, actor_marker)
exe_actor = body(EXE, actor_marker)
impl_actor = body(IMPL, "void CGameActor::SendMsg(CGameObject* src, int msg, msgparam_t par1, msgparam_t par2, msgparam_t par3)\n{")
assert re.sub(r"\s+", " ", ref_actor) == re.sub(r"\s+", " ", exe_actor), "Ref/EXE SendMsg bodies differ"
ref_case = case(ref_actor, 0)
exe_case = case(exe_actor, 0)
impl_case = case(impl_actor, 0)
assert re.sub(r"\s+", " ", ref_case) == re.sub(r"\s+", " ", exe_case), "message 0 Ref/EXE branches differ"

find_path_marker = "unsigned __int8 __thiscall CGameActor::FindPath(\n        CGameActor *this,"
ref_find_path = body(REF, find_path_marker)
exe_find_path = body(EXE, find_path_marker)
assert re.sub(r"\s+", " ", ref_find_path) == re.sub(r"\s+", " ", exe_find_path), "Ref/EXE FindPath helper differs"
normalized_find_path = re.sub(r"\s+", "", exe_find_path)
assert "CPathFinder::FindPath(&g_pathFinder,this->m_moveStartTime,mssx,mssy,mscellX,mscellY,msdx,msdy,msspeed,&this->m_path)" in normalized_find_path
assert "if(lastCellTime>ServerTime){returnv14;" in normalized_find_path
assert "this->m_moveStartTime=ServerTime;returnCPathFinder::FindPath(&g_pathFinder,ServerTime,mssx,mssy,mscellX,mscellY,msdx,msdy,10*msspeed/13,p_m_path)" in normalized_find_path

# Source branch drives an initial wall-clock path, then the helper may reschedule it.
assert re.search(r"Time = timeGetTime\(\);\s*v166 = \(int \*\)arg1;\s*this->m_moveStartTime = Time;", exe_case)
assert re.search(r"FindPath\(this, v166\[1\], v166\[2\], v166\[3\], v166\[4\], v166\[5\], v166\[6\], this->m_speed\)", exe_case)
assert re.search(r"this->m_moveDestX = v166\[5\];[\s\S]*?this->m_moveDestY = v162;[\s\S]*?SetState\(this, 1\);\s*this->m_pathStartCell = 0;", exe_actor)
assert re.search(r"LABEL_391:[\s\S]*?SetState\(this, 0\);", exe_actor)

# Current branch maps the seven payload words, both path attempts, and both exits in source order.
for path_index in range(1, 7):
    assert f"pathArgs[{path_index}]" in impl_case, f"path payload index {path_index} missing"
assert "m_moveStartTime = timeGetTime();" in impl_case
assert re.search(r"findPath\(m_moveStartTime, static_cast<int>\(m_speed\)\)", impl_case)
assert re.search(r"lastCellTime\s*<=\s*serverTime", impl_case)
assert re.search(r"m_moveStartTime = serverTime;\s*foundPath = findPath\(serverTime, 10 \* static_cast<int>\(m_speed\) / 13\);", impl_case)
assert re.search(r"if \(foundPath\)\s*\{\s*m_moveDestX = pathArgs\[5\];\s*m_moveDestY = pathArgs\[6\];\s*SetState\(1\);\s*m_pathStartCell = 0;\s*\} else \{\s*SetState\(0\);", impl_case)
assert "m_isNeverAnimation" not in impl_case and "m_isMotionFreezed" not in impl_case
print("PASS message 0 full path fields, time comparison/replan, destination, and success/failure exits")
