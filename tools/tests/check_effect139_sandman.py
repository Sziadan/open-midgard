#!/usr/bin/env python3
"""Compare effect139 Sandman generic STR caller and resource mapping."""
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
    case = re.search(r"\bcase\s+139\s*:[\s\S]*?\bbreak\s*;", init)
    assert case
    check(f"{source_name} Init maps effect139 to SandMan.str",
          "sprintf(StrName,aSandmanStr)" in re.sub(r"\s+", "", case.group(0)))
check("local STR table preserves effect139 SandMan.str",
      '{ 139, "SandMan.str", nullptr, nullptr, 0, 0 }' in STR_TABLE)

pe_process = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
generic_group = re.search(r"case\s+135\s*:[\s\S]*?goto\s+LABEL_424\s*;", pe_process)
assert generic_group
group_compact = re.sub(r"\s+", "", generic_group.group(0))
check("PE effect139 is in the generic STR-only LABEL_424 group",
      "case139:" in group_compact and group_compact.endswith("gotoLABEL_424;"))
process_compact = re.sub(r"\s+", "", pe_process)
check("PE LABEL_424 propagates ProcessEZ2STR(0,-80,0) completion",
      "LABEL_424:result=CRagEffect::ProcessEZ2STR(this,0.0,-80.0,0.0);LABEL_425:if(!result)returnresult;break;"
      in process_compact)

image = ExeImage(ROOT / "HighPriest.exe")
resolved = image.read_cstring_at(0x71AE70)
check("PE bytes at 0x71AE70 are exact SandMan.str",
      resolved is not None and resolved.raw == b"SandMan.str")

check("local effect139 dispatcher propagates generic ProcessEZ2STR result",
      "case 139: return ProcessEZ2STR();" in DISPATCH)
local_group = body(LOCAL, "std::optional<bool> CRagEffect::ProcessReferenceStrOnly()")
local_compact = re.sub(r"\s+", "", local_group)
check("local source-parity group preserves effect139 and offsets (0,-80,0)",
      "case126:case135:case139:" in local_compact
      and "returnProcessEZ2STR(0.0f,-80.0f,0.0f);" in local_compact)
check("existing lifecycle matrix covers effect139 through STR terminal states",
      "167,168,169,181,182,186,187" in LIFECYCLE
      and "const int ids[]={23,25,29,52,64,70,109,110,126,135,139,141" in LIFECYCLE)

print("effect139 Sandman generic STR Init/caller/resource/local route comparison passed")
