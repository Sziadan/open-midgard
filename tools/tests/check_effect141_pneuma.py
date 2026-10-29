#!/usr/bin/env python3
"""Compare effect141 Pneuma variant Init, STR caller, and resources."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
CATALOG = (ROOT / "src/world/RagEffectCatalog.generated.inc").read_text(encoding="utf-8")
LIFECYCLE = (ROOT / "tools/tests/EffectLifecycleTests.cpp").read_text(encoding="utf-8")

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
    case = re.search(r"\bcase\s+141\s*:[\s\S]*?\bbreak\s*;", init)
    assert case
    compact = re.sub(r"\s+", "", case.group(0))
    check(f"{source_name} Init selects Pneuma variants with one modulo-three RNG draw",
          compact.count("rand()") == 1
          and "%3+1" in compact
          and ("Pneuma%d.str" in compact or "aPneumaStr" in compact))

image = ExeImage(ROOT / "HighPriest.exe")
resolved = image.read_cstring_at(0x71AE60)
check("PE bytes at 0x71AE60 are exact Pneuma%d.str format string",
      resolved is not None and resolved.raw == b"Pneuma%d.str")

check("local catalog defines variants Pneuma1.str through Pneuma3.str",
      '{ 141, nullptr, nullptr, "Pneuma", 1, 3 }' in CATALOG)
resolver = body(LOCAL, "std::string ResolveCatalogStrName(")
resolver_compact = re.sub(r"\s+", "", resolver)
check("local catalog resolver consumes one rand modulo variantCount",
      "constintoffset=rand()%entry.variantCount;" in resolver_compact)
check("local catalog resolver formats prefix plus first+offset",
      '"%s%d.str",entry.variantPrefix,entry.variantFirst+offset' in resolver_compact)

pe_process = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
pe_group = re.search(r"case\s+139\s*:[\s\S]*?goto\s+LABEL_424\s*;", pe_process)
assert pe_group
pe_compact = re.sub(r"\s+", "", pe_group.group(0))
check("PE effect141 joins generic STR caller group and propagates its result",
      "case141:" in pe_compact and pe_compact.endswith("gotoLABEL_424;")
      and "LABEL_424:result=CRagEffect::ProcessEZ2STR(this,0.0,-80.0,0.0);LABEL_425:if(!result)returnresult;break;"
      in re.sub(r"\s+", "", pe_process))
check("local effect141 dispatch propagates ProcessEZ2STR completion",
      "case 141: return ProcessEZ2STR();" in DISPATCH
      and "case141:" in re.sub(r"\s+", "", body(LOCAL, "std::optional<bool> CRagEffect::ProcessReferenceStrOnly()")))
check("existing lifecycle coverage includes effect141 STR playback",
      "126,135,139,141,143" in LIFECYCLE)
check("lifecycle fixture covers all three variant outcomes and exact RNG count",
      '"Pneuma1.str","Pneuma2.str","Pneuma3.str"' in LIFECYCLE
      and "rngCursor==1" in LIFECYCLE)

print("effect141 Pneuma variant Init/caller/resource/local route comparison passed")
