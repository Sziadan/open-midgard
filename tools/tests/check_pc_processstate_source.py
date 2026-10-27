#!/usr/bin/env python3
"""Source-to-current branch gate for CPc::ProcessState's in-scope behavior."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
REF = (ROOT / "Ref/Pc.cpp").read_text(encoding="utf-8", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
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


def ordered_cases(source: str, switch_name: str) -> tuple[list[int], dict[int, str]]:
    switch = re.search(rf"switch\s*\(\s*(?:this->)?{switch_name}\s*\)\s*\{{", source)
    assert switch, f"switch({switch_name}) missing"
    start = switch.end()
    depth = 1
    labels: list[tuple[int, int, int]] = []
    end = len(source)
    for line in re.finditer(r"[^\n]*(?:\n|$)", source[start:]):
        value = line.group()
        if depth == 1:
            label = re.match(r"\s*case\s+([^:]+)\s*:", value)
            if label:
                token = label.group(1).strip()
                value_id = int(token, 0) if re.fullmatch(r"\d+|0[xX][0-9a-fA-F]+", token) else {
                    "kMoveStateId": 1, "kAttackStateId": 2, "kSecondAttackStateId": 9
                }.get(token)
                assert value_id is not None, f"unmapped state label: {token}"
                labels.append((value_id, start + line.start(), start + line.end()))
        depth += value.count("{") - value.count("}")
        if depth == 0:
            end = start + line.end()
            break
    sections: dict[int, str] = {}
    for index, (case_id, _, label_end) in enumerate(labels):
        next_start = labels[index + 1][1] if index + 1 < len(labels) else end
        assert case_id not in sections, f"duplicate switch state {case_id}"
        sections[case_id] = source[label_end:next_start]
    return [item[0] for item in labels], sections


def normalized(value: str) -> str:
    return re.sub(r"\s+", " ", value).strip()


ref_sig, ref_body = body(REF, "unsigned __int8 __thiscall CPc::ProcessState(CPc *this)")
exe_sig, exe_body = body(EXE, "unsigned __int8 __thiscall CPc::ProcessState(CPc *this)")
assert normalized(ref_sig) == normalized(exe_sig), "CPc::ProcessState Ref/EXE signatures differ"
assert normalized(ref_body) == normalized(exe_body), "complete CPc::ProcessState Ref/EXE bodies differ"
print(f"PASS complete Ref/EXE body equality ({len(normalized(ref_body))} normalized characters)")

ref_ids, ref_cases = ordered_cases(ref_body, "m_stateId")
exe_ids, exe_cases = ordered_cases(exe_body, "m_stateId")
assert ref_ids == exe_ids == [2, 3, 6, 7, 8], f"source states/order mismatch: Ref={ref_ids}, EXE={exe_ids}"
print(f"PASS mechanically extracted Ref/EXE state IDs and order: {ref_ids}")

source_checks = {
    2: r"this->CRenderObject::ProcessMotion\(this\)\s*;\s*timeGetTime\(\)\s*;\s*goto\s+LABEL_12",
    3: r"m_isForceState\s*=\s*0[\s\S]*?m_isForceState2\s*=\s*0[\s\S]*?m_isForceState3\s*=\s*0[\s\S]*?m_isMotionFreezed\s*=\s*0[\s\S]*?ProcessMotion\(this\)[\s\S]*?return\s+1",
    6: r"this->CRenderObject::ProcessMotion\(this\)\s*;\s*return\s+1",
    7: r"this->CRenderObject::ProcessMotion\(this\)",
    8: r"CRenderObject::ProcessMotion\(this\)[\s\S]*?0\.041666668[\s\S]*?m_targetGid[\s\S]*?%\s*34[\s\S]*?GetGameActorByAID[\s\S]*?CalcDir",
}
for state, pattern in source_checks.items():
    assert re.search(pattern, ref_cases[state]), f"Ref state {state} missing expected branch behavior"
    assert re.search(pattern, exe_cases[state]), f"EXE state {state} missing expected branch behavior"
assert re.search(r"LABEL_12\s*:[\s\S]*?m_isMotionFinished[\s\S]*?CGameActor::SetState\(this,\s*0\)", ref_body), "shared state-2/7 completion branch missing"
assert re.search(r"LABEL_12\s*:[\s\S]*?m_isMotionFinished[\s\S]*?CGameActor::SetState\(this,\s*0\)", exe_body), "EXE shared state-2/7 completion branch missing"
print("PASS source branches: attacks 2/7 share idle completion; 3 clears force/freeze; 6 advances motion; 8 retargets every 34 frames")

impl_sig, impl_body = body(IMPL, "u8 CGameActor::ProcessState()")
impl_ids, impl_cases = ordered_cases(impl_body, "m_stateId")
source_order_in_current = [state for state in impl_ids if state in ref_ids]
assert source_order_in_current == [2, 3, 6, 7, 8], f"current source branch order mismatch: {source_order_in_current}"
assert 6 in impl_cases, "current consolidated ProcessState lacks state 6"
assert re.search(r"if\s*\(\s*m_isPc\s*\)\s*ProcessMotion\(\)\s*;", impl_cases[6]), "state 6 motion is not guarded to PC actors"
assert not re.search(r"SetState|m_isCounter\s*=|m_isMotionFreezed\s*=", impl_cases[6]), "state 6 adds behavior absent from CPc source"
print("PASS current state 6 maps to a PC-guarded ProcessMotion-only branch")

current_attack_branch = " ".join(impl_cases[state] for state in (2, 9, 12))
assert re.search(r"ProcessMotion\(\)[\s\S]*?m_isMotionFinished[\s\S]*?SetState\(0\)", current_attack_branch), "current state 2 lacks source motion completion to idle"
assert re.search(r"ProcessMotion\(\)[\s\S]*?m_isMotionFinished[\s\S]*?SetState\(0\)", impl_cases[7]), "current state 7 lacks source motion completion to idle"
assert re.search(r"m_isForceState\s*=\s*0[\s\S]*?m_isForceState2\s*=\s*0[\s\S]*?m_isForceState3\s*=\s*0[\s\S]*?m_isMotionFreezed\s*=\s*0[\s\S]*?ProcessMotion\(\)", impl_cases[3]), "current state 3 force/freeze clearing or motion order differs"
assert re.search(r"ProcessMotion\(\)[\s\S]*?static_cast<double>\(nCx\)\s*\*\s*0\.041666668[\s\S]*?slice\s*%\s*34[\s\S]*?ResolveActorByGidForCastRetarget[\s\S]*?std::atan2", impl_cases[8]), "current state 8 retarget branch/order/constants differ"
assert re.search(r"static_cast<long double>\(target->m_pos\.x\)\s*-\s*m_pos\.x[\s\S]*?static_cast<long double>\(target->m_pos\.z\)\s*-\s*m_pos\.z[\s\S]*?/\s*3\.141592L\s*\*\s*180\.0L[\s\S]*?rotation\s*>=\s*360\.0L[\s\S]*?m_roty\s*<\s*0\.0f", impl_cases[8]), "current CalcDir-equivalent arithmetic differs from Ref/GameActor.cpp:8831"
assert "CGameActor::ProcessVanish" in ref_body and re.search(r"if\s*\(m_vanishTime\s*!=\s*0\)[\s\S]*?if\s*\(Time\s*>\s*m_vanishTime\)[\s\S]*?return\s+0", impl_body), "vanish early-exit behavior is not represented in the consolidated handler"
assert re.search(r"if\s*\(m_freezeEndTick\s*!=\s*0\s*&&\s*timeGetTime\(\)\s*>\s*m_freezeEndTick\)[\s\S]*?m_isMotionFreezed\s*=\s*0[\s\S]*?m_freezeEndTick\s*=\s*0", impl_body), "freeze expiry prologue differs"
assert re.search(r"default\s*:[\s\S]*?break\s*;", ref_body), "source default-to-base route missing"
assert re.search(r"ProcessEffectState\(\)[\s\S]*?ProcessWillBeAttacked\(\)[\s\S]*?ProcessQueuedHitWave", impl_body), "consolidated actor upkeep integration order is missing"
print("PASS current branch comparison: states 2/3/6/7/8, freeze/vanish, cast timing, and preserved actor-update integrations")
print("Documented architecture difference: source state 3 returns alive and delegates corpse creation; current actor lifecycle retains its final death frame in state 10")
