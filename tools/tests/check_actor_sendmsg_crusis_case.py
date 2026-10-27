#!/usr/bin/env python3
"""Compare Crusis indicator messages 78/79 across Ref, canonical EXE, and src."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
REF_ACTOR = (ROOT / "Ref/GameActor.cpp").read_text(encoding="utf-8", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF_MODE = (ROOT / "Ref/GameMode.cpp").read_text(encoding="utf-8", errors="replace")
REF_SKILL = (ROOT / "Ref/Skill.cpp").read_text(encoding="utf-8", errors="replace")
IMPL_ACTOR = (ROOT / "src/world/GameActor.cpp").read_text(encoding="utf-8")
IMPL_HEADER = (ROOT / "src/world/GameActor.h").read_text(encoding="utf-8")
IMPL_EFFECT = (ROOT / "src/world/MsgEffect.cpp").read_text(encoding="utf-8")


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


def switch_case(function: str, switch_name: str, case_id: int) -> str:
    switches = list(re.finditer(rf"switch\s*\(\s*{re.escape(switch_name)}\s*\)\s*\{{", function))
    assert switches, f"switch({switch_name}) missing"
    for switch in switches:
        offset = switch.end()
        depth = 1
        labels = []
        for match in re.finditer(r"[^\n]*(?:\n|$)", function[offset:]):
            line = match.group(0)
            if depth == 1:
                label = re.match(r"\s*case\s+(?:0x([0-9A-Fa-f]+)|(\d+))\s*:", line)
                if label:
                    value = int(label.group(1), 16) if label.group(1) else int(label.group(2))
                    labels.append((value, offset + match.start(), offset + match.end()))
            depth += line.count("{") - line.count("}")
            if depth == 0:
                break
        for index, (label, _, end_label) in enumerate(labels):
            if label == case_id:
                return function[end_label:(labels[index + 1][1] if index + 1 < len(labels) else len(function))]
    raise AssertionError(f"case {case_id} missing")


def norm(value: str) -> str:
    return re.sub(r"\s+", " ", value)


actor_marker = "void __thiscall CGameActor::SendMsg(CGameActor *this, int sender, int message, int arg1, float arg2, int arg3)\n{"
ref_send, exe_send = body(REF_ACTOR, actor_marker), body(EXE, actor_marker)
assert norm(ref_send) == norm(exe_send), "Ref/EXE CGameActor::SendMsg bodies differ"
for msg in (78, 79):
    assert norm(switch_case(ref_send, "message", msg)) == norm(switch_case(exe_send, "message", msg)), f"message {msg} source branches differ"
src_send = body(IMPL_ACTOR, "void CGameActor::SendMsg(CGameObject* src, int msg, msgparam_t par1, msgparam_t par2, msgparam_t par3)\n{")
src78, src79 = norm(switch_case(src_send, "msg", 78)), norm(switch_case(src_send, "msg", 79))
assert "if ( !this->m_crusisEffect )" in norm(switch_case(exe_send, "message", 78))
assert "MakeMsgEffect(GameMode, (CMsgEffect *)0xB, &this->m_pos, this->m_gid)" in norm(switch_case(exe_send, "message", 78))
assert "m_crusisEffect" in IMPL_HEADER and "m_crusisEffect = SpawnMsgSprite(12," in src78
assert "msg.spr" in src78 and "msg.act" in src78 and "2," in src78
assert "ReleaseMsgSprite(m_crusisEffect)" in src79
assert not re.search(r"\bcase\s+77\s*:", ref_send) and not re.search(r"\bcase\s+77\s*:", src_send)

factory_marker = "CMsgEffect *__thiscall CGameMode::MakeMsgEffect(\n"
ref_factory, exe_factory = body(REF_MODE, factory_marker), body(EXE, factory_marker)
assert norm(ref_factory) == norm(exe_factory), "Ref/EXE MakeMsgEffect bodies differ"
factory = norm(exe_factory)
type_b = factory[factory.index("case 0xBu:"):factory.index("case 0xCu:")]
assert all(token in type_b for token in ("byte_712290", "byte_712280", "v5 = 2", "Ptr = 12"))
assert "char byte_712290[16]" in EXE and "char byte_712280[16]" in EXE
assert "'m',\n  's',\n  'g',\n  '.',\n  's',\n  'p',\n  'r'" in EXE
assert "'m',\n  's',\n  'g',\n  '.',\n  'a',\n  'c',\n  't'" in EXE

process_marker = "bool __thiscall CMsgEffect::OnProcess(CMsgEffect *this)\n{"
ref_process, exe_process = body(REF_SKILL, process_marker), body(EXE, process_marker)
assert norm(ref_process) == norm(exe_process), "Ref/EXE CMsgEffect::OnProcess bodies differ"
periodic = norm(switch_case(exe_process, "v5", 12))
assert "% 34" in periodic and "SendMsg(this->m_masterActor, 0, 77, 0, 0, 0)" in periodic
src_process = body(IMPL_EFFECT, "u8 CMsgEffect::OnProcess()\n{")
current = norm(switch_case(src_process, "m_msgEffectType", 12))
assert "m_pos = m_masterActor->m_pos" in current and "m_pos.y -= 20.0f" in current
assert "static_cast<int>(stateCount) % 34 == 0" in current
assert "m_masterActor->SendMsg(nullptr, 77, 0, 0, 0)" in current

position_marker = "void __thiscall CMsgEffect::ProcessPosition(CMsgEffect *this)\n{"
ref_pos, exe_pos = body(REF_SKILL, position_marker), body(EXE, position_marker)
assert norm(ref_pos) == norm(exe_pos), "Ref/EXE CMsgEffect::ProcessPosition bodies differ"
assert "this->m_pos.y = v7 - 20.0" in norm(switch_case(exe_pos, "this->m_msgEffectType", 12))
print("messages 78/79: Ref/EXE actor, factory, process, and position source checks passed; current routes verified")
