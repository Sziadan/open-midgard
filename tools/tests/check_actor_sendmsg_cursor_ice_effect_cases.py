#!/usr/bin/env python3
"""Compare actor cursor and ice-effect message routes against Ref and the PE decompile."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
REF_ACTOR = (ROOT / "Ref/GameActor.cpp").read_text(encoding="utf-8", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF_MODE = (ROOT / "Ref/GameMode.cpp").read_text(encoding="utf-8", errors="replace")
REF_SKILL = (ROOT / "Ref/Skill.cpp").read_text(encoding="utf-8", errors="replace")
ACTOR = (ROOT / "src/world/GameActor.cpp").read_text(encoding="utf-8")
ACTOR_H = (ROOT / "src/world/GameActor.h").read_text(encoding="utf-8")
MSG = (ROOT / "src/world/MsgEffect.cpp").read_text(encoding="utf-8")


def body(text: str, marker: str) -> str:
    start = text.index(marker)
    opening = text.index("{", start)
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{": depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0: return text[opening + 1:pos]
    raise AssertionError(f"unclosed function: {marker}")


def norm(value: str) -> str:
    return re.sub(r"\s+", " ", value)


def case_branch(function: str, switch_expr: str, case_id: int) -> str:
    switch = re.search(rf"switch\s*\(\s*{re.escape(switch_expr)}\s*\)\s*\{{", function)
    assert switch, f"switch({switch_expr}) missing"
    labels, depth = [], 1
    for match in re.finditer(r"[^\n]*(?:\n|$)", function[switch.end():]):
        line = match.group(0)
        if depth == 1:
            label = re.match(r"\s*case\s+(?:0x([\da-fA-F]+)|(\d+))[uUlL]*\s*:", line)
            if label:
                number = int(label.group(1), 16) if label.group(1) else int(label.group(2))
                labels.append((number, switch.end() + match.start(), switch.end() + match.end()))
        depth += line.count("{") - line.count("}")
        if depth == 0: break
    for ix, (label, _, end) in enumerate(labels):
        if label == case_id:
            return function[end:(labels[ix + 1][1] if ix + 1 < len(labels) else len(function))]
    raise AssertionError(f"case {case_id} missing in switch({switch_expr})")


actor_marker = "void __thiscall CGameActor::SendMsg(CGameActor *this, int sender, int message, int arg1, float arg2, int arg3)\n{"
ref_actor, exe_actor = body(REF_ACTOR, actor_marker), body(EXE, actor_marker)
assert norm(ref_actor) == norm(exe_actor), "complete Ref/EXE CGameActor::SendMsg differs"
actor_source = body(ACTOR, "void CGameActor::SendMsg(CGameObject* src, int msg, msgparam_t par1, msgparam_t par2, msgparam_t par3)\n{")
for message in (135, 136, 139):
    assert norm(case_branch(ref_actor, "message", message)) == norm(case_branch(exe_actor, "message", message)), f"actor message {message} Ref/EXE differs"

ref_factory_marker = "CMsgEffect *__thiscall CGameMode::MakeMsgEffect(\n        CGameMode *this,\n        CMsgEffect *EffectMakeType,\n        vector3d *sPos,\n        unsigned int masterGid)\n{"
ref_factory, exe_factory = body(REF_MODE, ref_factory_marker), body(EXE, ref_factory_marker)
assert norm(ref_factory) == norm(exe_factory), "complete Ref/EXE CGameMode::MakeMsgEffect differs"
factory31 = norm(case_branch(ref_factory, "(unsigned int)EffectMakeType", 31))
assert "aCursorsSpr" in factory31 and "aCursorsAct" in factory31
assert 'char aCursorsSpr[] = "cursors.spr"' in EXE
assert 'char aCursorsAct[] = "cursors.act"' in EXE
assert "v5 = 10" in factory31 and "Ptr = 26" in factory31
assert "v50->CGameObject::SendMsg(v50, 0, 22, masterGid, Ptr, 0)" in norm(ref_factory)
assert "v50->CGameObject::SendMsg(v50, 0, 14, (int)sPos, (int)&sprActName, v5)" in norm(ref_factory)

ref_effect_marker = "void __thiscall CMsgEffect::SendMsg(\n        CMsgEffect *this,\n        CGameActor *sender,\n        int message,\n        float *arg1,\n        unsigned int arg2,\n        int arg3)\n{"
ref_effect, exe_effect = body(REF_SKILL, ref_effect_marker), body(EXE, ref_effect_marker)
assert norm(ref_effect) == norm(exe_effect), "complete Ref/EXE CMsgEffect::SendMsg differs"
branch139 = norm(case_branch(ref_effect, "message", 139))
assert "this->m_masterActor = sender" in branch139
assert branch139.index("rand()") < branch139.rindex("rand()")
assert "m_pos.x" in branch139 and "m_pos.z" in branch139 and "m_pos.y" not in branch139

cursor135 = norm(case_branch(actor_source, "msg", 135))
cursor136 = norm(case_branch(actor_source, "msg", 136))
assert "if (!m_cursorEffect)" in cursor135 and "SpawnMsgSprite(26" in cursor135
assert "m_cursorEffect->m_pos.y -= 20.0f" in cursor135
assert "m_cursorEffect->m_orgPos = m_cursorEffect->m_pos" in cursor135
for part in ('"cursors.spr"', '"cursors.act"'):
    assert part in cursor135, f"canonical cursor resource path missing: {part}"
assert re.search(r"SpawnMsgSprite\(26,\s*\"cursors\.spr\",\s*\"cursors\.act\",\s*10,\s*1\.0f\)", cursor135)
assert cursor135.index("1.0f);") < cursor135.index("if (m_cursorEffect)")
assert "m_cursorEffect->SendMsg(this, 53, 0, 0, 0)" in cursor136
assert cursor136.index("SendMsg(this, 53") < cursor136.index("m_cursorEffect = nullptr")
assert "CMsgEffect* m_cursorEffect = nullptr" in ACTOR_H
assert "if (m_cursorEffect == effect)" in ACTOR

current139 = norm(case_branch(body(MSG, "void CMsgEffect::SendMsg(CGameObject* sender, int msg, msgparam_t par1, msgparam_t par2, msgparam_t par3)\n{"), "msg", 139))
assert "m_masterActor = static_cast<CGameActor*>(sender)" in current139
assert current139.index("m_pos.x =") < current139.index("rand()") < current139.index("m_pos.z =") < current139.rindex("rand()")
assert current139.count("rand()") == 2 and "0.1 + m_masterActor->m_pos.x" in current139 and "0.1 + m_masterActor->m_pos.z" in current139
assert "m_iceEffect->SendMsg(this, 139, 0, 0, 0)" in norm(case_branch(actor_source, "msg", 139))
assert "m_iceEffect = SpawnMsgSprite(5" in ACTOR

print("messages 135/136/139: complete Ref/EXE actor, factory, and effect sender bodies match; cursor lifecycle, type-31 resources, and ordered ice-jitter route verified")
