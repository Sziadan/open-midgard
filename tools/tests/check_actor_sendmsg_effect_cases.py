#!/usr/bin/env python3
"""Compare effect-launch arms 20 and 36 in CGameActor::SendMsg."""
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


def switch_branch(function: str, case_id: int) -> str:
    switch = re.search(r"switch\s*\(\s*(?:message|msg)\s*\)\s*\{", function)
    assert switch, "message switch missing"
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
            end = labels[index + 1][1] if index + 1 < len(labels) else len(function)
            return function[end_label:end]
    raise AssertionError(f"case {case_id} missing")


ref = body(REF, "void __thiscall CGameActor::SendMsg(CGameActor *this, int sender, int message, int arg1, float arg2, int arg3)\n{")
exe = body(EXE, "void __thiscall CGameActor::SendMsg(CGameActor *this, int sender, int message, int arg1, float arg2, int arg3)\n{")
impl = body(IMPL, "void CGameActor::SendMsg(CGameObject* src, int msg, msgparam_t par1, msgparam_t par2, msgparam_t par3)\n{")
assert re.sub(r"\s+", " ", ref) == re.sub(r"\s+", " ", exe), "Ref and executable CGameActor::SendMsg bodies differ"
print("PASS full Ref/executable SendMsg body equality")

source20 = switch_branch(exe, 20)
source36 = switch_branch(exe, 36)
impl20 = switch_branch(impl, 20)
impl36 = switch_branch(impl, 36)
assert re.search(r"CAbleToMakeEffect::LaunchEffect\(this, 7, z, 0\.0\);", source20)
assert re.search(r"LaunchEffect\(7, vector3d\{\}, 0\.0f\);", impl20)
print("PASS message 20 launches effect 7")

assert re.search(r"CAbleToMakeEffect::LaunchEffect\(this, 68, z, 0\.0\);", source36)
assert re.search(r"SendMsg\(v12, this, 50, 0, 0, 0\);", source36)
assert re.search(r"LaunchEffect\(68, vector3d\{\}, 0\.0f\)[\s\S]*?effect->SendMsg\(this, 50, 0, 0, 0\);", impl36)
print("PASS message 36 launches effect 68, then sends effect message 50")

REF_EFFECT = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
ref_effect = body(REF_EFFECT, "void __thiscall CRagEffect::SendMsg(\n        CRagEffect *this,")
exe_effect = body(EXE, "void __thiscall CRagEffect::SendMsg(\n        CRagEffect *this,")
rag = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
impl_effect = body(rag, "void CRagEffect::SendMsg(CGameObject* sender, int msg, msgparam_t par1, msgparam_t par2, msgparam_t par3)\n{")
assert re.sub(r"\s+", " ", ref_effect) == re.sub(r"\s+", " ", exe_effect), "Ref and executable CRagEffect::SendMsg bodies differ"
assert not re.search(r"case 50\s*:", ref_effect)
assert not re.search(r"case 50\s*:", impl_effect)
print("PASS effect receiver source equality and message-50 default no-op")
