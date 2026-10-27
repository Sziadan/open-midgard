#!/usr/bin/env python3
"""Compare the full level-99 aura branch with canonical Ref and EXE behavior."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
REF = (ROOT / "Ref/GameActor.cpp").read_text(encoding="utf-8", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
IMPL = (ROOT / "src/world/GameActor.cpp").read_text(encoding="utf-8")
SESSION_REF = (ROOT / "Ref/Session.cpp").read_text(encoding="utf-8", errors="replace")
SESSION_EXE = EXE
SESSION_H = (ROOT / "src/session/Session.h").read_text(encoding="utf-8")
EFFECT_HANDLER_INDEX = (ROOT / "docs/skill/generated/effect_handler_index.tsv").read_text(encoding="utf-8")
EFFECT_CATALOG = (ROOT / "src/world/RagEffectCatalog.generated.inc").read_text(encoding="utf-8")


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


def switch_case(function: str, case_id: int) -> str:
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
            end = labels[index + 1][1] if index + 1 < len(labels) else len(function)
            return function[end_label:end]
    raise AssertionError(f"case {case_id} missing")


actor_marker = "void __thiscall CGameActor::SendMsg(CGameActor *this, int sender, int message, int arg1, float arg2, int arg3)\n{"
ref_actor = body(REF, actor_marker)
exe_actor = body(EXE, actor_marker)
impl_actor = body(IMPL, "void CGameActor::SendMsg(CGameObject* src, int msg, msgparam_t par1, msgparam_t par2, msgparam_t par3)\n{")
assert re.sub(r"\s+", " ", ref_actor) == re.sub(r"\s+", " ", exe_actor), "Ref/EXE SendMsg bodies differ"
ref_case = switch_case(ref_actor, 126)
exe_case = switch_case(exe_actor, 126)
impl_case = switch_case(impl_actor, 126)
assert re.sub(r"\s+", " ", ref_case) == re.sub(r"\s+", " ", exe_case), "message 126 Ref/EXE branches differ"

# Compare all source gates and the source's full effect-ID dispatch matrix.
for source_guard in (
    r"m_99\s*==\s*99",
    r"IsEffectStateSpecialHiding\(this->m_effectState\)",
    r"IsEffectStateBurrow\(this->m_effectState\)",
    r"IsEffectStateHiding\(this->m_effectState\)",
    r"g_session\.m_isAura\s*!=\s*1",
    r"GetJob\(this\)\s*>\s*4000",
    r"GetJob\(this\)\s*==\s*23",
    r"GetJob\(this\)\s*>=\s*1324",
    r"GetJob\(this\)\s*<=\s*1363",
    r"GetJob\(this\)\s*==\s*1732",
    r"GetJob\(this\)\s*>=\s*1938",
    r"GetJob\(this\)\s*<=\s*1955",
    r"GetJob\(this\)\s*<\s*1638",
    r"GetJob\(this\)\s*>\s*1663",
):
    assert re.search(source_guard, exe_case), f"source guard missing: {source_guard}"
for effect_id in (362, 397, 398, 202, 200, 201):
    assert re.search(rf"LaunchEffect\(this, {effect_id}, z, 0\.0\)", exe_case), f"source effect route missing: {effect_id}"

# Current branch preserves early suppression, aura toggle, all job ranges, IDs, counts and order.
for current_guard in (
    r"m_99\s*==\s*99",
    r"kEffectStateSpecialHidingMask",
    r"kEffectStateBurrowMask",
    r"kEffectStateHidingMask",
    r"g_session\.m_isAura\s*!=\s*1",
    r"job\s*>\s*4000",
    r"job\s*==\s*23",
    r"job\s*>=\s*1324\s*&&\s*job\s*<=\s*1363",
    r"job\s*==\s*1732",
    r"job\s*>=\s*1938\s*&&\s*job\s*<=\s*1955",
    r"job\s*<\s*1638",
    r"job\s*>\s*1663",
):
    assert re.search(current_guard, impl_case), f"implementation guard missing: {current_guard}"
assert impl_case.count("launchTwice(362)") == 3
assert "launchTwice(job < 1638 ? 202 : 362)" in impl_case
assert re.search(r"LaunchEffect\(397, vector3d\{\}, 0\.0f\);\s*LaunchEffect\(398, vector3d\{\}, 0\.0f\);", impl_case)
assert re.search(r"if \(job < 1638 \|\| job > 1663\)\s*\{\s*LaunchEffect\(200, vector3d\{\}, 0\.0f\);\s*LaunchEffect\(201, vector3d\{\}, 0\.0f\);", impl_case)
assert impl_case.index("m_99 = 99") < impl_case.index("g_session.m_isAura != 1")
assert impl_case.index("launchTwice(362)") < impl_case.index("LaunchEffect(397")
assert impl_case.rindex("m_is99 = 1") > impl_case.index("LaunchEffect(201")

# The source constructor enables aura by default; the current in-class default matches it.
assert "this->m_isAura = 1;" in SESSION_REF
assert "this->m_isAura = 1;" in SESSION_EXE
assert re.search(r"int\s+m_isAura\s*=\s*1\s*;", SESSION_H)

# Confirm each launched source effect has a canonical handler and track missing current handlers.
source_effect_ids = set()
for line in EFFECT_HANDLER_INDEX.splitlines()[1:]:
    columns = line.split("\t")
    if len(columns) >= 3 and columns[0] in {"OnProcess", "OnProcess2", "OnProcess3"}:
        try:
            source_effect_ids.add(int(columns[2]))
        except ValueError:
            pass
launched_effect_ids = {362, 397, 398, 202, 200, 201}
assert launched_effect_ids <= source_effect_ids, f"canonical effect handler missing: {sorted(launched_effect_ids - source_effect_ids)}"
current_catalog_ids = {
    int(value) for value in re.findall(r"^\s*\{\s*(\d+)\s*,", EFFECT_CATALOG, re.M)
}
current_effect_gaps = launched_effect_ids - current_catalog_ids
assert current_effect_gaps == launched_effect_ids, f"re-audit current aura effect dispatcher: {sorted(current_effect_gaps)}"
print("PASS message 126 source branch and default aura state; WP5 handler backlog remains for current effect IDs "
      + ",".join(map(str, sorted(current_effect_gaps))))
