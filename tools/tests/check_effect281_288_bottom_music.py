#!/usr/bin/env python3
"""Compare Bottom_Music caller texture selectors for effects 281/284/288/370."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage, SymbolTable  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/dhxj/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
REF_PRIMARY = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
DISPATCH2 = (ROOT / "src/world/RagEffectOnProcess2Cases.generated.inc").read_text(encoding="utf-8")
DISPATCH3 = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")
GENERATOR = (ROOT / "tools/generate_effect_dispatch.py").read_text(encoding="utf-8")
symbols = SymbolTable(ExeImage())

def require(ok: bool, message: str) -> None:
    if not ok:
        raise SystemExit(message)

expected_callers = {
    281: ("&Buf", 2),
    284: ("&Buf", 5),
    288: ("&Buf", 8),
}
for effect_id, (texture_arg, variant) in expected_callers.items():
    require(re.search(rf"case {effect_id}:\s*CRagEffect::Bottom_Music\(this,\s*{re.escape(texture_arg)},\s*{variant}\);", PE),
            f"PE caller changed for effect {effect_id}")
    require(f"case {effect_id}: ApplyBottomMusic(nullptr, {variant}); return true;" in DISPATCH,
            f"local dispatch missing effect {effect_id}")
    require(re.search(rf"{effect_id}:\s*\"ApplyBottomMusic\(nullptr, {variant}\); return true;\"", GENERATOR),
            f"generator route missing effect {effect_id}")

for source_name, source in (("PE", PE), ("primary Ref", (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace"))):
    match = re.search(r"case 0x172:\s*CRagEffect::Bottom_Music\(this, aEffectCrossOld, 9\);\s*return 1;", source)
    require(match is not None, f"{source_name} effect370 Bottom_Music variant9 caller missing")

def function_body(source: str, signature: str) -> str:
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 0
    for position in range(opening, len(source)):
        if source[position] == "{":
            depth += 1
        elif source[position] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:position]
    raise AssertionError(f"unterminated function {signature}")

def effect674_caller(source: str) -> str:
    body = function_body(source, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
    start = body.index("case 0x2A2:")
    return body[start:body.index("case 0x2A3:", start)]

pe_674 = effect674_caller(PE)
ref_674 = effect674_caller(REF_PRIMARY)
require(re.sub(r"\s+", "", pe_674) == re.sub(r"\s+", "", ref_674),
        "PE and primary-Ref effect674 Bottom_Music callers differ")
require("Bottom_Music(this, aEffectCurseBmp, 6)" in pe_674,
        "effect674 Bottom_Music variant6 or curse resource changed")
require('case 674: ApplyBottomMusic("effect\\\\curse.bmp", 6); return true;' in DISPATCH3,
        "local effect674 OnProcess3 route or curse.bmp resource changed")
require(re.search(r"674:\s*'ApplyBottomMusic\(\"effect\\\\\\\\curse\.bmp\", 6\); return true;'", GENERATOR),
        "effect674 generator mapping changed")
curse = symbols.resolve_name("aEffectCurseBmp")
require(curse is not None and (curse.va, curse.raw) == (0x719C58, b"effect\\curse.bmp"),
        "effect674 exact curse.bmp executable bytes changed")
bottom_music = function_body(LOCAL, "void CRagEffect::ApplyBottomMusic(const char* textureName, int variant)")
require("variant == 6" in bottom_music and "gi.distance = 8.0f" in bottom_music
        and "variant == 5 || variant == 6" in bottom_music,
        "local Bottom_Music variant6 profile changed")
require('case 370: ApplyBottomMusic("effect\\\\cross_old.bmp", 9); return true;' in DISPATCH2,
        "local OnProcess2 effect370 route or texture changed")
require(symbols.resolve_name("aEffectCrossOld") is not None
        and (symbols.resolve_name("aEffectCrossOld").va, symbols.resolve_name("aEffectCrossOld").raw)
        == (0x719348, b"effect\\cross_old.bmp"),
        "effect370 cross_old.bmp executable resource mismatch")

resource_sets = {
    "aEffectMelodyAB": (0x71A4E8, b"effect\\melody_a.bmp"),
    "aEffectMelodyBB": (0x71A514, b"effect\\melody_b.bmp"),
    "aEffectSpell01B": (0x71C47C, b"effect\\spell_01.bmp"),
    "aEffectSpell02B": (0x71C468, b"effect\\spell_02.bmp"),
    "aEffectSpell03B": (0x71C454, b"effect\\spell_03.bmp"),
    "aEffectSpell04B": (0x71C440, b"effect\\spell_04.bmp"),
    "aEffectSpell05B": (0x71C42C, b"effect\\spell_05.bmp"),
    "aEffectSpell06B": (0x71C418, b"effect\\spell_06.bmp"),
    "aEffectSpell07B": (0x71C404, b"effect\\spell_07.bmp"),
    "aEffectSpell08B": (0x71C3F0, b"effect\\spell_08.bmp"),
}
for symbol, expected in resource_sets.items():
    actual = symbols.resolve_name(symbol)
    require(actual is not None and (actual.va, actual.raw) == expected,
            f"{symbol} executable resource mismatch")
prefix_symbols = [symbols.resolve_name(name) for name in ("aEffect_29", "aEffect_71", "aEffect_110")]
require(all(item is not None and (item.va, item.raw) == (0x6F1FF0, b"effect\\") for item in prefix_symbols),
        "PE variant5 symbols must remain the same exact effect\\ prefix")

pe_helper = re.search(r"void __thiscall CRagEffect::Bottom_Music\([^)]*\)\s*\{(.*?)\n\}", PE, re.S)
ref_helper = re.search(r"void CRagEffect\s*::\s*Bottom_Music\([^)]*\)\s*\{(.*?)\n\}", REF, re.S)
require(pe_helper is not None and ref_helper is not None, "PE/secondary Ref Bottom_Music helper missing")
for token in ("rand() & 0x80000001", "case 2:", "case 5:", "case 8:", "rand() % 8", "rand() % 3", "rand() % 360", "PP_BOTTOM"):
    require(token in pe_helper.group(1), f"canonical PE Bottom_Music lacks {token}")
for token in ("random(2)", "random(8)", "random(360)", "PP_BOTTOM"):
    require(token in ref_helper.group(1), f"secondary Ref Bottom_Music lacks {token}")
require('selectedTexture = chooseMelodyA ? "effect\\\\melody_a.bmp" : "effect\\\\melody_b.bmp"' in LOCAL
        and 'spell_08.bmp' in LOCAL and 'selectedTexture = "effect\\\\"' in LOCAL
        and "rand() % 3" in LOCAL and "rand() % 8" in LOCAL,
        "local variant texture selectors or unresolved prefix changed")
require('"effect\\\\redgemstone.bmp"' in ref_helper.group(1)
        and '"effect\\\\bluegemstone.bmp"' in ref_helper.group(1)
        and '"effect\\\\yellowgemstone.bmp"' in ref_helper.group(1),
        "recorded secondary Ref/PE variant5 resource discrepancy changed")
print("effects281/284/288/370/674 Bottom_Music source parity passed; effect370 and effect674 use exact texture bytes")
