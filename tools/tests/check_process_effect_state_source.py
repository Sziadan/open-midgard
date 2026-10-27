#!/usr/bin/env python3
"""Compare CGameActor::ProcessEffectState against Ref and canonical EXE."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
REF = (ROOT / "Ref/GameActor.cpp").read_text(encoding="utf-8", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
IMPL = (ROOT / "src/world/GameActor.cpp").read_text(encoding="utf-8")


def definition(text: str, marker: str, *, last: bool = False) -> tuple[str, str]:
    search = text.rfind if last else text.find
    start = search(marker)
    if start < 0:
        raise AssertionError(f"function definition not found: {marker}")
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


REF_MARKER = "void __thiscall CGameActor::ProcessEffectState(CGameActor *this)"
ref_signature, ref_body = definition(REF, REF_MARKER)
exe_signature, exe_body = definition(EXE, REF_MARKER, last=True)
impl_signature, impl_body = definition(IMPL, "void CGameActor::ProcessEffectState()")
normalize = lambda value: re.sub(r"\s+", " ", value).strip()
assert normalize(ref_signature) == normalize(exe_signature), "Ref/EXE ProcessEffectState signatures differ"
assert normalize(ref_body) == normalize(exe_body), "complete Ref/EXE ProcessEffectState bodies differ"
print(f"PASS complete Ref/EXE body equality ({len(normalize(ref_body))} normalized characters)")


def require_order(text: str, patterns: list[str], description: str) -> None:
    cursor = 0
    for pattern in patterns:
        match = re.compile(pattern).search(text, cursor)
        assert match, f"{description}: missing ordered source operation {pattern}"
        cursor = match.end()


# The executable's named thresholds resolve to exact numeric values.
assert re.search(r"dword_718304\s*=\s*270\s*;", EXE), "canonical Sight relaunch threshold is not 270"
assert re.search(r"dword_718330\s*=\s*200\s*;", EXE), "canonical Ruwach relaunch threshold is not 200"

# Source control-flow anchors: skip-frame reset, zero state reset, Sight then
# independent Ruwach test, and one end-of-branch counter increment.
require_order(ref_body, [
    r"g_isSkipFrame\s*>\s*0", r"m_BeZero\s*==\s*1", r"m_BodyLight\s*=\s*0",
    r"m_BeZero\s*=\s*0", r"IsEffectStateNothing", r"m_effectLaunchCnt\s*=\s*36000",
    r"IsEffectStateSight", r"dword_718304", r"m_effectLaunchCnt\s*=\s*0",
    r"LaunchEffect\([^;]*?22", r"SendMsg\([^;]*?44", r"IsEffectStateRuwach",
    r"dword_718330", r"m_effectLaunchCnt\s*=\s*0", r"LaunchEffect\([^;]*?33",
    r"SendMsg\([^;]*?44", r"\+\+this->m_effectLaunchCnt",
], "canonical ProcessEffectState branch map")
assert len(re.findall(r"\+\+this->m_effectLaunchCnt", ref_body)) == 1, "source must increment the counter once"

# Current implementation branch map. Keep the two effect checks independent:
# when Sight resets the count, Ruwach must observe the reset value.
current_order = [
    r"g_isSkipFrame\s*>\s*0", r"m_BeZero\s*==\s*1", r"m_BodyLight\s*=\s*0",
    r"m_BeZero\s*=\s*0", r"m_effectState\s*==\s*0", r"m_effectLaunchCnt\s*=\s*36000",
    r"m_effectState\s*&\s*(?:kEffectStateSightMask|0x0*1)", r"m_effectLaunchCnt\s*>=\s*(?:kSightRelaunchTicks|270)",
    r"m_effectLaunchCnt\s*=\s*0", r"LaunchEffect\(\s*22\b", r"SendMsg\([^;]*?44",
    r"m_effectState\s*&\s*(?:kEffectStateRuwachMask|0x0*2000)", r"m_effectLaunchCnt\s*>=\s*(?:kRuwachRelaunchTicks|200)",
    r"m_effectLaunchCnt\s*=\s*0", r"LaunchEffect\(\s*33\b", r"SendMsg\([^;]*?44",
    r"\+\+m_effectLaunchCnt",
]
require_order(impl_body, current_order, "current ProcessEffectState branch map")
assert len(re.findall(r"\+\+m_effectLaunchCnt", impl_body)) == 1, "current implementation must increment once"
assert len(re.findall(r"m_effectLaunchCnt\s*=\s*0", impl_body)) == 2, "counter must reset once in each effect branch"
assert len(re.findall(r"LaunchEffect\(\s*22\b", impl_body)) == 1, "Sight must launch effect 22 exactly once per branch"
assert len(re.findall(r"LaunchEffect\(\s*33\b", impl_body)) == 1, "Ruwach must launch effect 33 exactly once per branch"

random_marker = r"\(\s*timeGetTime\(\)\s*>>\s*4\s*\)\s*%\s*10\s*\+\s*1"
assert len(re.findall(random_marker, impl_body)) == 2, "each successful effect launch must use the source message-44 marker"
assert len(re.findall(r"SendMsg\([^;]*?44", impl_body)) == 2, "each effect branch must send message 44 once"
for effect_id in (22, 33):
    branch = re.search(
        rf"if\s*\(\s*CRagEffect\s*\*\s*(\w+)\s*=\s*LaunchEffect\(\s*{effect_id}\b"
        rf"[\s\S]{{0,220}}?\)\s*\)\s*\{{[\s\S]{{0,220}}?\1->SendMsg\(\1,\s*44",
        impl_body,
    )
    assert branch, f"effect {effect_id} message 44 must be conditional on a successful launch"

print("PASS ordered reset, threshold, Sight/Ruwach effect routes, random message 44, and single counter increment")
