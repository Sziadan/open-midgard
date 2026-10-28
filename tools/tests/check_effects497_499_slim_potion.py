#!/usr/bin/env python3
"""Compare slim-potion OnProcess3 callers and paired primitives to PE/Ref."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage, SymbolTable  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF1 = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
REF2 = (ROOT / "Ref/dhxj/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
REF_READABLE = (ROOT / "Ref/dhxj/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
DISPATCH = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")
GENERATOR = (ROOT / "tools/generate_effect_dispatch.py").read_text(encoding="utf-8")


def require(ok: bool, message: str) -> None:
    if not ok:
        raise SystemExit(message)


def body(source: str, signature: str) -> str | None:
    match = re.search(signature, source)
    if not match:
        return None
    opening = source.find("{", match.end())
    depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:pos]
    return None


def compact(text: str) -> str:
    return re.sub(r"\s+", "", text)


variants = ((497, "byte_719740", 0x719740, 1, 1),
            (498, "byte_71970C", 0x71970C, 2, 2),
            (499, "byte_7196E8", 0x7196E8, 3, 0))
symbols = SymbolTable(ExeImage())
for effect_id, symbol_name, va, pressure_variant, shake_variant in variants:
    caller = (rf"case 0x{effect_id:03X}:\s*CRagEffect::PRESSURE\(this, &{symbol_name}, {pressure_variant}\);\s*"
              rf"CRagEffect::GroundShake\(this, aEffectBbbbBmp, -16, {shake_variant}\);")
    require(re.search(caller, PE) is not None, f"PE effect{effect_id} caller pair changed")
    require(re.search(caller, REF1) is not None, f"primary Ref effect{effect_id} caller pair changed")
    symbol = symbols.resolve_name(symbol_name)
    require(symbol is not None and symbol.va == va and symbol.raw.startswith(b"\xC0"),
            f"PE pressure texture pointer {symbol_name} changed")
pressure_raws = (bytes.fromhex("c0afc0fac0cec5cdc6e4c0ccbdba5cb7b9b5e5bdbdb8b2c6f7bcc72e626d70"),
                 bytes.fromhex("c0afc0fac0cec5cdc6e4c0ccbdba5cbfbbb7cebfecbdbdb8b2c6f7bcc72e626d70"),
                 bytes.fromhex("c0afc0fac0cec5cdc6e4c0ccbdba5cc8adc0ccc6aebdbdb8b2c6f7bcc72e626d70"))
for (_, name, _, _, _), raw in zip(variants, pressure_raws):
    symbol = symbols.resolve_name(name)
    require(symbol is not None and symbol.raw == raw, f"{name} CP949 resource bytes changed")
shake_symbol = symbols.resolve_name("aEffectBbbbBmp")
require(shake_symbol is not None and (shake_symbol.va, shake_symbol.raw)
        == (0x719730, b"effect\\bbbb.bmp"), "GroundShake bitmap bytes changed")

pressure = body(PE, r"void\s+__thiscall\s+CRagEffect::PRESSURE\([^)]*\)(?=\s*\{)")
shake = body(PE, r"void\s+__thiscall\s+CRagEffect::GroundShake\([^)]*\)(?=\s*\{)")
ref_pressure = body(REF2, r"void\s+CRagEffect\s*::\s*PRESSURE\s*\([^)]*\)")
ref_shake = body(REF2, r"void\s+CRagEffect\s*::\s*GroundShake\s*\([^)]*\)")
local = body(LOCAL, r"bool\s+CRagEffect::ProcessSlimPotionEffects\s*\(int variant\)")
require(all(x is not None for x in (pressure, shake, ref_pressure, ref_shake, local)),
        "PE/Ref/local pressure or GroundShake helper missing")
for token in ("if(!this->m_stateCnt)", "m_GI[0].distance=8.0", "m_GI[0].vecB_pre.y=-80.0",
              "m_GI[0].flag1[2]=9", "m_GI[0].flag1[4]=F1", "m_GI[3].life=1"):
    require(token in compact(pressure), f"PE PRESSURE profile changed: missing {token}")
for token in ("m_size=4.0", "m_GI[0].process=minus", "m_GI[0].distance=15.0",
              "m_GI[0].height[0]=-5.0"):
    require(token in compact(shake), f"PE GroundShake profile changed: missing {token}")
for token in ("if (m_stateCnt != 0)", "LaunchEffectPrim(PP_PRESSURE", "LaunchEffectPrim(PP_GROUNDSHAKE",
              "first.distance = 8.0f", "first.arcBaselineY = -80.0f", "first.flag[2] = 9",
              "first.flag[3] = static_cast<u8>(variant == 1 ? 1 : variant == 2 ? 3 : 4)",
              "first.flag[4] = static_cast<u8>(variant)", "rand() % 360", '"effect\\\\bbbb.bmp"',
              "shake->m_gi[0].process = -16", "shake->m_size = variant == 1 ? 101.0f"):
    require(token in local, f"local paired emitter differs from PE profile: missing {token}")
require("\"UserInterface\\\\red_slim_potion.bmp\"" in REF_READABLE
        and "\"UserInterface\\\\yellow_slim_potion.bmp\"" in REF_READABLE
        and "\"UserInterface\\\\White_slim_potion.bmp\"" in REF_READABLE,
        "readable Ref's localized pressure resource names changed")
for effect_id, pressure_variant in ((497, 1), (498, 2), (499, 3)):
    route = f"case {effect_id}: return ProcessSlimPotionEffects({pressure_variant});"
    require(route in DISPATCH and re.search(
        rf'{effect_id}:\s*"return ProcessSlimPotionEffects\({pressure_variant}\);"', GENERATOR),
        f"effect{effect_id} generated route mismatch")
require("PlayWave" not in local and "rand()" in local,
        "paired emitter should only consume pressure's angle draw and should not play audio")
print("effects497-499 slim-potion source comparison passed; PE bytes and paired profiles agree")
