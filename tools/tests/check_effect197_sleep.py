#!/usr/bin/env python3
"""Compare decimal effect197 sleep STR routing across PE, Ref and local source."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
CATALOG = (ROOT / "src/world/RagEffectCatalog.generated.inc").read_text(encoding="utf-8")
STR_TABLE = (ROOT / "src/world/RagEffectStrNameTable.generated.inc").read_text(encoding="utf-8")

sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage


def body(source: str, marker: str) -> str:
    start = source.rindex(marker)
    opening = source.index("{", start)
    depth = 0
    for cursor in range(opening, len(source)):
        if source[cursor] == "{":
            depth += 1
        elif source[cursor] == "}":
            depth -= 1
            if depth == 0:
                return source[start:cursor + 1]
    raise AssertionError(f"unterminated function: {marker}")


def check(label: str, ok: bool) -> None:
    assert ok, f"FAIL {label}"
    print(f"PASS {label}")


init_marker = "void __thiscall CRagEffect::Init(CRagEffect *this, CRenderObject *master, int effectId, vector3d fDeltaPos)"
for source_name, source in (("PE", PE), ("primary Ref", REF)):
    init = body(source, init_marker)
    case = re.search(r"\bcase\s+197\s*:[\s\S]*?\bbreak\s*;", init)
    assert case
    check(f"{source_name} Init maps decimal197 directly to sleep.str",
          "sprintf(StrName, aSleepStr);" in case.group(0))
for path_name, source in (("local catalog", CATALOG), ("local STR table", STR_TABLE)):
    check(f"{path_name} maps decimal197 to sleep.str",
          '{ 197, "sleep.str", nullptr, nullptr, 0, 0 }' in source)

on_process = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
group = re.search(r"case\s+152\s*:[\s\S]*?case\s+197\s*:[\s\S]*?goto\s+LABEL_424\s*;", on_process)
check("PE OnProcess includes effect197 in the shared STR-only route", group is not None)
check("PE shared STR-only tail propagates ProcessEZ2STR result",
      "LABEL_424:result=CRagEffect::ProcessEZ2STR(this,0.0,-80.0,0.0);LABEL_425:if(!result)returnresult;break;"
      in re.sub(r"\s+", "", on_process))
check("local OnProcess routes decimal197 to generic STR processing",
      "case 197: return ProcessEZ2STR();" in DISPATCH)

image = ExeImage(ROOT / "HighPriest.exe")
resolved = image.read_cstring_at(0x71AB88)
check("PE resource bytes at 0x71ab88 are exactly sleep.str",
      resolved is not None and resolved.raw == b"sleep.str")
attack_table = (ROOT / "src/session/SkillAttackEffectIdTable.generated.inc").read_text(encoding="utf-8")
table_body = re.search(r"kSkillAttackedEffectIdTable\[[^\]]+\]\s*=\s*\{([\s\S]*?)\};", attack_table)
assert table_body
attack_effect_ids = [int(value) for value in re.findall(r"-?\d+", table_body.group(1))]
check("effect197 is assigned in the skill attacked-effect table", 197 in attack_effect_ids)
actor_handler = (ROOT / "src/world/GameActorMsgHandler.cpp").read_text(encoding="utf-8")
check("actor combat flow reads attacked-effect ID before ResolveSkillAttackEffect",
      "int effectId2 = g_session.GetAttackedEffectIdBySkillId(skillId);" in actor_handler
      and "ResolveSkillAttackEffect(skillId, damage, effectId2)" in actor_handler)

print("effect197 sleep Init/OnProcess/resource/local route comparison passed")
