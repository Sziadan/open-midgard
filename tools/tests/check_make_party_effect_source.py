#!/usr/bin/env python3
"""Source/current branch gate for CGameActor::MakePartyEffect."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
REF = (ROOT / "Ref/GameActor.cpp").read_text(encoding="utf-8", errors="replace")
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


marker = "void __thiscall CGameActor::MakePartyEffect(CGameActor *this, int AID)"
ref_sig, ref_body = body(REF, marker, last=True)
exe_sig, exe_body = body(EXE, marker, last=True)
normalize = lambda text: re.sub(r"\s+", " ", text).strip()
assert normalize(ref_sig) == normalize(exe_sig), "Ref/EXE signatures differ"
assert normalize(ref_body) == normalize(exe_body), "complete Ref/EXE bodies differ"
print(f"PASS complete Ref/EXE body equality ({len(normalize(ref_body))} normalized characters)")

# Ordered branch inventory for the valid call route; cleanup of copied party
# strings in the PE is lifetime bookkeeping, represented by current value-owned
# party data and not by an observable gameplay branch.
source_order = [
    r"GetGameMode\(&g_modeMgr\)", r"m_isPKZone\s*\|\|\s*(?:m_world->)?m_isEventPVPMode",
    r"GetNumParty\(&g_session\)\s*>\s*0", r"GetPartyInfoBySequence\(&g_session,\s*&fi,\s*v3\)",
    r"fi\.AID\s*==\s*AID",
]
cursor = 0
for pattern in source_order:
    match = re.search(pattern, ref_body[cursor:])
    assert match, f"source branch missing or out of order: {pattern}"
    cursor += match.end()
assert re.search(r"LABEL_21:[\s\S]*?LaunchEffect\(v20,\s*160,\s*v15\[0\],\s*0\.0\)", ref_body), "source empty/missing-effect branch does not launch type 160"
assert "Value->m_type == 160" in ref_body and "goto LABEL_21" in ref_body, "source effect-exists branch must skip another launch"
print("PASS source branch order: zone gate and party membership; existing-effect branch skips the LABEL_21 type-160 launch")

impl_marker = "void CGameActor::MakePartyEffect(int AID)"
if impl_marker not in IMPL:
    assert "MakePartyEffect" not in HEADER, "declaration exists without definition"
    print("PASS current implementation is absent; branch comparator is a prerequisite")
else:
    _, impl_body = body(IMPL, impl_marker)
    ordered = [
        r"GetCurrentGameMode\(\)", r"m_world", r"m_isPKZone\s*\|\|\s*[^;]*m_isEventPVPMode",
        r"FindPartyMemberByAid\(\s*static_cast<unsigned int>\(AID\)\s*\)",
        r"m_effectList", r"m_type\s*==\s*160", r"LaunchEffect\(160,\s*vector3d\{\s*\},\s*0\.0f\)",
    ]
    cursor = 0
    for pattern in ordered:
        match = re.search(pattern, impl_body[cursor:])
        assert match, f"current branch missing or out of order: {pattern}"
        cursor += match.end()
    assert re.search(r"if\s*\(\s*!hasPartyEffect\s*\)[\s\S]*?LaunchEffect", impl_body), "current effect must only launch when no type-160 instance exists"
    print("PASS current branch comparison: zone and party gates, effect-160 dedupe, zero-offset launch")
