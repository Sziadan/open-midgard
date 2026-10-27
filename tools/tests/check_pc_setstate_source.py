#!/usr/bin/env python3
"""Source audit gate for CPc::SetState before its in-scope port."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
REF = (ROOT / "Ref/Pc.cpp").read_text(encoding="utf-8", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
IMPL = (ROOT / "src/world/GameActor.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "src/world/GameActor.h").read_text(encoding="utf-8")


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


def top_level_cases(function_body: str, switch_var: str) -> dict[int, str]:
    switch = re.search(rf"switch\s*\(\s*{switch_var}\s*\)\s*\{{", function_body)
    assert switch, f"switch({switch_var}) missing"
    offset = switch.end()
    depth = 1
    labels: list[tuple[int, int, int]] = []
    close = len(function_body)
    for match in re.finditer(r"[^\n]*(?:\n|$)", function_body[offset:]):
        line = match.group(0)
        if depth == 1:
            label = re.match(r"\s*case\s+([^:]+)\s*:", line)
            if label:
                token = label.group(1).strip()
                try:
                    value = int(token, 0)
                except ValueError:
                    value = {"kMoveStateId": 1, "kAttackStateId": 2, "kSecondAttackStateId": 9}.get(token, -1)
                assert value >= 0, f"unmapped state label: {token}"
                labels.append((value, offset + match.start(), offset + match.end()))
        depth += line.count("{") - line.count("}")
        if depth == 0:
            close = offset + match.end()
            break
    sections: dict[int, str] = {}
    for index, (value, _, label_end) in enumerate(labels):
        end = labels[index + 1][1] if index + 1 < len(labels) else close
        assert value not in sections, f"duplicate top-level case {value}"
        sections[value] = function_body[label_end:end]
    return sections


marker = "void __thiscall CPc::SetState(CPc *this, int stateId)"
ref_sig, ref_body = body(REF, marker, last=True)
exe_sig, exe_body = body(EXE, marker, last=True)
normalize = lambda value: re.sub(r"\s+", " ", value).strip()
assert normalize(ref_sig) == normalize(exe_sig), "CPc::SetState Ref/EXE signatures differ"
assert normalize(ref_body) == normalize(exe_body), "complete CPc::SetState Ref/EXE bodies differ"
print(f"PASS complete Ref/EXE body equality ({len(normalize(ref_body))} normalized characters)")

ref_cases = top_level_cases(ref_body, "stateId")
exe_cases = top_level_cases(exe_body, "stateId")
assert list(ref_cases) == list(exe_cases), "Ref/EXE CPc state labels/order differ"
assert list(ref_cases) == [0, 1, 2, 6, 7, 8, 9], f"unexpected source state inventory: {list(ref_cases)}"

# Explicit branch-level source inventory for the animation/effect routes. Case
# 0 camera zoom and waiting-use-item-ack are UI/input behavior and intentionally
# excluded from this skills/animations port.
source_requirements = {
    0: [r"CGameActor::SetState\(this,\s*0\)", r"m_curMotion\s*=\s*this->m_headDir", r"CRenderObject::SetAction\(this,\s*32,\s*4,\s*0\)"],
    1: [r"m_isCounter\s*=\s*0", r"m_isMotionFreezed\s*=\s*0", r"m_headDir\s*=\s*0", r"CGameActor::SetState\(this,\s*1\)"],
    2: [r"CGameObject::SendMsg\(this,\s*0,\s*83", r"CSession::IsSecondAttack", r"rand\(\)\s*%\s*10\s*<\s*7", r"CSession::GetPCAttackMotion", r"WeaponType\s*==\s*11", r"CGameMode::MakeArrowEffect", r"8\.0\s*/\s*this->m_motionSpeed"],
    6: [r"CGameActor::RegisterPos\(this\)", r"SetAction\(this,\s*16,\s*4,\s*1\)", r"m_isMotionFinished\s*=\s*1", r"m_curMotion\s*=\s*m_headDir"],
    7: [r"m_stateId\s*==\s*6", r"CGameActor::GetJob\(this\)\s*==\s*4020", r"SetForceAnimSpeed\(this,\s*4u\)", r"SetForceAnimation\(this,\s*96,\s*0,\s*0,\s*96,\s*0,\s*1,\s*0x190u,\s*0\)", r"SetForceAnimation\(this,\s*96,\s*2,\s*5,\s*96,\s*5,\s*1,\s*0x3E8u,\s*0\)", r"SetForceAnimation\(this,\s*96,\s*0,\s*3,\s*96,\s*3,\s*1,\s*0x3E8u,\s*0\)", r"m_attackMotion\s*=\s*6\.0"],
    8: [r"CGameActor::RegisterPos\(this\)", r"SetAction\(this,\s*96,\s*6,\s*1\)", r"m_isMotionFinished\s*=\s*1"],
    9: [r"CGameObject::SendMsg\(this,\s*0,\s*83", r"CSession::IsSecondAttack", r"CSession::GetPCAttackMotion", r"v17\s*==\s*14\s*\|\|\s*v17\s*==\s*13", r"goto\s+LABEL_70"],
}
for state, patterns in source_requirements.items():
    branch = ref_cases[state]
    for pattern in patterns:
        assert re.search(pattern, branch), f"source state {state} missing branch operation: {pattern}"
print("PASS source branch inventory for idle/move/attack/sit/skill/cast/second-attack animation routes")

impl_marker = "void CPc::SetState(int state)"
assert impl_marker in IMPL and re.search(r"void SetState\(int state\) override;", HEADER), "current CPc::SetState implementation/declaration missing"
_, impl_body = body(IMPL, impl_marker)
impl_cases = top_level_cases(impl_body, "state")
print(f"Current state labels: {list(impl_cases)}; source labels: {list(ref_cases)}")
missing = sorted(set(ref_cases) - set(impl_cases))
print(f"Current explicit state gaps before port: {missing}; state-2/state-9 attack semantics are currently combined")
