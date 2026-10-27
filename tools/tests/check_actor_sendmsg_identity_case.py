#!/usr/bin/env python3
"""Compare actor message 4 identity, job, movement, state, and entry effect behavior."""
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
            return function[end_label:(labels[index + 1][1] if index + 1 < len(labels) else len(function))]
    raise AssertionError(f"case {case_id} missing")


marker = "void __thiscall CGameActor::SendMsg(CGameActor *this, int sender, int message, int arg1, float arg2, int arg3)\n{"
ref_actor = body(REF, marker)
exe_actor = body(EXE, marker)
impl_actor = body(IMPL, "void CGameActor::SendMsg(CGameObject* src, int msg, msgparam_t par1, msgparam_t par2, msgparam_t par3)\n{")
normalize = lambda value: re.sub(r"\s+", " ", value)
assert normalize(ref_actor) == normalize(exe_actor), "Ref/EXE SendMsg bodies differ"
ref_case = case(ref_actor, 4)
exe_case = case(exe_actor, 4)
impl_case = case(impl_actor, 4)
assert normalize(ref_case) == normalize(exe_case), "message 4 Ref/EXE branches differ"

# The source helper is independently verified so all chicken/star job gates are covered.
ref_proper_job = body(REF, "int __thiscall CGameActor::GetProperJob(")
exe_job_marker = "int __thiscall CGameActor::GetProperJob(CGameActor *this, int job, __int16 effectState)"
exe_proper_job = body(EXE, exe_job_marker + "\n{")
assert normalize(ref_proper_job) == normalize(exe_proper_job), "Ref/EXE GetProperJob differs"
for token in (
    "case 7:", "return 13;", "case 14:", "return 21;",
    "case 4008:", "return 4014;", "case 4030:", "return 4036;",
    "case 4015:", "return 4022;", "case 4037:", "return 4044;",
    "if ( job != 4047 )", "IsEffectStateStar2(effectState)", "result = 4048;", "return result;",
):
    assert token in normalize(exe_proper_job), f"source job transform missing {token}"

source = normalize(exe_case)
source_order = [
    "this->m_gid = *(_DWORD *)arg1",
    "this->gap34)(this, v151, *(_DWORD *)(v150 + 8))",
    "CGameMode::GetClientCoor",
    "this->m_pos.y = C3dAttr::GetHeight",
    "this->m_roty = (float)arg1",
    "this->m_speed = *(_DWORD *)(v150 + 24)",
    "CGameActor::SetAttrState(this, *(_DWORD *)(v150 + 52), *(_DWORD *)(v150 + 60), *(_DWORD *)(v150 + 56), 0)",
    "CGameActor::SetState(this, 0)",
    "this->m_pkState = *(_DWORD *)(v150 + 64)",
    "this->CGameActor::SetGuildInfo",
    "this->CGameActor::SetPropensityInfo(this, *(_DWORD *)(v150 + 100), *(_DWORD *)(v150 + 104))",
]
source_positions = [source.index(token) for token in source_order]
assert source_positions == sorted(source_positions), "source identity/state operation order changed"
assert "LODWORD(arg2) == 1" in source and "this->m_sprArgb = 1157627903" in source
assert "LODWORD(arg2) == 2" in source and "IsEffectStateSpecialHiding(*(_DWORD *)(v150 + 60))" in source
assert "CAbleToMakeEffect::LaunchEffect(this, 344, z, 0.0)" in source

impl = normalize(impl_case)
impl_order = [
    "m_gid = static_cast<u32>(actorArgs[0])",
    "m_job = resolvedJob",
    "m_sex = actorArgs[2]",
    "gameMode->GetClientCoor(actorArgs[3], actorArgs[4], m_pos.x, m_pos.z)",
    "m_roty = static_cast<float>(45 * (actorArgs[5] + 4))",
    "m_speed = static_cast<u32>(actorArgs[6])",
    "SetAttrState(actorArgs[13], effectState, actorArgs[14], false)",
    "SetState(0)",
    "m_pkState = actorArgs[16]",
    "SetGuildInfo(guildId, actorArgs[17])",
    "pcActor->m_honor = actorArgs[25]",
    "if (static_cast<int>(par2) == 1)",
]
impl_positions = [impl.index(token) for token in impl_order]
assert impl_positions == sorted(impl_positions), "implementation operation order changed"
for payload_index in (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 13, 14, 15, 16, 17, 18, 19, 20, 25, 26):
    assert f"actorArgs[{payload_index}]" in impl_case, f"actor payload index {payload_index} missing"
for old_job, replacement in ((7, 13), (14, 21), (4008, 4014), (4030, 4036), (4015, 4022), (4037, 4044)):
    assert f"case {old_job}: resolvedJob = {replacement};" in impl_case
assert "originalJob == 4047 && (effectState & star2Mask) == 0" in impl_case
assert "m_headType = 100" in impl_case
assert "m_u28.m_sprArgb = 1157627903u" in impl_case
assert "LaunchEffect(344, vector3d{}, 0.0f)" in impl_case
print("PASS message 4 Ref/EXE equality; chicken/star job transforms, state/movement/identity fields, guild/propensity, tint, and effect-344 gates mapped")
