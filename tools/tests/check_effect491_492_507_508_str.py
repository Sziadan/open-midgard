#!/usr/bin/env python3
"""Compare effects491/492/507/508 shared STR-only behavior and resources."""
from __future__ import annotations

import ast
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
RAG = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
HEADER = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8", errors="replace")
DISPATCH = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")
CATALOG = (ROOT / "src/world/RagEffectCatalog.generated.inc").read_text(encoding="utf-8")
STR_TABLE = (ROOT / "src/world/RagEffectStrNameTable.generated.inc").read_text(encoding="utf-8")
EFFECTS = {
    491: (0x1EB, "byte_71A930", 0x71A930),
    492: (0x1EC, "aRamadanStr", 0x71A8F0),
    507: (0x1FB, "aMapaeStr", 0x71A8E4),
    508: (0x1FC, "aItempokjukStr", 0x71A804),
}


def function_body(source: str, signature: str, last: bool = False) -> str:
    matches = list(re.finditer(signature, source))
    if not matches:
        raise AssertionError(f"missing {signature}")
    match = matches[-1] if last else matches[0]
    opening = source.find("{", match.end())
    depth = 0
    for offset in range(opening, len(source)):
        if source[offset] == "{":
            depth += 1
        elif source[offset] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:offset]
    raise AssertionError(f"unterminated {signature}")


pe_dispatch = function_body(PE, r"__thiscall CRagEffect::OnProcess3\(CRagEffect \*this\)", last=True)
ref_dispatch = function_body(REF, r"unsigned __int8 __thiscall CRagEffect::OnProcess3\(CRagEffect \*this\)")
route = re.compile(r"case\s+0x1EB:\s*case\s+0x1EC:\s*case\s+0x1FB:\s*case\s+0x1FC:\s*case\s+0x286:")
for label, dispatch in (("PE", pe_dispatch), ("Ref", ref_dispatch)):
    if not route.search(dispatch):
        raise AssertionError(f"{label} effects491/492/507/508 no longer share the STR-only dispatch group")
    if not re.search(r"\$L146219:\s*CRagEffect::ProcessEZ2STR\(this,\s*0\.0,\s*-80\.0,\s*0\.0\);\s*return 1", dispatch):
        raise AssertionError(f"{label} shared STR-only tail must ignore completion and return true")

pe_init = function_body(PE, r"__thiscall CRagEffect::Init\(CRagEffect \*this, CRenderObject \*master, int effectId, vector3d fDeltaPos\)", last=True)
ref_init = function_body(REF, r"void __thiscall CRagEffect::Init\(CRagEffect \*this, CRenderObject \*master, int effectId, vector3d fDeltaPos\)")
image = ExeImage()
for effect_id, (case_id, symbol, va) in EFFECTS.items():
    expected = image.read_cstring_at(va)
    if expected is None:
        raise AssertionError(f"PE Init resource at VA {va:#x} is unreadable")
    for label, init in (("PE", pe_init), ("Ref", ref_init)):
        if effect_id == 508:
            mapping = re.search(r"case\s+508:\s*goto\s+\$L147927_0;", init)
            shared = re.search(r"\$L147927_0:\s*sprintf\(StrName,\s*aItempokjukStr\);", init)
            if mapping is None or shared is None:
                raise AssertionError(f"{label} Init effect508 no longer maps through itempokjuk STR label")
        else:
            if not re.search(rf"case\s+{effect_id}:\s*sprintf\(StrName,\s*{symbol}\);\s*break;", init):
                raise AssertionError(f"{label} Init effect{effect_id} resource symbol changed")
    for label, table in (("catalog", CATALOG), ("STR table", STR_TABLE)):
        row = re.search(rf"\{{\s*{effect_id},\s*(\"(?:\\.|[^\"\\])*\")", table)
        if row is None:
            raise AssertionError(f"local {label} effect{effect_id} row is missing")
        local_bytes = ast.literal_eval(row.group(1)).encode("latin-1")
        if local_bytes != expected.raw:
            raise AssertionError(f"local {label} effect{effect_id} bytes differ from PE VA {va:#x}")
    if not re.search(rf"case\s+{effect_id}:\s*ProcessEZ2STR\(\);\s*return true;", DISPATCH):
        raise AssertionError(f"local effect{effect_id} must ignore STR completion and return true")

if not re.search(r"bool ProcessEZ2STR\(float x\s*=\s*0\.0f,\s*float y\s*=\s*-80\.0f,\s*float z\s*=\s*0\.0f\)", HEADER):
    raise AssertionError("local no-argument ProcessEZ2STR offsets must remain (0,-80,0)")

print("effects491/492/507/508 source check passed: ignored STR completion, return-true contract and four PE-backed Init resources")
