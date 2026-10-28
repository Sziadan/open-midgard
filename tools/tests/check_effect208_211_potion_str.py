"""Verify potion STR resources and the exact effect-prefix wave caller for 208/209/211."""
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
GENERATOR = (ROOT / "tools/generate_effect_dispatch.py").read_text(encoding="utf-8")
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
    208: ("파란포션.str", "byte_71AB2C", 0x71AB2C),
    209: ("초록포션.str", "byte_71AB1C", 0x71AB1C),
    211: ("fruit_.str", "aFruitStr_0", 0x71AB04),
}
symbols = SymbolTable(ExeImage())
for effect_id, (filename, symbol, va) in expected.items():
    for label, init in (("primary Ref", ref_init), ("canonical PE", pe_init)):
        if not re.search(rf"case\s+{effect_id}\s*:\s*sprintf\(StrName,\s*{symbol}\);\s*break;", init):
            raise SystemExit(f"{label} Init does not map effect{effect_id} to {symbol}")
    resolved = symbols.resolve_name(symbol)
    expected_raw = filename.encode("cp949") if effect_id != 211 else filename.encode("ascii")
    if resolved is None or resolved.va != va or resolved.raw != expected_raw:
        actual = None if resolved is None else (hex(resolved.va), resolved.raw.hex())
        raise SystemExit(f"{symbol} resolved as {actual!r}, expected VA 0x{va:X} bytes {expected_raw.hex()}")
    stem, extension = filename.rsplit(".", 1)
    local_name = ("".join(f"\\x{byte:02X}" for byte in stem.encode("cp949")) + "." + extension
                  if effect_id != 211 else filename)
    for table, label in ((CATALOG, "catalog"), (STR_TABLE, "STR table")):
        if f'{{ {effect_id}, "{local_name}"' not in table:
            raise SystemExit(f"local {label} does not map effect{effect_id} to the expected STR name")
    if f"case {effect_id}: return ProcessClipWaveStrEffect();" not in DISPATCH:
        raise SystemExit(f"local effect{effect_id} route is missing")
    if not re.search(rf"{effect_id}:\s*\"return ProcessClipWaveStrEffect\(\);\"", GENERATOR):
        raise SystemExit(f"effect generator does not preserve the effect{effect_id} route")

wave = symbols.resolve_name("aEffect_28")
if wave is None or (wave.va, wave.raw) != (0x6F1FF0, b"effect\\"):
    raise SystemExit(f"aEffect_28 resolved as {wave!r}, expected 0x6F1FF0 and the exact effect-prefix bytes")
main = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
start = main.index("          case 208:")
end = main.index("          case 212:", start)
caller = main[start:end]
for token in (
    "case 209:", "case 211:", "if ( !this->m_stateCnt )",
    "this->m_pos = (*p_m_master)->m_pos;",
    "PlayWave(aEffect_28, xdb, -80.0, zdb, 250, 40, 1.0);", "goto LABEL_424;",
):
    if token not in caller:
        raise SystemExit(f"canonical effect208/209/211 caller is missing {token}")

local = body(SOURCE, "bool CRagEffect::ProcessClipWaveStrEffect()")
local_branch = local[local.index("case 208: case 209: case 211:"):local.index("case 234:")]
if not all(token in local_branch for token in (r'wave = "effect\\";', "yOffset = -80.0f;", "copyMasterPosition = true;")):
    raise SystemExit("local shared wave helper does not preserve the effect-prefix cue and source offset")
print("effects208/209/211 parity: Ref/PE STR maps, raw resources, exact PE cue, routes and local helper passed")
