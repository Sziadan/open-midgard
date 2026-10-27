#!/usr/bin/env python3
"""Compare PK indicator actor messages against Ref, canonical EXE, and current code."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
REF_ACTOR = (ROOT / "Ref/GameActor.cpp").read_text(encoding="utf-8", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF_MODE = (ROOT / "Ref/GameMode.cpp").read_text(encoding="utf-8", errors="replace")
REF_SKILL = (ROOT / "Ref/Skill.cpp").read_text(encoding="utf-8", errors="replace")
IMPL_ACTOR = (ROOT / "src/world/GameActor.cpp").read_text(encoding="utf-8")
IMPL_HEADER = (ROOT / "src/world/GameActor.h").read_text(encoding="utf-8")


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


def case_body(function: str, case_id: int) -> str:
    switch = re.search(r"switch\s*\(\s*(?:message|msg|m_msgEffectType)\s*\)\s*\{", function)
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
            return function[end_label:(labels[index + 1][1] if index + 1 < len(labels) else len(function))]
    raise AssertionError(f"actor message {case_id} missing")


def norm(text: str) -> str:
    return re.sub(r"\s+", " ", text)


ref_send = body(REF_ACTOR, "void __thiscall CGameActor::SendMsg(CGameActor *this, int sender, int message, int arg1, float arg2, int arg3)\n{")
exe_send = body(EXE, "void __thiscall CGameActor::SendMsg(CGameActor *this, int sender, int message, int arg1, float arg2, int arg3)\n{")
impl_send = body(IMPL_ACTOR, "void CGameActor::SendMsg(CGameObject* src, int msg, msgparam_t par1, msgparam_t par2, msgparam_t par3)\n{")
assert norm(ref_send) == norm(exe_send), "Ref/EXE CGameActor::SendMsg bodies differ"
for case_id in (56, 57):
    assert norm(case_body(ref_send, case_id)) == norm(case_body(exe_send, case_id)), f"message {case_id} Ref/EXE branches differ"

case56 = norm(case_body(exe_send, 56))
case57 = norm(case_body(exe_send, 57))
assert "if ( !this->m_pkEffect )" in case56
assert "CGameMode::MakeMsgEffect(GameMode, (CMsgEffect *)2, &this->m_pos, this->m_gid)" in case56
assert case56.index("m_msgEffectList") < case56.rindex("m_pkEffect")
assert "m_pkEffect->CGameObject::SendMsg(m_pkEffect, 0, 53, 0, 0, 0)" in case57
assert case57.index("SendMsg") < case57.index("this->m_pkEffect = 0")

ref_factory = body(REF_MODE, "CMsgEffect *__thiscall CGameMode::MakeMsgEffect(\n")
exe_factory = body(EXE, "CMsgEffect *__thiscall CGameMode::MakeMsgEffect(\n")
assert norm(ref_factory) == norm(exe_factory), "Ref/EXE MakeMsgEffect bodies differ"
factory = norm(exe_factory)
type2 = factory[factory.index("case 2u:"):factory.index("case 3u:")]
assert "byte_712380" in type2 and "v5 = 8" in type2 and "Ptr = 4" in type2
assert "CGameObject::SendMsg(v50, 0, 22, masterGid, Ptr, 0)" in factory
assert "CGameObject::SendMsg(v50, 0, 14, (int)sPos, (int)&sprActName, v5)" in factory

ref_process = body(REF_SKILL, "bool __thiscall CMsgEffect::OnProcess(CMsgEffect *this)\n{")
exe_process = body(EXE, "bool __thiscall CMsgEffect::OnProcess(CMsgEffect *this)\n{")
assert norm(ref_process) == norm(exe_process), "Ref/EXE CMsgEffect::OnProcess bodies differ"
ref_position = body(REF_SKILL, "void __thiscall CMsgEffect::ProcessPosition(CMsgEffect *this)\n{")
exe_position = body(EXE, "void __thiscall CMsgEffect::ProcessPosition(CMsgEffect *this)\n{")
assert norm(ref_position) == norm(exe_position), "Ref/EXE CMsgEffect::ProcessPosition bodies differ"
assert "case 4:" in norm(exe_process) and "this->m_curMotion = (int)(__int64)(motionCnt_4a / this->m_motionSpeed) % CActRes::GetMotionCount(v53, this->m_curAction)" in norm(exe_process)
assert "case 4:" in norm(exe_position) and "this->m_pos.y = this->m_pos.y - 15.0" in norm(exe_position)
ref_effect_send = body(REF_SKILL, "void __thiscall CMsgEffect::SendMsg(\n")
exe_effect_send = body(EXE, "void __thiscall CMsgEffect::SendMsg(\n")
assert norm(ref_effect_send) == norm(exe_effect_send), "Ref/EXE CMsgEffect::SendMsg bodies differ"
assert "SetArgb(this, 127, 255, 0, 0)" in norm(exe_effect_send)

impl56 = norm(case_body(impl_send, 56))
impl57 = norm(case_body(impl_send, 57))
spawn = norm(body(IMPL_ACTOR, "CMsgEffect* CGameActor::SpawnMsgSprite(int processType, const char* spritePath, const char* actPath, int action, float zoom)\n{"))
release = norm(body(IMPL_ACTOR, "void ReleaseMsgSprite(CMsgEffect*& slot)\n{"))
assert "m_pkEffect" in IMPL_HEADER
assert "if (!m_pkEffect)" in impl56 and "SpawnMsgSprite(4," in impl56
assert "emotion.spr" in impl56 and "emotion.act" in impl56 and "8," in impl56
assert "m_colorArgb = 0x7FFF0000u" in impl56
assert "case 4:" in norm((ROOT / "src/world/MsgEffect.cpp").read_text(encoding="utf-8"))
impl_process = body((ROOT / "src/world/MsgEffect.cpp").read_text(encoding="utf-8"), "u8 CMsgEffect::OnProcess()\n{")
impl_process4 = norm(case_body(impl_process, 4))
assert "m_pos = m_masterActor->m_pos" in impl_process4 and "m_pos.y -= 15.0f" in impl_process4
assert "m_spriteMotionIndex = static_cast<int>(stateCount) % motionCount" in impl_process4
assert spawn.index("g_world.m_gameObjectList.push_back(effect)") < spawn.index("m_msgEffectList.push_back(effect)") < spawn.index("SendMsg(this, 50")
assert impl56.index("m_pkEffect = SpawnMsgSprite(4,") >= 0
assert "ReleaseMsgSprite(m_pkEffect)" in impl57
assert "slot->SendMsg(nullptr, 53, 0, 0, 0)" in release and "slot = nullptr" in release
print("message 56/57 and MakeMsgEffect type 2: Ref/EXE source compare and current route checks passed")
