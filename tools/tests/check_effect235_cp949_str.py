#!/usr/bin/env python3
"""Compare launched effect235's CP949 STR mapping and generic route."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
CATALOG = (ROOT / "src/world/RagEffectCatalog.generated.inc").read_text(encoding="utf-8")
STR_TABLE = (ROOT / "src/world/RagEffectStrNameTable.generated.inc").read_text(encoding="utf-8")
LAUNCHES = (ROOT / "src/gamemode/UseSkillEffect.generated.inc").read_text(encoding="utf-8")

import sys
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
    case = re.search(r"\bcase\s+235\s*:[\s\S]*?\bbreak\s*;", init)
    assert case
    check(f"{source_name} Init case235 selects the executable's fixed STR symbol",
          "sprintf(StrName, byte_71AAAC);" in case.group(0))

local_name = r'\xB5\xF0\xBD\xBA\xC6\xE7.str'
for label, source in (("local catalog", CATALOG), ("local STR table", STR_TABLE)):
    check(f"{label} maps effect235 to the same escaped CP949 STR bytes",
          f'{{ 235, "{local_name}", nullptr, nullptr, 0, 0 }}' in source)

image = ExeImage(ROOT / "HighPriest.exe")
resolved = image.read_cstring_at(0x71AAAC)
check("PE bytes at VA 0x71aaac equal the CP949 filename bytes",
      resolved is not None and resolved.raw == bytes.fromhex("B5 F0 BD BA C6 E7 2E 73 74 72"))

check("skill effect table contains a target launch of effect235",
      re.search(r"LaunchOn\(target,\s*235\)", LAUNCHES) is not None)
on_process = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
pe_group = re.search(r"case\s+214\s*:[\s\S]*?case\s+235\s*:[\s\S]*?goto\s+LABEL_424\s*;", on_process)
check("PE OnProcess routes effect235 through the shared STR-only branch", pe_group is not None)
check("PE shared STR-only tail propagates ProcessEZ2STR completion",
      "LABEL_424:result=CRagEffect::ProcessEZ2STR(this,0.0,-80.0,0.0);LABEL_425:if(!result)returnresult;break;"
      in re.sub(r"\s+", "", on_process))
check("local OnProcess route returns generic ProcessEZ2STR for effect235",
      "case 235: return ProcessEZ2STR();" in DISPATCH)

print("effect235 launch/Init/resource/OnProcess comparison passed")
