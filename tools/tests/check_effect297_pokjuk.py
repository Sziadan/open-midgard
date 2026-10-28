#!/usr/bin/env python3
"""Compare effect297 PokJuk's player anchor, ordered assets, and RNG profile."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage, SymbolTable  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF2 = (ROOT / "Ref/dhxj/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
REF1 = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
GENERATOR = (ROOT / "tools/generate_effect_dispatch.py").read_text(encoding="utf-8")
symbols = SymbolTable(ExeImage())

def require(ok: bool, message: str) -> None:
    if not ok:
        raise SystemExit(message)

def function_body(source: str, signature: str) -> str | None:
    match = re.search(signature, source)
    if match is None:
        return None
    opening = source.find("{", match.end())
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:index]
    return None

require(re.search(r"case 297:\s*CRagEffect::PokJuk\(this\);", PE) is not None,
        "PE effect297 caller changed")
for symbol, va, raw in (
    ("aEffectPok1Tga", 0x71C244, b"effect\\pok1.tga"),
    ("aEffectPok3Tga", 0x71C234, b"effect\\pok3.tga"),
    ("aEffectPok2Tga", 0x71C224, b"effect\\pok2.tga"),
):
    resolved = symbols.resolve_name(symbol)
    require(resolved is not None and (resolved.va, resolved.raw) == (va, raw),
            f"{symbol} no longer resolves to the expected ordered texture")
require("case 297: return ProcessPokJuk297();" in DISPATCH
        and re.search(r"297:\s*\"return ProcessPokJuk297\(\);\"", GENERATOR),
        "effect297 local/generated route mismatch")

pe = function_body(PE, r"void __thiscall CRagEffect::PokJuk\([^)]*\)(?=\s*\{)")
ref = function_body(REF2, r"void\s+CRagEffect\s*::\s*PokJuk\s*\(\)")
require(pe is not None and ref is not None, "PE or secondary Ref PokJuk helper missing")
for label, body, tokens in (
    ("PE", pe, ("PP_POKJUK", "m_stateCnt", "m_player", "aEffectPok1Tga",
                "aEffectPok3Tga", "aEffectPok2Tga", "rand()", "max_height", "flag1[2]", "0.40000001")),
    ("secondary Ref", ref, ("PP_POKJUK", "m_stateCnt", "m_player", "pok1.tga", "pok3.tga",
                             "pok2.tga", "random(", "max_height", "flag1[2]", "0.004f")),
):
    for token in tokens:
        require(token in body, f"{label} PokJuk helper lost {token}")
require("m_pos.x = this->m_pos.x - 15.0" in pe
        and "m_pos.x-=15.0f" in ref,
        "PokJuk's 15-unit player-relative X adjustment changed")
for token in ("ProcessPokJuk297()", "PP_POKJUK", 'effect\\\\pok1.tga',
              'effect\\\\pok3.tga', 'effect\\\\pok2.tga',
              "m_effectPos.x -= 15.0f", "0.0040000002 + 0.40000001"):
    require(token in LOCAL, f"local PokJuk implementation missing {token}")
require(re.search(r"CRagEffect\s*::\s*PokJuk\s*\(\)", REF1) is None,
        "primary Ref now has PokJuk emitter; re-audit source provenance")
print("effect297 PokJuk source parity passed; secondary Ref helper corroborates PE")
