#!/usr/bin/env python3
"""Source compare actor emotion message and its MakeEmotion/OnProcess behavior."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
REF_ACTOR = (ROOT / "Ref/GameActor.cpp").read_text(encoding="utf-8", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF_MODE = (ROOT / "Ref/GameMode.cpp").read_text(encoding="utf-8", errors="replace")
REF_SKILL = (ROOT / "Ref/Skill.cpp").read_text(encoding="utf-8", errors="replace")
IMPL_ACTOR = (ROOT / "src/world/GameActor.cpp").read_text(encoding="utf-8")
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


def actor_case(function: str, case_id: int) -> str:
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
            return function[end_label:(labels[index + 1][1] if index + 1 < len(labels) else len(function))]
    raise AssertionError(f"actor message {case_id} missing")


def normalized(value: str) -> str:
    return re.sub(r"\s+", " ", value)


send_marker = "void __thiscall CGameActor::SendMsg(CGameActor *this, int sender, int message, int arg1, float arg2, int arg3)\n{"
ref_send = body(REF_ACTOR, send_marker)
exe_send = body(EXE, send_marker)
impl_send = body(IMPL_ACTOR, "void CGameActor::SendMsg(CGameObject* src, int msg, msgparam_t par1, msgparam_t par2, msgparam_t par3)\n{")
assert normalized(ref_send) == normalized(exe_send), "Ref/EXE CGameActor::SendMsg bodies differ"
ref_emotion = actor_case(ref_send, 21)
exe_emotion = actor_case(exe_send, 21)
impl_emotion = actor_case(impl_send, 21)
assert normalized(ref_emotion) == normalized(exe_emotion), "message 21 Ref/EXE branches differ"
assert "arg1 == 34" in normalized(exe_emotion)
assert "CGameMode::MakeEmotion(GameMode, &this->m_pos, 34, this->m_gid)" in normalized(exe_emotion)
assert "CGameMode::MakeEmotion(GameMode, &this->m_pos, arg1, this->m_gid)" in normalized(exe_emotion)
assert "m_chatProhibitEffect" in impl_emotion and "emotion == 34" in impl_emotion
assert "g_world.m_gameObjectList.push_back(effect)" in impl_emotion
assert "m_msgEffectList.push_back(effect)" in impl_emotion
assert "effect->SendMsg(this, 50, 0, 0, 0)" in impl_emotion

ref_make = body(REF_MODE, "CMsgEffect *__thiscall CGameMode::MakeEmotion(")
exe_make = body(EXE, "CMsgEffect *__thiscall CGameMode::MakeEmotion(CGameMode *this, vector3d *sPos, int emotionType, unsigned int masterGid)\n{")
assert normalized(ref_make) == normalized(exe_make), "Ref/EXE MakeEmotion bodies differ"
assert "case 34:" in normalized(exe_make) and "CGameObject::SendMsg(emotionTypea, 0, 14" in normalized(exe_make)
assert "case 34:" in normalized(exe_make) and "case 80:" in normalized(exe_make)
source_actions = {
    int(emotion): int(action)
    for emotion, action in re.findall(r"case\s+(\d+)\s*:\s*v8\s*=\s*(\d+)\s*;\s*goto LABEL_106", exe_make)
}
assert len(source_actions) == 79 and 34 not in source_actions, f"unexpected source emotion action map size: {len(source_actions)}"
# Mapping moved to the shared helper; retain the independently derived PE map.
emotion_source = (ROOT / "src/world/Emotion.h").read_text(encoding="utf-8")
emotion_helper = body(emotion_source, "constexpr int EmotionAction(int id)")
assert "int action = EmotionAction(emotion);" in impl_emotion
first_match = re.search(r"first\[\]\s*=\s*\{([^}]+)\}", emotion_helper)
assert first_match, "shared emotion prefix map missing"
first = [int(value.strip()) for value in first_match.group(1).split(",")]
assert len(first) == 15
assert "if (id < 0 || id > 80) return -1;" in emotion_helper
assert "if (id < 15) return first[id];" in emotion_helper
assert "if (id == 34) return 10;" in emotion_helper
assert "return id < 34 ? id : id - 1;" in emotion_helper
for emotion, expected in source_actions.items():
    current = first[emotion] if emotion < 15 else emotion if emotion < 34 else emotion - 1
    assert current == expected, f"emotion {emotion}: source action {expected}, current action {current}"
assert "if (action < 0) action = 0;" in impl_emotion, "PE MakeEmotion default must retain action-zero fallback"
assert "default: v8 = 0;" in normalized(exe_make)
# PE MakeEmotion loads the complete SPR string at0x6F2CE0;0x6F2CE6
# is only its suffix. ACT is at0x712380. The normal emotion uses emotion.spr.
assert "emotion.act" in impl_emotion and "emotion.spr" in impl_emotion
assert "effect->m_pos.y -= 20.0f" in impl_emotion
assert "m_spriteActionIndex = chatProhibit ? 10 : action" in impl_emotion
assert "m_spriteMotionIndex = chatProhibit ? 15 : 0" in impl_emotion

ref_process = body(REF_SKILL, "bool __thiscall CMsgEffect::OnProcess(CMsgEffect *this)\n{")
exe_process = body(EXE, "bool __thiscall CMsgEffect::OnProcess(CMsgEffect *this)\n{")
assert normalized(ref_process) == normalized(exe_process), "Ref/EXE CMsgEffect::OnProcess bodies differ"
source_process = normalized(exe_process)
assert "case 0: case 1: case 2: case 17:" in source_process
assert "this->m_stateId == 10000" in source_process and "this->m_curMotion = 15" in source_process
assert "if ( v48 / (double)MotionCount < 1.0 ) return 1;" in source_process
assert "this->m_masterGid != g_session.m_aid" in source_process

impl_process = body(IMPL_EFFECT, "u8 CMsgEffect::OnProcess()\n{")
impl_emotion_process = actor_case(impl_process, 1) if re.search(r"switch\s*\(\s*(?:message|msg)\s*\)", impl_process) else ""
# OnProcess switches by effect type; verify its new case directly instead of using the message extractor.
match = re.search(r"case 1:\s*\{([\s\S]*?)\n\s*\}\s*case kAttachedActorSpriteMsgEffectType", impl_process)
assert match, "current emotion effect process branch missing"
impl_process_branch = match.group(1)
assert "m_chatProhibitEffect == this" in impl_process_branch
assert "m_spriteMotionIndex = 15" in impl_process_branch and "m_colorArgb = 0xFFFA5050u" in impl_process_branch
assert "ResolveMsgSpriteMotionCountForEffect(*this, m_spriteActionIndex)" in impl_process_branch
assert "m_isDisappear = 1" in impl_process_branch
print("PASS message 21 Ref/EXE branch, all 79 canonical emotion actions, special 34 persistence, and emotion motion lifecycle mapping")
