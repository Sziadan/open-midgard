#!/usr/bin/env python3
"""Ref/EXE source gate for the Mandra attack-effect route and lifecycle."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
REF_SKILL = (ROOT / "Ref/Skill.cpp").read_text(encoding="utf-8", errors="replace")
REF_MODE = (ROOT / "Ref/GameMode.cpp").read_text(encoding="utf-8", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
IMPL_ACTOR = (ROOT / "src/world/GameActor.cpp").read_text(encoding="utf-8")
IMPL_ARROW = (ROOT / "src/world/ArrowEffect.cpp").read_text(encoding="utf-8")
IMPL_MODE = (ROOT / "src/world/ArrowEffect.cpp").read_text(encoding="utf-8")


def body(text: str, marker: str, *, last: bool = False) -> tuple[str, str]:
    start = (text.rfind if last else text.find)(marker)
    if start < 0:
        raise AssertionError(f"function not found: {marker}")
    opening = text.find("{", start)
    semicolon = text.find(";", start)
    if opening < 0 or (semicolon >= 0 and semicolon < opening):
        raise AssertionError(f"found declaration instead of definition: {marker}")
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[start:opening].strip(), text[opening + 1:pos]
    raise AssertionError(f"unclosed function: {marker}")


def equal(ref: str, exe: str, marker: str) -> str:
    ref_sig, ref_body = body(ref, marker, last=True)
    exe_sig, exe_body = body(exe, marker, last=True)
    norm = lambda text: re.sub(r"\s+", " ", text).strip()
    assert norm(ref_sig) == norm(exe_sig), f"Ref/EXE signature mismatch: {marker}"
    assert norm(ref_body) == norm(exe_body), f"complete Ref/EXE body mismatch: {marker}"
    return ref_body


ctor = equal(REF_SKILL, EXE, "CMandraAttackEffect *__thiscall CMandraAttackEffect::CMandraAttackEffect(CMandraAttackEffect *this)")
on_init = equal(REF_SKILL, EXE, "void __thiscall CMandraAttackEffect::OnInit(CMandraAttackEffect *this)")
on_process = equal(REF_SKILL, EXE, "bool __thiscall CMandraAttackEffect::OnProcess(CMandraAttackEffect *this)")
send_msg = equal(REF_SKILL, EXE, "void __thiscall CMandraAttackEffect::SendMsg(")
factory = equal(REF_MODE, EXE, "void __thiscall CGameMode::MakeMandraAttackEffect(CGameMode *this, unsigned int targetGid, int job, float hideTime)")
print("PASS complete Ref/EXE body equality for Mandra constructor/init/process/message/factory")

# Branch-level source inventory: exact sprite routes, target lookup and animation
# lifecycle. Asset strings are kept in EXE encoding; dhxj names are only comments.
sprite_routes = {
    "1884": "DROSERA_BULLET", "1886": "DROSERA_BULLET",
    "1957": "ENTWEIHEN_BULLET", "1958": "ENTWEIHEN_BULLET",
    "1959": "ENTWEIHEN_BULLET", "1960": "ENTWEIHEN_BULLET",
    "1961": "ENTWEIHEN_BULLET", "1781": "hydra_atk",
    "1068": "MAVKA_BULLET", "1704": "tha_odium_ef", "1709": "tha_odium_ef",
}
source_job_assets = {}
for match in re.finditer(r"case\s+(\d+)\s*:", send_msg):
    job = int(match.group(1))
    branch_end = send_msg.find("break;", match.end())
    branch_text = send_msg[match.end():branch_end if branch_end >= 0 else len(send_msg)]
    spr = re.search(r"SetSprName\([^;]*?(byte_\w+)", branch_text)
    act = re.search(r"SetActName\([^;]*?(byte_\w+)", branch_text)
    if spr and act:
        source_job_assets[job] = (spr.group(1), act.group(1))
expected_source_assets = {
    1884: ("byte_725084", "byte_72506C"), 1886: ("byte_725084", "byte_72506C"),
    **{job: ("byte_725050", "byte_725034") for job in range(1957, 1962)},
    1781: ("byte_7250B8", "byte_72509C"), 1068: ("byte_7250EC", "byte_7250D4"),
    1704: ("byte_72511C", "byte_725104"), 1709: ("byte_72511C", "byte_725104"),
}
assert source_job_assets == expected_source_assets, f"source job->sprite branch mapping differs: {source_job_assets}"
for token in (
    "CRenderObject::GetTargetPos", "m_isVisible = 1", "m_stateStartTick = timeGetTime()",
    "CActRes::GetDelay(ActRes, 0)", "arg3 == 1068", "m_motionSpeed = 2.0",
    "if ( arg2 )", "m_hideTime = *arg2",
):
    assert token in send_msg, f"source message branch missing {token}"
for token in ("v2 >= this->m_hideTime", "this->m_isVisible = 0", "this->m_isVisible = 1", "stateCnt / this->m_motionSpeed", "this->m_isMotionFinished = 1", "MotionCount - 1"):
    assert token in on_process, f"source process branch missing {token}"
for token in ("operator new(0xFCu)", "m_gameObjectList", "targetGid", "&hideTime", "job"):
    assert token in factory, f"source factory operation missing {token}"
print("PASS source branch inventory: 11 job sprite variants, hide timing, delay, finish and factory lifecycle")

current_marker = "CMandraAttackEffect::"
has_impl = current_marker in IMPL_ARROW or "MakeMandraAttackEffect" in IMPL_MODE
if not has_impl:
    print("PASS current Mandra implementation is absent; source comparison prerequisite recorded")
else:
    # Current port intentionally reuses the current billboard/world-object
    # architecture. Compare each message branch by job and selected asset.
    _, current_send = body(IMPL_ARROW, "void CMandraAttackEffect::SendMsg(")
    _, current_process = body(IMPL_ARROW, "u8 CMandraAttackEffect::OnProcess()")
    _, current_factory = body(IMPL_ARROW, "void CGameMode::MakeMandraAttackEffect(")
    cases = list(re.finditer(r"case\s+(\d+)\s*:", current_send))
    current_routes = {}
    for match in cases:
        job = int(match.group(1))
        branch_end = current_send.find("break;", match.end())
        branch_text = current_send[match.end():branch_end if branch_end >= 0 else len(current_send)]
        asset = re.search(r'sprite\s*=.*\\\\([A-Za-z0-9_]+)\.spr"', branch_text)
        assert asset, f"current sprite choice is missing after job {job}"
        current_routes[job] = asset.group(1)
    expected_assets = {
        1884: "drosera_bullet", 1886: "drosera_bullet",
        1957: "ENTWEIHEN_BULLET", 1958: "ENTWEIHEN_BULLET", 1959: "ENTWEIHEN_BULLET",
        1960: "ENTWEIHEN_BULLET", 1961: "ENTWEIHEN_BULLET", 1781: "hydra_atk",
        1068: "MAVKA_BULLET", 1704: "tha_odium_ef", 1709: "tha_odium_ef",
    }
    assert current_routes == expected_assets, f"current job->sprite branches differ: {current_routes}"
    assert re.search(r"message\s*!=\s*14[\s\S]*?CRenderObject::SendMsg", current_send), "non-14 messages must delegate to the base renderer"
    ordered_process = [r"elapsedFrames\s*<\s*m_mandraHideTime", r"m_isVisible\s*=\s*0", r"m_isVisible\s*=\s*1", r"animationFrames\s*=", r"m_actRes->GetMotionCount", r"static_cast<int>\(motionFrame\)\s*%\s*motionCount", r"motionFrame\s*/\s*static_cast<double>\(motionCount\)\s*>=\s*1\.0", r"m_mandraMotionFinished\s*=\s*true", r"m_curMotion\s*=\s*motionCount\s*-\s*1"]
    cursor = 0
    for pattern in ordered_process:
        match = re.search(pattern, current_process[cursor:])
        assert match, f"current animation branch missing/out of order: {pattern}"
        cursor += match.end()
    assert "job == 1068" in current_send and "m_motionSpeed = 2.0f" in current_send, "job 1068 speed override missing"
    assert re.search(r"GetDelay\(0\)[\s\S]*?job\s*==\s*1068", current_send), "action-zero delay must precede the 1068 override"
    assert re.search(r"m_world->m_gameObjectList\.push_back\(effect\)[\s\S]*?effect->SendMsg\(nullptr,[\s\S]*?targetGid[\s\S]*?hideTime[\s\S]*?job", current_factory), "factory must insert and initialize the effect with target, hide time, and job"
    print("PASS current branch comparison: exact job->sprite table, ordered hide/animation lifecycle, delay override and factory message")
