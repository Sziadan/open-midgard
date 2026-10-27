#!/usr/bin/env python3
"""Source-to-current branch gate for the consolidated CNpc::SetState route."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
REF = (ROOT / "Ref/Npc.cpp").read_text(encoding="utf-8", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
MODE = (ROOT / "Ref/Mode.cpp").read_text(encoding="utf-8", errors="replace")
IMPL = (ROOT / "src/world/GameActor.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "src/world/GameActor.h").read_text(encoding="utf-8")


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


def cases(source: str, switch_pattern: str) -> tuple[list[str], dict[str, str]]:
    switch = re.search(switch_pattern, source)
    assert switch, f"switch missing: {switch_pattern}"
    start = switch.end()
    depth = 1
    labels: list[tuple[str, int, int]] = []
    end = len(source)
    for line in re.finditer(r"[^\n]*(?:\n|$)", source[start:]):
        value = line.group()
        if depth == 1:
            label = re.match(r"\s*(case\s+[^:]+|default)\s*:", value)
            if label:
                labels.append((label.group(1).strip(), start + line.start(), start + line.end()))
        depth += value.count("{") - value.count("}")
        if depth == 0:
            end = start + line.end()
            break
    sections: dict[str, str] = {}
    for index, (label, _, label_end) in enumerate(labels):
        next_start = labels[index + 1][1] if index + 1 < len(labels) else end
        assert label not in sections, f"duplicate switch label: {label}"
        sections[label] = source[label_end:next_start]
    return [label for label, _, _ in labels], sections


normalize = lambda value: re.sub(r"\s+", " ", value).strip()
source_marker = "void __thiscall CNpc::SetState(CNpc *this, int stateId)"
ref_sig, ref = body(REF, source_marker)
exe_sig, exe = body(EXE, source_marker)
assert normalize(ref_sig) == normalize(exe_sig), "CNpc::SetState Ref/EXE signatures differ"
assert normalize(ref) == normalize(exe), "complete CNpc::SetState Ref/EXE bodies differ"
print(f"PASS complete Ref/EXE body equality ({len(normalize(ref))} normalized characters)")

state_ids, ref_states = cases(ref, r"switch\s*\(\s*stateId\s*\)\s*\{")
exe_state_ids, exe_states = cases(exe, r"switch\s*\(\s*stateId\s*\)\s*\{")
assert state_ids == exe_state_ids == [f"case {value}" for value in (0, 1, 2, 3, 4, 7, 10)] + ["default"], \
    f"Ref/EXE CNpc state label/order mismatch: Ref={state_ids}, EXE={exe_state_ids}"
print(f"PASS mechanically extracted ordered state labels: {state_ids}")

pet_ids, _ = cases(ref, r"switch\s*\(\s*this->m_pkState\s*\)\s*\{")
pet_ids_exe, _ = cases(exe, r"switch\s*\(\s*this->m_pkState\s*\)\s*\{")
assert pet_ids == pet_ids_exe == [f"case {value}" for value in (1, 2, 3, 4)] + ["default"], \
    f"Ref/EXE pet-state labels differ: Ref={pet_ids}, EXE={pet_ids_exe}"
print(f"PASS mechanically extracted pet substates: {pet_ids}")

job_ids, _ = cases(ref, r"switch\s*\(\s*CGameActor::GetJob\(&this->CGameActor\)\s*\)\s*\{")
job_ids_exe, _ = cases(exe, r"switch\s*\(\s*CGameActor::GetJob\(&this->CGameActor\)\s*\)\s*\{")
assert job_ids == job_ids_exe == ["case 45", "case 104", "case 111", "case 139", "default"], \
    f"Ref/EXE special-job route mismatch: Ref={job_ids}, EXE={job_ids_exe}"
print(f"PASS mechanically extracted special jobs: {job_ids}")

source_anchors = {
    "entry state-4 reset and invalid-state return": r"if\s*\(\s*stateId\s*==\s*4\s*\)[\s\S]*?m_stateId\s*==\s*1[\s\S]*?m_stateId\s*=\s*0[\s\S]*?else if\s*\(\s*stateId\s*==\s*-1\s*\)[\s\S]*?return",
    "job 45 dimensions, bounds, once-only effect 321": r"case\s+45\s*:[\s\S]*?m_ySize\s*<=\s*10[\s\S]*?m_xSize\s*<=\s*10[\s\S]*?m_x1\s*=\s*this->m_pos\.x[\s\S]*?m_z1\s*=\s*this->m_pos\.z[\s\S]*?m_x2\s*=[\s\S]*?m_z2\s*=[\s\S]*?!m_doesLaunchEffect\s*&&\s*this->m_isVisibleBody[\s\S]*?m_doesLaunchEffect\s*=\s*1[\s\S]*?LaunchEffect\(&this->CGameActor,\s*321",
    "job 104/111 spatial and dimension mutations": r"case\s+104\s*:[\s\S]*?5\s*\*\s*this->m_ySize[\s\S]*?case\s+111\s*:[\s\S]*?m_xSize\s*\*=\s*2[\s\S]*?m_ySize\s*\*=\s*2",
    "ordinary NPC default idle and registration": r"else\s*\{\s*this->CRenderObject::SetAction\(this,\s*0,\s*4,\s*0\)[\s\S]*?CGameActor::RegisterPos\(this\)",
    "state 0 pet actions and registration": r"m_isLieOnGround\s*=\s*0[\s\S]*?m_shouldAddPickInfo\s*=\s*1[\s\S]*?switch\s*\(\s*this->m_pkState\s*\)[\s\S]*?case\s+1\s*:[\s\S]*?(?:CRenderObject::)?SetAction\(this,\s*40,\s*4,\s*1\)[\s\S]*?case\s+2\s*:[\s\S]*?(?:CRenderObject::)?SetAction\(this,\s*48,\s*4,\s*1\)[\s\S]*?case\s+3\s*:[\s\S]*?(?:CRenderObject::)?SetAction\(this,\s*56,\s*4,\s*1\)[\s\S]*?case\s+4\s*:[\s\S]*?(?:CRenderObject::)?SetAction\(this,\s*64,\s*4,\s*1\)[\s\S]*?m_pkState\s*=\s*0[\s\S]*?RegisterPos",
    "state 1 body freeze gate, job actions and unregister": r"m_bodyState\s*=\s*this->m_bodyState[\s\S]*?m_bodyState\s*!=\s*1\s*&&\s*m_bodyState\s*!=\s*2[\s\S]*?m_isMotionFreezed\s*=\s*0[\s\S]*?GetJob\(&this->CGameActor\)\s*<\s*6017[\s\S]*?(?:CRenderObject::)?SetAction\(this,\s*8,\s*4,\s*0\)[\s\S]*?(?:CRenderObject::)?SetAction\(this,\s*8,\s*6,\s*0\)[\s\S]*?UnRegisterPos",
    "state 2 RNG, job animation, facing, attack timing and effect route": r"v16\s*=\s*this->m_bodyState[\s\S]*?v16\s*!=\s*1\s*&&\s*v16\s*!=\s*2[\s\S]*?(?:CRenderObject::)?SetAction\(this,\s*80,\s*3,\s*1\)[\s\S]*?rand\(\)\s*&\s*0x80000001[\s\S]*?(?:CRenderObject::)?SetAction\(this,\s*48,\s*4,\s*1\)[\s\S]*?(?:CRenderObject::)?SetAction\(this,\s*16,\s*4,\s*1\)[\s\S]*?CalcDir[\s\S]*?GetAttackMotion[\s\S]*?RegisterPos[\s\S]*?MakeSpecialAttackEffect",
    "state 3 clear messages, grounded pose and unregister": r"v24\s*=\s*this->m_bodyState[\s\S]*?m_isForceState\s*=\s*0[\s\S]*?m_isForceState2\s*=\s*0[\s\S]*?m_isForceState3\s*=\s*0[\s\S]*?m_isMotionFreezed\s*=\s*0[\s\S]*?v24\s*==\s*2[\s\S]*?SendMsg\(this,\s*0,\s*59[\s\S]*?SendMsg\(this,\s*0,\s*62[\s\S]*?m_isLieOnGround\s*=\s*1[\s\S]*?m_shouldAddPickInfo\s*=\s*0[\s\S]*?(?:CRenderObject::)?SetAction\(this,\s*32,\s*8,\s*1\)[\s\S]*?(?:CRenderObject::)?SetAction\(this,\s*64,\s*4,\s*1\)[\s\S]*?UnRegisterPos",
    "state 4 forced-state return and conditional hit motion": r"m_isForceState\s*\|\|\s*this->m_isForceState2\s*\|\|\s*this->m_isForceState3[\s\S]*?return[\s\S]*?(?:CRenderObject::)?SetAction\(this,\s*48,\s*5,\s*1\)[\s\S]*?m_curAction\s*<\s*40[\s\S]*?(?:CRenderObject::)?SetAction\(this,\s*24,\s*4,\s*1\)[\s\S]*?ProcessMotion",
    "state 7 object type and job action routes": r"m_objectType\s*==\s*9[\s\S]*?(?:CRenderObject::)?SetAction\(this,\s*16,\s*4,\s*1\)[\s\S]*?GetJob\(&this->CGameActor\)\s*<\s*6001[\s\S]*?GetJob\(&this->CGameActor\)\s*>=\s*6017[\s\S]*?(?:CRenderObject::)?SetAction\(this,\s*96,\s*6,\s*1\)[\s\S]*?(?:CRenderObject::)?SetAction\(this,\s*40,\s*4,\s*1\)",
    "state 10 body messages, frozen completed pose": r"m_bodyState\s*==\s*2[\s\S]*?SendMsg\(this,\s*0,\s*59[\s\S]*?SendMsg\(this,\s*0,\s*62[\s\S]*?m_isLieOnGround\s*=\s*0[\s\S]*?m_isMotionFinished\s*=\s*1[\s\S]*?m_isMotionFreezed\s*=\s*1[\s\S]*?(?:CRenderObject::)?SetAction\(this,\s*0,\s*8,\s*1\)",
    "shared action/state/tick tail": r"m_curAction\s*=\s*this->m_baseAction[\s\S]*?Get8Dir[\s\S]*?if\s*\(\s*stateId\s*!=\s*4\s*\)[\s\S]*?m_stateId\s*=\s*stateId[\s\S]*?m_stateStartTick\s*=\s*timeGetTime\(\)",
}
for label, pattern in source_anchors.items():
    source_scope = ref_states[f"case {label.split()[1]}"] if label.startswith("state ") else ref
    exe_scope = exe_states[f"case {label.split()[1]}"] if label.startswith("state ") else exe
    assert re.search(pattern, source_scope), f"Ref CNpc::SetState branch missing: {label}"
    assert re.search(pattern, exe_scope), f"EXE CNpc::SetState branch missing: {label}"
print(f"PASS source branch inventory ({len(source_anchors)} ordered branch maps, including all source states/jobs)")

mode_marker = "int __thiscall CMode::SendMsg(CMode *this, int messageId, int val1, int val2, int val3)"
mode_ref = body(MODE, mode_marker)[1]
mode_exe = body(EXE, mode_marker)[1]
assert normalize(mode_ref) == normalize(mode_exe), "CMode::SendMsg Ref/EXE bodies differ"
assert not re.search(r"messageId\s*==\s*161\b|case\s+161\s*:", mode_ref), "pet talk mode message 161 is no longer a source no-op"

impl_marker = "void CGameActor::SetNpcState(int state)"
assert impl_marker in IMPL and "void SetNpcState(int state);" in HEADER, "current NPC state handler missing"
_, impl = body(IMPL, impl_marker)
impl_ids, impl_states = cases(impl, r"switch\s*\(\s*state\s*\)\s*\{")
assert impl_ids == [f"case {value}" for value in (0, 1, 2, 3, 4, 7, 10)] + ["default"], \
    f"current NPC state IDs/order differ: {impl_ids}"
impl_pet_ids, _ = cases(impl, r"switch\s*\(\s*m_pkState\s*\)\s*\{")
assert impl_pet_ids == pet_ids, f"current pet animation IDs differ: {impl_pet_ids}"
print(f"PASS current mechanically extracted NPC state IDs: {impl_ids}; pet states: {impl_pet_ids}")

impl_anchors = {
    "job-specific runtime dispatch excludes skill units": (IMPL, r"if\s*\(\s*m_isPc\s*==\s*0\s*&&\s*dynamic_cast<CSkill\*>\(this\)\s*==\s*nullptr\s*\)\s*\{\s*SetNpcState\(state\)\s*;\s*return"),
    "job 45 bounds and once-only effect": (impl, r"m_job\s*==\s*45[\s\S]*?m_ySize\s*<=\s*10[\s\S]*?m_xSize\s*<=\s*10[\s\S]*?m_x1\s*=\s*m_pos\.x[\s\S]*?m_z1\s*=\s*m_pos\.z[\s\S]*?m_x2\s*=\s*m_pos\.x[\s\S]*?m_z2\s*=\s*m_pos\.z[\s\S]*?!m_doesLaunchEffect\s*&&\s*m_isVisibleBody[\s\S]*?m_doesLaunchEffect\s*=\s*1[\s\S]*?LaunchEffect\(321,\s*vector3d\{\},\s*0\.0f\)"),
    "job 104/111 mutations and 139 no-op": (impl, r"m_job\s*==\s*104[\s\S]*?5\s*\*\s*m_ySize[\s\S]*?m_job\s*==\s*111[\s\S]*?m_xSize\s*\*=\s*2[\s\S]*?m_ySize\s*\*=\s*2[\s\S]*?m_job\s*!=\s*139"),
    "state 0 actions and pick/lie fields": (impl_states["case 0"], r"m_isLieOnGround\s*=\s*0[\s\S]*?m_shouldAddPickInfo\s*=\s*1[\s\S]*?SetAction\(0,\s*4,\s*0\)[\s\S]*?switch\s*\(m_pkState\)[\s\S]*?case 1:\s*petAction\s*=\s*40[\s\S]*?case 2:\s*petAction\s*=\s*48[\s\S]*?case 3:\s*petAction\s*=\s*56[\s\S]*?case 4:\s*petAction\s*=\s*64[\s\S]*?m_pkState\s*=\s*0[\s\S]*?m_isMotionFinished\s*=\s*0[\s\S]*?RegisterPos"),
    "state 1 freeze and action ranges": (impl_states["case 1"], r"m_bodyState\s*!=\s*1\s*&&\s*m_bodyState\s*!=\s*2[\s\S]*?m_isMotionFreezed\s*=\s*0[\s\S]*?SetAction\(8,\s*\(m_job\s*>=\s*6017\s*&&\s*m_job\s*<=\s*6046\)\s*\?\s*6\s*:\s*4,\s*0\)[\s\S]*?UnRegisterPos"),
    "state 2 RNG/job animation/facing/effect route": (impl_states["case 2"], r"m_bodyState\s*!=\s*1\s*&&\s*m_bodyState\s*!=\s*2[\s\S]*?rand\(\)\s*&\s*static_cast<int>\(0x80000001u\)[\s\S]*?SetAction\(48,\s*4,\s*1\)[\s\S]*?SetAction\(16,\s*4,\s*1\)[\s\S]*?SetAction\(80,\s*3,\s*1\)[\s\S]*?calcDir\([\s\S]*?GetAttackMotion\(\)[\s\S]*?RegisterPos\(\)[\s\S]*?MakeSpecialAttackEffect\(m_targetGid,\s*hideTime\)"),
    "state 3 effect cleanup and animation": (impl_states["case 3"], r"const int bodyState\s*=\s*m_bodyState[\s\S]*?m_isForceState\s*=\s*0[\s\S]*?m_isForceState2\s*=\s*0[\s\S]*?m_isForceState3\s*=\s*0[\s\S]*?m_isMotionFreezed\s*=\s*0[\s\S]*?bodyState\s*==\s*2[\s\S]*?SendMsg\(nullptr,\s*59[\s\S]*?SendMsg\(nullptr,\s*62[\s\S]*?m_isLieOnGround\s*=\s*1[\s\S]*?m_shouldAddPickInfo\s*=\s*0[\s\S]*?if\s*\(m_job\s*>=\s*6017\s*&&\s*m_job\s*<=\s*6046\)[\s\S]*?SetAction\(64,\s*4,\s*1\)[\s\S]*?else[\s\S]*?SetAction\(32,\s*8,\s*1\)[\s\S]*?UnRegisterPos"),
    "state 4 force return and conditional action/motion": (impl_states["case 4"], r"m_isForceState\s*\|\|\s*m_isForceState2\s*\|\|\s*m_isForceState3[\s\S]*?return[\s\S]*?SetAction\(48,\s*5,\s*1\)[\s\S]*?m_curAction\s*<\s*40[\s\S]*?SetAction\(24,\s*4,\s*1\)[\s\S]*?ProcessMotion\(\)"),
    "state 7 object/job actions": (impl_states["case 7"], r"m_objectType\s*==\s*9[\s\S]*?SetAction\(16,\s*4,\s*1\)[\s\S]*?m_job\s*>=\s*6001[\s\S]*?m_job\s*<=\s*6016[\s\S]*?SetAction\(40,\s*4,\s*1\)[\s\S]*?SetAction\(96,\s*6,\s*1\)"),
    "state 10 lie and frozen finished pose": (impl_states["case 10"], r"m_bodyState\s*==\s*2[\s\S]*?SendMsg\(nullptr,\s*59[\s\S]*?SendMsg\(nullptr,\s*62[\s\S]*?m_isLieOnGround\s*=\s*0[\s\S]*?m_isMotionFinished\s*=\s*1[\s\S]*?m_isMotionFreezed\s*=\s*1[\s\S]*?SetAction\(0,\s*8,\s*1\)"),
    "common tail state-4 exception": (impl, r"m_curAction\s*=\s*m_baseAction\s*\+\s*Get8Dir\(m_roty\)[\s\S]*?if\s*\(state\s*!=\s*4\)[\s\S]*?m_stateId\s*=\s*state[\s\S]*?m_stateStartTick\s*=\s*timeGetTime\(\)"),
}
for description, (text, pattern) in impl_anchors.items():
    assert re.search(pattern, text), f"current NPC implementation mismatch: {description}"
print(f"PASS source/current branch comparison ({len(impl_anchors)} current-route assertions)")
print("Documented scope: pet talk is a source-proven no-op; actor class is unified; skill units bypass the NPC route")
