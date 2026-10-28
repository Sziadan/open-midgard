"""Verify effects204-207/210 STR resources and shared heal-wave caller."""
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
    204: ("빨간포션.str", "byte_71AB6C", 0x71AB6C),
    205: ("주홍포션.str", "byte_71AB5C", 0x71AB5C),
    206: ("노란포션.str", "byte_71AB4C", 0x71AB4C),
    207: ("하얀포션.str", "byte_71AB3C", 0x71AB3C),
    210: ("fruit.str", "aFruitStr", 0x71AB10),
}
symbols = SymbolTable(ExeImage())
for effect_id, (filename, symbol, va) in expected.items():
    for label, init in (("primary Ref", ref_init), ("canonical PE", pe_init)):
        if not re.search(rf"case\s+{effect_id}\s*:\s*sprintf\(StrName,\s*{symbol}\);\s*break;", init):
            raise SystemExit(f"{label} Init does not map effect{effect_id} to {symbol}")
    resolved = symbols.resolve_name(symbol)
    expected_raw = filename.encode("cp949") if effect_id != 210 else filename.encode("ascii")
    if resolved is None or resolved.va != va or resolved.raw != expected_raw:
        actual = None if resolved is None else (hex(resolved.va), resolved.raw.hex())
        raise SystemExit(f"{symbol} resolved as {actual!r}, expected VA 0x{va:X} bytes {expected_raw.hex()}")
    for table, label in ((CATALOG, "catalog"), (STR_TABLE, "STR table")):
        if effect_id != 210:
            stem, extension = filename.rsplit(".", 1)
            table_name = "".join(f"\\x{byte:02X}" for byte in stem.encode("cp949")) + "." + extension
        else:
            table_name = filename
        if f'{{ {effect_id}, "{table_name}"' not in table:
            raise SystemExit(f"local {label} does not map effect{effect_id} to the expected STR filename")
    if f"case {effect_id}: return ProcessClipWaveStrEffect();" not in DISPATCH:
        raise SystemExit(f"local effect{effect_id} route is missing")

main = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
group = main[main.index("          case 204:"):main.index("          case 208:")]
if not all(f"case {effect_id}:" in group for effect_id in expected) or "goto LABEL_424;" not in group:
    raise SystemExit("canonical OnProcess cases204-207/210 changed their shared STR branch")
for token in (
    "if ( !this->m_stateCnt )", "this->m_pos = (*p_m_master)->m_pos;",
    "PlayWave(aHealEffectWav, xda, -80.0, zda, 250, 40, 1.0);",
):
    if token not in group:
        raise SystemExit(f"canonical clip wave caller is missing {token}")

wave = symbols.resolve_name("aHealEffectWav")
if wave is None or (wave.va, wave.text) != (0x712840, "_heal_effect.wav"):
    raise SystemExit(f"aHealEffectWav resolved as {wave!r}, expected 0x712840 _heal_effect.wav")
local = body(SOURCE, "bool CRagEffect::ProcessClipWaveStrEffect()")
if not re.search(r"case\s+204:\s*case\s+205:\s*case\s+206:\s*case\s+207:\s*case\s+210:\s*wave\s*=\s*\"_heal_effect\.wav\";\s*yOffset\s*=\s*-80\.0f;\s*copyMasterPosition\s*=\s*true;", local):
    raise SystemExit("local shared helper changed the wave/offset/position setup for these effects")

print("effects204-207/210 parity: Ref/PE STR maps, CP949/ASCII resource bytes, wave caller, local route/helper passed")
