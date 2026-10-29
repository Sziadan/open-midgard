#!/usr/bin/env python3
"""Compare effect699's STR mapping and ignored completion result."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
DISPATCH = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")
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


for name, source in (("PE", PE), ("primary Ref", REF)):
    init = body(source, "void __thiscall CRagEffect::Init(CRagEffect *this, CRenderObject *master, int effectId, vector3d fDeltaPos)")
    case = re.search(r"\bcase\s+699\s*:[\s\S]*?\bbreak\s*;", init)
    check(f"{name} Init maps effect699 to flower_leaf.str",
          case is not None and "sprintf(StrName, aFlowerLeafStr);" in case.group(0))

for name, source in (("local catalog", CATALOG), ("local STR table", STR_TABLE)):
    check(f"{name} maps effect699 to flower_leaf.str",
          '{ 699, "flower_leaf.str", nullptr, nullptr, 0, 0 }' in source)
image = ExeImage(ROOT / "HighPriest.exe")
resolved = image.read_cstring_at(0x71A7D0)
check("PE bytes at VA 0x71a7d0 equal flower_leaf.str",
      resolved is not None and resolved.raw == b"flower_leaf.str")

for name, source in (("PE", PE), ("primary Ref", REF)):
    on_process = body(source, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
    compact = re.sub(r"\s+", "", on_process)
    start = compact.index("case0x1EB:")
    end = compact.index("case0x1ED:", start)
    cases = compact[start:end]
    check(f"{name} effect699 groups into shared STR tail and ignores completion",
          "case0x2BB:" in cases and "goto$L146219;" in cases
          and "$L146219:CRagEffect::ProcessEZ2STR(this,0.0,-80.0,0.0);return1;" in compact)

check("local effect699 ignores ProcessEZ2STR completion and returns true",
      "case 699: ProcessEZ2STR(); return true;" in DISPATCH)
check("local shared STR processor exposes false completion to be ignored",
      "if (m_iCurLayer >= m_cEndLayer) {\n        return false;" in LOCAL)
print("effect699 STR/ignored-completion comparison passed")
