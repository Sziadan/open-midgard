#!/usr/bin/env python3
"""Compare decimal effect156 JobChange STR routing across PE, Ref and local source."""
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
    case = re.search(r"\bcase\s+156\s*:[\s\S]*?\bbreak\s*;", init)
    assert case
    check(f"{source_name} Init maps decimal156 directly to JobChange.str",
          "sprintf(StrName, aJobchangeStr);" in case.group(0))
for path_name, source in (("local catalog", CATALOG), ("local STR table", STR_TABLE)):
    check(f"{path_name} maps decimal156 to JobChange.str",
          '{ 156, "JobChange.str", nullptr, nullptr, 0, 0 }' in source)

on_process = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
group = re.search(r"case\s+152\s*:[\s\S]*?case\s+156\s*:[\s\S]*?goto\s+LABEL_424\s*;", on_process)
check("PE OnProcess includes effect156 in the shared STR-only route", group is not None)
check("PE shared STR-only tail propagates ProcessEZ2STR result",
      "LABEL_424:result=CRagEffect::ProcessEZ2STR(this,0.0,-80.0,0.0);LABEL_425:if(!result)returnresult;break;"
      in re.sub(r"\s+", "", on_process))
check("local OnProcess routes decimal156 to generic STR processing",
      "case 156: return ProcessEZ2STR();" in DISPATCH)

image = ExeImage(ROOT / "HighPriest.exe")
resolved = image.read_cstring_at(0x71AD54)
check("PE resource bytes at 0x71ad54 are exactly JobChange.str",
      resolved is not None and resolved.raw == b"JobChange.str")
check("decimal156 stays distinct from Entry2 effect0x158 (decimal344)",
      "case 156: return ProcessEZ2STR();" in DISPATCH and "{ 156, \"JobChange.str\"" in CATALOG)

print("effect156 JobChange Init/OnProcess/resource/local route comparison passed")
