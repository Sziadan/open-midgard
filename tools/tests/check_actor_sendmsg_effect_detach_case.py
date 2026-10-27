#!/usr/bin/env python3
"""Compare message 48's owner-list detach with the current CMsgEffect lifecycle."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
REF_ACTOR = (ROOT / "Ref/GameActor.cpp").read_text(encoding="utf-8", errors="replace")
REF_EFFECT = (ROOT / "Ref/Skill.cpp").read_text(encoding="utf-8", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
ACTOR = (ROOT / "src/world/GameActor.cpp").read_text(encoding="utf-8")
EFFECT = (ROOT / "src/world/MsgEffect.cpp").read_text(encoding="utf-8")


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


actor_marker = "void __thiscall CGameActor::SendMsg(CGameActor *this, int sender, int message, int arg1, float arg2, int arg3)\n{"
ref_actor = body(REF_ACTOR, actor_marker)
exe_actor = body(EXE, actor_marker)
assert re.sub(r"\s+", " ", ref_actor) == re.sub(r"\s+", " ", exe_actor), "Ref/EXE SendMsg bodies differ"
assert re.sub(r"\s+", " ", case(ref_actor, 48)) == re.sub(r"\s+", " ", case(exe_actor, 48)), "message 48 branches differ"
source_case = case(exe_actor, 48)
assert re.search(r"if \( Next->_Value == \(CMsgEffect \*\)sender \).*?DeleteMatchingEffect\(this, Next->_Value\).*?operator delete\(Next\).*?--this->m_msgEffectList\._Size", source_case, re.S)

ref_dtor = body(REF_EFFECT, "void __thiscall CMsgEffect::~CMsgEffect(CMsgEffect *this)\n{")
exe_dtor = body(EXE, "void __thiscall CMsgEffect::~CMsgEffect(CMsgEffect *this)\n{")
assert re.sub(r"\s+", " ", ref_dtor) == re.sub(r"\s+", " ", exe_dtor), "Ref/EXE effect destructor bodies differ"
assert re.search(r"m_masterActor->CGameObject::SendMsg\(m_masterActor, this, 48, 0, 0, 0\)", exe_dtor)

current_dtor = body(EFFECT, "CMsgEffect::~CMsgEffect()\n{")
current_delete = body(ACTOR, "void CGameActor::DeleteMatchingEffect(CMsgEffect* effect)\n{")
assert re.search(r"if \(!m_removedFromOwner && m_masterActor\)\s*\{\s*m_masterActor->DeleteMatchingEffect\(this\);", current_dtor)
assert re.search(r"m_msgEffectList\.erase\(it\)", current_delete)
assert "m_removedFromOwner" in current_dtor, "current lifecycle must suppress duplicate owner detachment"
print("PASS message 48 owner-list detach is represented by current CMsgEffect destruction and owner cleanup")
