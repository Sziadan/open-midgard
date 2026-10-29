#!/usr/bin/env python3
"""Compare effect649's CP949 STR mapping and ignored completion result."""
from __future__ import annotations

import re
from pathlib import Path
import sys

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
    case = re.search(r"\bcase\s+649\s*:[\s\S]*?\bbreak\s*;", init)
    check(f"{name} Init maps effect649 to its CP949 STR symbol",
          case is not None and "sprintf(StrName, byte_71A88C);" in case.group(0))

expected = bytes.fromhex("BA D2 BD BA BE C6 C0 CC 2E 73 74 72")
escaped = r"\xBA\xD2\xBD\xBA\xBE\xC6\xC0\xCC.str"
for name, source in (("local catalog", CATALOG), ("local STR table", STR_TABLE)):
    check(f"{name} maps effect649 to the exact CP949 STR bytes",
          f'{{ 649, "{escaped}", nullptr, nullptr, 0, 0 }}' in source)
image = ExeImage(ROOT / "HighPriest.exe")
resolved = image.read_cstring_at(0x71A88C)
check("PE bytes at VA 0x71a88c match the CP949 STR filename",
      resolved is not None and resolved.raw == expected)

for name, source in (("PE", PE), ("primary Ref", REF)):
    on_process = body(source, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
    compact = re.sub(r"\s+", "", on_process)
    start = compact.index("case0x1EB:")
    end = compact.index("case0x1ED:", start)
    cases = compact[start:end]
    check(f"{name} effect649 groups into the ignored-result STR tail",
          "case0x289:" in cases and "goto$L146219;" in cases
          and "$L146219:CRagEffect::ProcessEZ2STR(this,0.0,-80.0,0.0);return1;" in compact)

check("local effect649 ignores ProcessEZ2STR completion and returns true",
      "case 649: ProcessEZ2STR(); return true;" in DISPATCH)
check("local shared STR processor can report false completion for caller to ignore",
      "if (m_iCurLayer >= m_cEndLayer) {\n        return false;" in LOCAL)
print("effect649 CP949 STR/ignored-completion comparison passed")
