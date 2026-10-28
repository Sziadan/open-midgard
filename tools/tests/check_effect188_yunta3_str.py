"""Verify effect188 Yunta3 STR mapping and shared caller."""
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
    if not re.search(r"case\s+188\s*:\s*sprintf\(StrName,\s*aYunta3Str\);\s*break;", init):
        raise SystemExit(f"{label} Init does not map effect188 to aYunta3Str")

resolved = SymbolTable(ExeImage()).resolve_name("aYunta3Str")
if resolved is None or (resolved.va, resolved.text) != (0x71ABDC, "yunta_3.str"):
    raise SystemExit(f"aYunta3Str resolved as {resolved!r}, expected 0x71ABDC yunta_3.str")
for table, label in ((CATALOG, "catalog"), (STR_TABLE, "STR table")):
    if not re.search(r'\{\s*188,\s*"yunta_3\.str"', table):
        raise SystemExit(f"local {label} does not map effect188 to yunta_3.str")
if "case 188: return ProcessEZ2STR();" not in DISPATCH:
    raise SystemExit("local effect188 route is missing")

main = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
group = main[main.index("          case 158:"):main.index("          case 24:")]
if "case 188:" not in group or "goto LABEL_424;" not in group:
    raise SystemExit("canonical OnProcess effect188 no longer enters the shared STR path")
label = main[main.index("LABEL_424:"):main.index("LABEL_425:")]
if "CRagEffect::ProcessEZ2STR(this, 0.0, -80.0, 0.0);" not in label:
    raise SystemExit("canonical shared STR caller changed its Y offset")

print("effect188 parity: Ref/PE Init map, executable bytes, local tables/route and shared STR caller passed")
