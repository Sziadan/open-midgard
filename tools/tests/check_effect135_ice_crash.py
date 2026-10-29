#!/usr/bin/env python3
"""Compare effect135 IceCrash generic STR routing and resource mapping."""
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
pe_init = body(PE, init_marker)
ref_init = body(REF, init_marker)
for source_name, init in (("PE", pe_init), ("primary Ref", ref_init)):
    match = re.search(r"\bcase\s+135\s*:[\s\S]*?\bbreak\s*;", init)
    assert match
    check(f"{source_name} Init maps effect135 to IceCrash.str",
          "sprintf(StrName,aIcecrashStr)" in re.sub(r"\s+", "", match.group(0)))
check("local STR table preserves IceCrash.str",
      '{ 135, "IceCrash.str", nullptr, nullptr, 0, 0 }' in STR_TABLE)

pe_process = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
pe_generic_group = re.search(r"case\s+126\s*:[\s\S]*?goto\s+LABEL_424\s*;", pe_process)
assert pe_generic_group
pe_group_compact = re.sub(r"\s+", "", pe_generic_group.group(0))
check("PE generic STR caller includes effect135 in the direct LABEL_424 group",
      "case135:" in pe_group_compact and "case135:case139:" in pe_group_compact
      and "gotoLABEL_424;" in pe_group_compact)
check("PE shared STR tail propagates ProcessEZ2STR(0,-80,0) result",
      "LABEL_424:result=CRagEffect::ProcessEZ2STR(this,0.0,-80.0,0.0);LABEL_425:if(!result)returnresult;break;"
      in re.sub(r"\s+", "", pe_process))

image = ExeImage(ROOT / "HighPriest.exe")
resolved = image.read_cstring_at(0x71AE7C)
check("PE string bytes at 0x71AE7C are exact IceCrash.str",
      resolved is not None and resolved.raw == b"IceCrash.str")

check("local generic STR dispatcher preserves the effect135 helper route",
      "case 135: return ProcessEZ2STR();" in DISPATCH)
ref_str_cases = body(LOCAL, "std::optional<bool> CRagEffect::ProcessReferenceStrOnly()")
ref_compact = re.sub(r"\s+", "", ref_str_cases)
check("local source-parity group preserves effect135 and the exact STR offsets",
      "case126:case135:case139:" in ref_compact
      and "returnProcessEZ2STR(0.0f,-80.0f,0.0f);" in ref_compact)

print("effect135 IceCrash generic STR Init/caller/resource/local route comparison passed")
