#!/usr/bin/env python3
"""Compare effect646's CP949 STR mapping and ignored completion result."""
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


expected = bytes.fromhex("C6 AE B7 A2 C5 B7 2E 73 74 72")
for name, source in (("PE", PE), ("primary Ref", REF)):
    init = body(source, "void __thiscall CRagEffect::Init(CRagEffect *this, CRenderObject *master, int effectId, vector3d fDeltaPos)")
    case = re.search(r"\bcase\s+646\s*:[\s\S]*?\bbreak\s*;", init)
    check(f"{name} Init selects the effect646 CP949 STR symbol",
          case is not None and "sprintf(StrName, byte_71A89C);" in case.group(0))

escaped = r"\xC6\xAE\xB7\xA2\xC5\xB7.str"
for name, source in (("local catalog", CATALOG), ("local STR table", STR_TABLE)):
    check(f"{name} maps effect646 to the same CP949 STR bytes",
          f'{{ 646, "{escaped}", nullptr, nullptr, 0, 0 }}' in source)

image = ExeImage(ROOT / "HighPriest.exe")
resolved = image.read_cstring_at(0x71A89C)
check("PE bytes at VA 0x71a89c equal the CP949 STR filename",
      resolved is not None and resolved.raw == expected)

on_process = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
ref_process = body(REF, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
for name, source in (("PE", on_process), ("primary Ref", ref_process)):
    compact = re.sub(r"\s+", "", source)
    start = compact.index("case0x1EB:")
    end = compact.index("case0x1ED:", start)
    case = compact[start:end]
    check(f"{name} effect646 selects the shared STR tail",
          "case0x286:" in case and "goto$L146219;" in case)
    check(f"{name} shared STR tail ignores completion and returns true",
          "$L146219:CRagEffect::ProcessEZ2STR(this,0.0,-80.0,0.0);return1;" in compact)

check("local OnProcess3 dispatch runs ProcessEZ2STR and returns true regardless of its result",
      "case 646: ProcessEZ2STR(); return true;" in DISPATCH)
check("local ProcessEZ2STR preserves its false completion result for the caller to ignore",
      "if (m_iCurLayer >= m_cEndLayer) {\n        return false;" in LOCAL)

print("effect646 CP949 STR/ignored-completion comparison passed")
