"""Verify effect181 MentalBreak STR mapping and shared caller."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage, SymbolTable  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
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
for label, text in (("primary Ref", REF), ("canonical PE", PE)):
    init = body(text, signature)
    match = re.search(r"case\s+181\s*:\s*sprintf\(StrName,\s*(\w+)\);\s*break;", init)
    if not match or match.group(1) != "aMentalbreakStr":
        raise SystemExit(f"{label} Init does not map effect181 to aMentalbreakStr")

resolved = SymbolTable(ExeImage()).resolve_name("aMentalbreakStr")
if resolved is None or (resolved.va, resolved.text) != (0x71AC44, "MentalBreak.str"):
    raise SystemExit(f"aMentalbreakStr resolved as {resolved!r}, expected 0x71AC44 MentalBreak.str")
for table, label in ((CATALOG, "catalog"), (STR_TABLE, "STR table")):
    if not re.search(r'\{\s*181,\s*"MentalBreak\.str"', table):
        raise SystemExit(f"local {label} does not map effect181 to MentalBreak.str")
if "case 181: return ProcessEZ2STR();" not in DISPATCH:
    raise SystemExit("local effect181 route is missing")

main = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
start = main.index("          case 158:")
end = main.index("          case 24:", start)
if "case 181:" not in main[start:end] or "goto LABEL_424;" not in main[start:end]:
    raise SystemExit("canonical effect181 no longer enters the shared STR completion path")
label = main[main.index("LABEL_424:"):main.index("LABEL_425:")]
if "CRagEffect::ProcessEZ2STR(this, 0.0, -80.0, 0.0);" not in label:
    raise SystemExit("canonical shared STR label has changed its effect offset")

print("effect181 parity: Ref/PE Init map, executable bytes, local tables, route and shared STR caller passed")
