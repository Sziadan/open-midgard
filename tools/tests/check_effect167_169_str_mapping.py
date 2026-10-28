"""Verify the shared ProcessEZ2STR filename map for effects167-169."""
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


ref_init = body(REF, "void __thiscall CRagEffect::Init(CRagEffect *this, CRenderObject *master, int effectId, vector3d fDeltaPos)")
pe_init = body(PE, "void __thiscall CRagEffect::Init(CRagEffect *this, CRenderObject *master, int effectId, vector3d fDeltaPos)")
main = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
symbols = SymbolTable(ExeImage())
expected = {
    167: ("TamingSuccess.str", "aTamingsuccessS", 0x71ACA0),
    168: ("TamingFailed.str", "aTamingfailedSt", 0x71AC8C),
    169: ("EnergyCoat.str", "aEnergycoatStr", 0x71AC7C),
}

for effect_id, (filename, symbol, va) in expected.items():
    ref_case = re.search(rf"case\s+{effect_id}\s*:\s*sprintf\(StrName,\s*(\w+)\);\s*break;", ref_init)
    pe_case = re.search(rf"case\s+{effect_id}\s*:\s*sprintf\(StrName,\s*(\w+)\);\s*break;", pe_init)
    if not ref_case or ref_case.group(1) != symbol:
        raise SystemExit(f"primary Ref Init does not map {effect_id} to {symbol}")
    if not pe_case or pe_case.group(1) != symbol:
        raise SystemExit(f"canonical PE Init does not map {effect_id} to {symbol}")
    resolved = symbols.resolve_name(symbol)
    if resolved is None or (resolved.va, resolved.text) != (va, filename):
        raise SystemExit(f"{symbol} resolved as {resolved!r}, expected 0x{va:X} {filename}")
    for table, label in ((CATALOG, "catalog"), (STR_TABLE, "STR table")):
        if not re.search(rf'\{{\s*{effect_id},\s*"{re.escape(filename)}"', table):
            raise SystemExit(f"local {label} does not map {effect_id} to {filename}")
    if f"case {effect_id}: return ProcessEZ2STR();" not in DISPATCH:
        raise SystemExit(f"local effect{effect_id} does not route to ProcessEZ2STR")

group = main[main.index("          case 141:"):main.index("          case 24:")]
if not all(f"case {effect_id}:" in group for effect_id in expected) or "goto LABEL_424;" not in group:
    raise SystemExit("canonical OnProcess cases167-169 no longer share the STR path")
label = main[main.index("LABEL_424:"):main.index("LABEL_425:")]
if "CRagEffect::ProcessEZ2STR(this, 0.0, -80.0, 0.0);" not in label:
    raise SystemExit("canonical shared label no longer calls ProcessEZ2STR with the source offset")

print("effects167-169 parity: Ref/PE Init map, PE strings, catalog/STR tables, routes and shared ProcessEZ2STR caller passed")
