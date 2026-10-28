"""Verify the shared STR mappings and generic lifecycle coverage for effects189-195."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage, SymbolTable  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
SOURCE = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
CATALOG = (ROOT / "src/world/RagEffectCatalog.generated.inc").read_text(encoding="utf-8")
STR_TABLE = (ROOT / "src/world/RagEffectStrNameTable.generated.inc").read_text(encoding="utf-8")
LIFECYCLE = (ROOT / "tools/tests/EffectLifecycleTests.cpp").read_text(encoding="utf-8")


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
ref_init, pe_init = body(REF, signature), body(PE, signature)
expected = {
    189: ("yunta_4.str", "aYunta4Str", 0x71ABD0),
    190: ("yunta_5.str", "aYunta5Str", 0x71ABC4),
    191: ("homing.str", "aHomingStr", 0x71ABB8),
    192: ("poison.str", "aPoisonStr", 0x71ABAC),
    193: ("silence.str", "aSilenceStr", 0x71ABA0),
    194: ("stun.str", "aStunStr", 0x71AB94),
    195: ("StoneCurse.str", "aStonecurseStr", 0x71ACC0),
}
symbols = SymbolTable(ExeImage())
for effect_id, (filename, symbol, va) in expected.items():
    for label, init in (("primary Ref", ref_init), ("canonical PE", pe_init)):
        if not re.search(rf"case\s+{effect_id}\s*:\s*(?:\$L\d+\s*:\s*)?sprintf\(StrName,\s*{symbol}\);\s*break;", init):
            raise SystemExit(f"{label} Init does not map effect{effect_id} to {symbol}")
    resolved = symbols.resolve_name(symbol)
    if resolved is None or (resolved.va, resolved.text) != (va, filename):
        raise SystemExit(f"{symbol} resolved as {resolved!r}, expected 0x{va:X} {filename}")
    for table, label in ((CATALOG, "catalog"), (STR_TABLE, "STR table")):
        if not re.search(rf'\{{\s*{effect_id},\s*"{re.escape(filename)}"', table):
            raise SystemExit(f"local {label} does not map effect{effect_id} to {filename}")
    if f"case {effect_id}: return ProcessEZ2STR();" not in DISPATCH:
        raise SystemExit(f"local effect{effect_id} route is missing")

main = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
group = main[main.index("          case 158:"):main.index("          case 24:")]
if not all(f"case {effect_id}:" in group for effect_id in expected) or "goto LABEL_424;" not in group:
    raise SystemExit("canonical OnProcess cases189-195 no longer enter the shared STR path")
label = main[main.index("LABEL_424:"):main.index("LABEL_425:")]
if "CRagEffect::ProcessEZ2STR(this, 0.0, -80.0, 0.0);" not in label:
    raise SystemExit("canonical shared STR caller changed its Y offset")

if not re.search(r"const int ids\[\]\s*=\s*\{[^}]*\b189\s*,\s*190\s*,\s*191\s*,\s*192\s*,\s*193\s*,\s*194\s*,\s*195\b", LIFECYCLE, re.S):
    raise SystemExit("generic STR lifecycle ID matrix no longer includes effects189-195")
if "ProcessReferenceStrOnly()" not in LIFECYCLE:
    raise SystemExit("generic STR lifecycle fixture no longer exercises ProcessReferenceStrOnly")

print("effects189-195 parity: Ref/PE Init maps, executable bytes, local tables/routes, shared caller and generic lifecycle IDs passed")
