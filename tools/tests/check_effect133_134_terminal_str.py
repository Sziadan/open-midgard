#!/usr/bin/env python3
"""Compare effects133/134 terminal STR state and resource mappings."""
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


pe_init = body(PE, "void __thiscall CRagEffect::Init(CRagEffect *this, CRenderObject *master, int effectId, vector3d fDeltaPos)")
ref_init = body(REF, "void __thiscall CRagEffect::Init(CRagEffect *this, CRenderObject *master, int effectId, vector3d fDeltaPos)")
for effect_id, symbol, name in ((133, "aFreezeStr", "Freeze.str"), (134, "aFreezedStr", "Freezed.str")):
    for source_name, init in (("PE", pe_init), ("primary Ref", ref_init)):
        match = re.search(rf"\bcase\s+{effect_id}\s*:[\s\S]*?\bbreak\s*;", init)
        assert match
        check(f"{source_name} Init maps effect{effect_id} to {name}",
              f"sprintf(StrName,{symbol})" in re.sub(r"\s+", "", match.group(0)))
    check(f"local STR table preserves effect{effect_id} mapping",
          f'{{ {effect_id}, "{name}", nullptr, nullptr, 0, 0 }}' in STR_TABLE)

pe_process = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
pe_cases = re.search(r"\bcase\s+133\s*:[\s\S]*?\bcase\s+136\s*:", pe_process)
assert pe_cases
pe_compact = re.sub(r"\s+", "", pe_cases.group(0))
for token in (
    "case133:case134:",
    "result=CRagEffect::ProcessEZ2STR(this,0.0,-80.0,0.0)",
    "if(result)break",
    "this->m_stateCnt=9999",
    "returnresult",
):
    check(f"PE effects133/134 terminal handler preserves {token}", token in pe_compact)

ref_init_compact = re.sub(r"\s+", "", ref_init)
check("primary Ref Init keeps distinct Freeze and Freezed mappings",
      "case133:sprintf(StrName,aFreezeStr);break;case134:sprintf(StrName,aFreezedStr);break;" in ref_init_compact)

for effect_id, va, expected in ((133, 0x71AE98, b"Freeze.str"), (134, 0x71AE8C, b"Freezed.str")):
    image = ExeImage(ROOT / "HighPriest.exe")
    resolved = image.read_cstring_at(va)
    check(f"PE resource bytes for effect{effect_id} at {va:#x} are exact",
          resolved is not None and resolved.raw == expected)
    check(f"local effect{effect_id} dispatcher selects terminal STR handler",
          f"case {effect_id}: return ProcessStrTerminalEffect();" in DISPATCH)

terminal = body(LOCAL, "bool CRagEffect::ProcessStrTerminalEffect()")
terminal_compact = re.sub(r"\s+", "", terminal)
check("local terminal handler matches PE continuation and completion transition",
      "if(ProcessEZ2STR(0.0f,-80.0f,0.0f))returntrue;m_stateCnt=9999;returnfalse;" in terminal_compact)

print("effects133/134 terminal STR Init/caller/resource/local route comparison passed")
