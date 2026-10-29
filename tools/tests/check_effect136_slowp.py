#!/usr/bin/env python3
"""Compare effect136 SlowP shared STR caller and resource mapping."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
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
    case = re.search(r"\bcase\s+136\s*:[\s\S]*?\bbreak\s*;", init)
    assert case
    check(f"{source_name} Init maps effect136 to slowp.str",
          "sprintf(StrName,aSlowpStr)" in re.sub(r"\s+", "", case.group(0)))
check("local STR table preserves effect136 slowp.str",
      '{ 136, "slowp.str", nullptr, nullptr, 0, 0 }' in STR_TABLE)

pe_process = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
pe_case = re.search(r"\bcase\s+136\s*:[\s\S]*?\bgoto\s+LABEL_99\s*;", pe_process)
assert pe_case
check("PE effect136 shares LABEL_99 with cases406/407",
      re.sub(r"\s+", "", pe_case.group(0)) == "case136:case406:case407:gotoLABEL_99;")
compact_process = re.sub(r"\s+", "", pe_process)
check("PE LABEL_99 calls ProcessEZ2STR(0,-80,0) and discards its result",
      "LABEL_99:CRagEffect::ProcessEZ2STR(this,0.0,-80.0,0.0);break;" in compact_process)

image = ExeImage(ROOT / "HighPriest.exe")
resolved = image.read_cstring_at(0x71AC14)
check("PE bytes at 0x71AC14 are exact slowp.str",
      resolved is not None and resolved.raw == b"slowp.str")

check("local effect136 route ignores the STR completion result",
      "case 136: ProcessEZ2STR(); return true;" in DISPATCH)
check("local ProcessEZ2STR default offset is (0,-80,0)",
      "bool ProcessEZ2STR(float x = 0.0f, float y = -80.0f, float z = 0.0f);"
      in (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8"))
lifecycle = (ROOT / "tools/tests/EffectLifecycleTests.cpp").read_text(encoding="utf-8")
ignored = re.search(r"const int ignored\[\]\s*=\s*\{([^}]+)\};", lifecycle)
ignored_ids = set(map(int, re.findall(r"\d+", ignored.group(1)))) if ignored else set()
check("lifecycle group checks effect136 and effects491/492/507/508 continue after missing STR",
      {136, 491, 492, 507, 508} <= ignored_ids)

print("effect136 SlowP shared STR caller/resource/local route comparison passed")
