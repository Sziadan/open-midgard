"""Verify effect187's combo STR map and record the primary Ref omission."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage, SymbolTable  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
SECONDARY_REF = (ROOT / "Ref/dhxj/RagEffect.cpp").read_text(encoding="latin-1")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
CATALOG = (ROOT / "src/world/RagEffectCatalog.generated.inc").read_text(encoding="utf-8")
STR_TABLE = (ROOT / "src/world/RagEffectStrNameTable.generated.inc").read_text(encoding="utf-8")


def body(text: str, signature: str) -> str:
    start = text.rindex(signature)
    opening = text.index("{", start + len(signature))
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{": depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[opening:pos + 1]
    raise AssertionError(f"unterminated function {signature}")


signature = "void __thiscall CRagEffect::Init(CRagEffect *this, CRenderObject *master, int effectId, vector3d fDeltaPos)"
primary_init = body(REF, signature)
pe_init = body(PE, signature)
secondary_init = body(SECONDARY_REF, "void\tCRagEffect :: Init(CRenderObject * master, int effectId, vector3d fDeltaPos )")
if re.search(r"case\s+187\s*:", primary_init):
    raise SystemExit("primary Ref unexpectedly contains effect187; review this version discrepancy")
if not re.search(r"v6\s*==\s*187\s*\)\s*\{\s*sprintf\(StrName,\s*aYunta2Str\);", pe_init):
    raise SystemExit("canonical PE Init no longer maps effect187 to aYunta2Str")
if not re.search(r"case\s+EF_COMBOATTACK2\s*:\s*sprintf\(StrName,\s*\"yunta_2\.str\"\);\s*break;", secondary_init):
    raise SystemExit("secondary Ref Init no longer maps effect187 to yunta_2.str")

resolved = SymbolTable(ExeImage()).resolve_name("aYunta2Str")
if resolved is None or (resolved.va, resolved.text) != (0x71AC08, "yunta_2.str"):
    raise SystemExit(f"aYunta2Str resolved as {resolved!r}, expected 0x71AC08 yunta_2.str")
for table, label in ((CATALOG, "catalog"), (STR_TABLE, "STR table")):
    if not re.search(r'\{\s*187,\s*"yunta_2\.str"', table):
        raise SystemExit(f"local {label} does not map effect187 to yunta_2.str")
if "case 187: return ProcessEZ2STR();" not in DISPATCH:
    raise SystemExit("local effect187 route is missing")

main = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
group = main[main.index("          case 158:"):main.index("          case 24:")]
if "case 187:" not in group or "goto LABEL_424;" not in group:
    raise SystemExit("canonical OnProcess effect187 no longer enters the shared STR path")
label = main[main.index("LABEL_424:"):main.index("LABEL_425:")]
if "CRagEffect::ProcessEZ2STR(this, 0.0, -80.0, 0.0);" not in label:
    raise SystemExit("canonical shared STR caller changed its Y offset")

print("effect187 parity: PE/secondary-Ref map, primary-Ref omission, executable bytes, local tables/route and shared caller passed")
