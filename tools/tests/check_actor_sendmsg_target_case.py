#!/usr/bin/env python3
"""Compare actor message 81 target-GID storage against Ref and the canonical EXE."""
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
assert re.sub(r"\s+", " ", ref) == re.sub(r"\s+", " ", exe), "Ref and EXE SendMsg function bodies differ"
ref_case, exe_case, impl_case = case(ref, 81), case(exe, 81), case(impl, 81)
assert re.sub(r"\s+", " ", ref_case) == re.sub(r"\s+", " ", exe_case), "message 81 Ref/EXE branches differ"
assert re.search(r"this->m_targetGid\s*=\s*arg1\s*;\s*return\s*;", exe_case)
assert re.search(r"m_targetGid\s*=\s*static_cast<u32>\(par1\)\s*;\s*return\s*;", impl_case)
assert not re.search(r"SetState|StartMoveToCell|LaunchEffect|FindPath", impl_case)
print("PASS message 81 stores target GID and returns without other actor effects")
