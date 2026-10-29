"""Compare effects52/64/70 STR-only routes and executable/Ref Init maps."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
SRC = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
CATALOG = (ROOT / "src/world/RagEffectCatalog.generated.inc").read_text(encoding="utf-8")
TABLE = (ROOT / "src/world/RagEffectStrNameTable.generated.inc").read_text(encoding="utf-8")
FIXTURE = (ROOT / "tools/tests/EffectLifecycleTests.cpp").read_text(encoding="utf-8")


def body(text: str, signature: str) -> str:
    begin = text.rindex(signature)
    opening = text.index("{", begin)
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[begin : pos + 1]
    raise AssertionError(f"unterminated body: {signature}")


pe_process = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
str_group_start = pe_process.index("case 23:")
str_group_end = pe_process.index("case 24:", str_group_start)
str_group = pe_process[str_group_start:str_group_end]
for effect in (52, 64, 70):
    if not re.search(rf"\bcase\s+{effect}\s*:", str_group):
        raise SystemExit(f"PE effect{effect} is missing from the grouped STR route")
if "goto LABEL_424;" not in str_group:
    raise SystemExit("PE grouped STR route no longer reaches the shared STR tail")
if not re.search(r"LABEL_424:\s*result\s*=\s*CRagEffect::ProcessEZ2STR\(this,\s*0\.0,\s*-80\.0,\s*0\.0\);", pe_process):
    raise SystemExit("PE shared STR tail offset or completion-result propagation changed")

# Primary Ref's OnProcess implementation omits these ids and returns true from
# its default. Keep this difference explicit: the local route follows PE.
ref_signature = "unsigned __int8 __thiscall CRagEffect::OnProcess2(CRagEffect *this)"
ref_start = REF.index(ref_signature)
ref_process = body(REF[ref_start:], ref_signature)
for effect in (52, 64, 70):
    if re.search(rf"\bcase\s+(?:{effect}|0x{effect:x})\s*:", ref_process):
        raise SystemExit(f"primary Ref OnProcess no longer omits effect{effect}; re-audit the source comparison")
if not re.search(r"default\s*:\s*return\s+1;", ref_process):
    raise SystemExit("primary Ref OnProcess default behavior changed; re-audit effects52/64/70")

local_str = body(SRC, "std::optional<bool> CRagEffect::ProcessReferenceStrOnly()")
if "case 52: case 64: case 70:" not in local_str or "return ProcessEZ2STR(0.0f,-80.0f,0.0f);" not in local_str:
    raise SystemExit("local STR-only routes or source offset changed")
for effect in (52, 64, 70):
    if f"case {effect}: return ProcessEZ2STR();" not in DISPATCH:
        raise SystemExit(f"local generated dispatch route for effect{effect} is missing")

pe_init = body(PE, "void __thiscall CRagEffect::Init(CRagEffect *this, CRenderObject *master, int effectId, vector3d fDeltaPos)")
ref_init = body(REF, "void __thiscall CRagEffect::Init(CRagEffect *this, CRenderObject *master, int effectId, vector3d fDeltaPos)")
for label, init in (("PE", pe_init), ("primary Ref", ref_init)):
    if not re.search(r'case\s+52\s*:\s*v\w+\s*=\s*rand\(\)\s*;\s*sprintf\(StrName,\s*"WindHit%d\.str",\s*v\w+\s*%\s*3\s*\+\s*1\s*\);', init):
        raise SystemExit(f"{label} effect52 WindHit1..3 mapping or single RNG draw changed")
    if not re.search(r"case\s+64\s*:\s*sprintf\(StrName,\s*aArrowshotStr\);", init):
        raise SystemExit(f"{label} effect64 ArrowShot.str mapping is missing")
    if not re.search(r"case\s+70\s*:\s*sprintf\(StrName,\s*aBrandishStr\);", init):
        raise SystemExit(f"{label} effect70 Brandish.str mapping is missing")
for table_name, table in (("catalog", CATALOG), ("STR name table", TABLE)):
    expected = (
        '{ 52, nullptr, nullptr, "WindHit", 1, 3 }',
        '{ 64, "ArrowShot.str", nullptr, nullptr, 0, 0 }',
        '{ 70, "Brandish.str", nullptr, nullptr, 0, 0 }',
    )
    if not all(entry in table for entry in expected):
        raise SystemExit(f"local {table_name} entries for effects52/64/70 changed")
for effect in (52, 64, 70):
    if not re.search(rf"\b{effect}\b", FIXTURE):
        raise SystemExit(f"existing pure STR lifecycle fixture no longer names effect{effect}")
for phrase in (
    "original STR-only case returns active clip result with exact minus-eighty offset despite reduced effects",
    "all forty-five original STR-only cases perform no extra RNG audio primitive or resource work during processing",
):
    if phrase not in FIXTURE:
        raise SystemExit(f"STR lifecycle coverage phrase missing: {phrase}")

print("effects52/64/70 PE/local STR routes, PE/Ref Init maps, Ref omission and lifecycle coverage passed")
