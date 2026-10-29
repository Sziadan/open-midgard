"""Compare effects23/25 grouped STR-only routes and Init mappings."""
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
case23 = pe_process.index("case 23:")
case24 = pe_process.index("case 24:", case23)
group = pe_process[case23:case24]
if not re.search(r"case\s+23\s*:\s*case\s+25\s*:", group) or "goto LABEL_424;" not in group:
    raise SystemExit("canonical PE effect23/25 cases no longer share the STR-only tail")
if "result = CRagEffect::ProcessEZ2STR(this, 0.0, -80.0, 0.0);" not in pe_process:
    raise SystemExit("canonical PE shared STR-only offset or completion result changed")
if "case 23: return ProcessEZ2STR();" not in DISPATCH or "case 25: return ProcessEZ2STR();" not in DISPATCH:
    raise SystemExit("local generated effect23/25 routes are missing")
str_only = body(SRC, "std::optional<bool> CRagEffect::ProcessReferenceStrOnly()")
if "case 23: case 25:" not in str_only or "return ProcessEZ2STR(0.0f,-80.0f,0.0f);" not in str_only:
    raise SystemExit("local effect23/25 STR-only mapping or offset changed")

pe_init = body(PE, "void __thiscall CRagEffect::Init(CRagEffect *this, CRenderObject *master, int effectId, vector3d fDeltaPos)")
ref_init = body(REF, "void __thiscall CRagEffect::Init(CRagEffect *this, CRenderObject *master, int effectId, vector3d fDeltaPos)")
for source_name, init in (("PE", pe_init), ("primary Ref", ref_init)):
    if "case 23:" not in init or "aStonecurseStr" not in init:
        raise SystemExit(f"{source_name} effect23 Init mapping to StoneCurse.str is missing")
    if "case 25:" not in init or "rand() % 2" not in init or '"Firewall%d.str"' not in init:
        raise SystemExit(f"{source_name} effect25 randomized Firewall Init mapping is missing")
for table_name, table in (("catalog", CATALOG), ("STR name table", TABLE)):
    if '{ 23, "StoneCurse.str", nullptr, nullptr, 0, 0 }' not in table:
        raise SystemExit(f"local effect23 {table_name} mapping changed")
    if '{ 25, nullptr, nullptr, "Firewall", 1, 2 }' not in table:
        raise SystemExit(f"local effect25 {table_name} mapping changed")

fixture = body(FIXTURE, "int main()")
ids_start = fixture.index("const int ids[]={23,25,29")
ids_end = fixture.index("};", ids_start)
ids = fixture[ids_start:ids_end]
if not all(re.search(rf"\b{effect}\b", ids) for effect in (23, 25)):
    raise SystemExit("existing STR-only lifecycle fixture no longer covers effects23 and25")
for phrase in (
    "original STR-only case is handled and missing clip terminates without projection",
    "original STR-only case returns active clip result with exact minus-eighty offset despite reduced effects",
    "original STR-only case follows moving actor in normal mode while preserving parent origin duration and empty primitive list",
    "original STR-only case distinguishes handled clip exhaustion from unhandled dispatch",
    "all forty-five original STR-only cases perform no extra RNG audio primitive or resource work during processing",
):
    if phrase not in FIXTURE:
        raise SystemExit(f"STR-only lifecycle fixture missing {phrase!r}")

print("effects23/25 grouped PE/local STR-only routes, Init maps and lifecycle coverage passed")
