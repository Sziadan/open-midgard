#!/usr/bin/env python3
"""Compare actor message 118 immediate chat-prohibition effect removal to source."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
REF = (ROOT / "Ref/GameActor.cpp").read_text(encoding="utf-8", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF_WORLD = (ROOT / "Ref/GameWorld.cpp").read_text(encoding="utf-8", errors="replace")
WORLD = (ROOT / "src/world/World.cpp").read_text(encoding="utf-8")
ACTOR = (ROOT / "src/world/GameActor.cpp").read_text(encoding="utf-8")


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
    raise AssertionError(f"unclosed body for {marker}")


def norm(value: str) -> str:
    return re.sub(r"\s+", " ", value)


def actor_case(function: str, case_id: int) -> str:
    switch = re.search(r"switch\s*\(\s*(?:message|msg)\s*\)\s*\{", function)
    assert switch
    depth, labels = 1, []
    for match in re.finditer(r"[^\n]*(?:\n|$)", function[switch.end():]):
        line = match.group(0)
        if depth == 1:
            label = re.match(r"\s*case\s+(\d+)\s*:", line)
            if label:
                labels.append((int(label.group(1)), switch.end() + match.start(), switch.end() + match.end()))
        depth += line.count("{") - line.count("}")
        if depth == 0:
            break
    for ix, (label, _, end) in enumerate(labels):
        if label == case_id:
            return function[end:(labels[ix + 1][1] if ix + 1 < len(labels) else len(function))]
    raise AssertionError(f"message {case_id} missing")


ref_send_marker = "void __thiscall CGameActor::SendMsg(CGameActor *this, int sender, int message, int arg1, float arg2, int arg3)\n{"
exe_send_marker = ref_send_marker
ref_send, exe_send = body(REF, ref_send_marker), body(EXE, exe_send_marker)
assert norm(ref_send) == norm(exe_send), "complete Ref/EXE CGameActor::SendMsg differs"
ref118, exe118 = actor_case(ref_send, 118), actor_case(exe_send, 118)
assert norm(ref118) == norm(exe118), "message 118 Ref/EXE branches differ"
assert "m_chatProhibitEffect" in ref118 and "RemoveGameObject" in ref118

src_send = body(ACTOR, "void CGameActor::SendMsg(CGameObject* src, int msg, msgparam_t par1, msgparam_t par2, msgparam_t par3)\n{")
src118 = actor_case(src_send, 118)
assert "m_chatProhibitEffect" in src118 and "RemoveGameObject(m_chatProhibitEffect)" in src118
assert "ReleaseMsgSprite" not in src118, "message 118 must remove immediately, not defer via disappear state"

world_marker = "void __thiscall CWorld::RemoveGameObject(CWorld *this, CGameObject *object)\n{"
ref_remove = body(REF_WORLD, world_marker)
exe_remove = body(EXE, world_marker)
assert norm(ref_remove) == norm(exe_remove), "complete Ref/EXE CWorld::RemoveGameObject differs"
assert "m_gameObjectList" in ref_remove and "object" in ref_remove
src_remove = norm(body(WORLD, "bool CWorld::RemoveGameObject(CGameObject* object)\n{"))
for anchor in ("std::find(m_gameObjectList.begin(), m_gameObjectList.end(), object)", "m_gameObjectList.erase(it)", "delete object"):
    assert anchor in src_remove, f"current immediate removal missing {anchor}"
assert "m_skillList.remove(skill)" in src_remove
assert "m_chatProhibitEffect == effect" in ACTOR, "effect destructor owner cleanup must clear the actor slot"

print("message 118: complete Ref/EXE actor and RemoveGameObject bodies match; current route removes the exact object immediately and destructor clears owner slot")
