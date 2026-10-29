"""Compare effect29 Lightning STR route, Init mapping and fixture binding."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage  # noqa: E402

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


process = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
case29 = process.index("case 29:")
case30 = process.index("case 30:", case29)
if "goto LABEL_424;" not in process[case29:case30]:
    raise SystemExit("canonical PE effect29 no longer enters the shared STR-only tail")
if "result = CRagEffect::ProcessEZ2STR(this, 0.0, -80.0, 0.0);" not in process:
    raise SystemExit("canonical PE STR-only offset or result propagation changed")
if "case 29: return ProcessEZ2STR();" not in DISPATCH:
    raise SystemExit("local generated effect29 route is missing")
str_only = body(SRC, "std::optional<bool> CRagEffect::ProcessReferenceStrOnly()")
if "case 29:" not in str_only or "return ProcessEZ2STR(0.0f,-80.0f,0.0f);" not in str_only:
    raise SystemExit("local effect29 STR-only mapping or offset changed")

pe_init = body(PE, "void __thiscall CRagEffect::Init(CRagEffect *this, CRenderObject *master, int effectId, vector3d fDeltaPos)")
ref_init = body(REF, "void __thiscall CRagEffect::Init(CRagEffect *this, CRenderObject *master, int effectId, vector3d fDeltaPos)")
for source_name, init in (("PE", pe_init), ("primary Ref", ref_init)):
    if not re.search(r"case\s+29\s*:\s*sprintf\(StrName,\s*aLightningStr\)", init):
        raise SystemExit(f"{source_name} effect29 Init mapping to Lightning.str is missing")
for table_name, table in (("catalog", CATALOG), ("STR name table", TABLE)):
    if '{ 29, "Lightning.str", nullptr, nullptr, 0, 0 }' not in table:
        raise SystemExit(f"local effect29 {table_name} mapping changed")

value = ExeImage(ROOT / "HighPriest.exe").read_cstring_at(0x71ACD0)
if value is None or value.raw != b"Lightning.str":
    raise SystemExit(f"effect29 PE string bytes differ at VA 0x71ACD0: {value!r}")

ids_start = FIXTURE.index("const int ids[]={23,25,29")
ids_end = FIXTURE.index("};", ids_start)
ids = FIXTURE[ids_start:ids_end]
if not re.search(r"\b29\b", ids):
    raise SystemExit("generic STR-only lifecycle fixture no longer includes effect29")
for phrase in (
    "original STR-only case is handled and missing clip terminates without projection",
    "original STR-only case returns active clip result with exact minus-eighty offset despite reduced effects",
    "original STR-only case follows moving actor in normal mode while preserving parent origin duration and empty primitive list",
    "original STR-only case distinguishes handled clip exhaustion from unhandled dispatch",
    "all forty-five original STR-only cases perform no extra RNG audio primitive or resource work during processing",
):
    if phrase not in FIXTURE:
        raise SystemExit(f"effect29 STR lifecycle fixture missing {phrase!r}")

print("effect29 PE/local STR-only route, PE/Ref Init map, executable bytes and lifecycle coverage passed")
