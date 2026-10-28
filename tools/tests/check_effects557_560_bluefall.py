#!/usr/bin/env python3
"""Compare BlueFall effects557-560 caller variants and waterfall payload."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage, SymbolTable  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/dhxj/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
REF1 = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
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


variants = ((557, 0, 0), (558, 1, 0), (559, 0, 1), (560, 1, 1))
for effect_id, axis, height in variants:
    require(re.search(rf"case 0x{effect_id:03X}:\s*CRagEffect::BlueFall\(this, {axis}, {height}, 0\);", PE)
            is not None, f"PE effect{effect_id} caller mapping changed")
    require(re.search(rf"case 0x{effect_id:03X}:\s*CRagEffect::BlueFall\(this, {axis}, {height}, 0\);", REF1)
            is not None, f"primary Ref effect{effect_id} caller mapping changed")

symbols = SymbolTable(ExeImage())
for name, va, raw in (("aEffectWaterfal", 0x71C2F4, b"effect\\waterfall11.tga"),
                      ("aEffectWaterfal_0", 0x71C2DC, b"effect\\waterfall12.tga"),
                      ("aEffectWaterfal_1", 0x71C2C4, b"effect\\waterfall13.tga")):
    symbol = symbols.resolve_name(name)
    require(symbol is not None and (symbol.va, symbol.raw) == (va, raw),
            f"{name} executable resource changed")

ref = body(REF, r"void\s+CRagEffect\s*::\s*BlueFall\s*\([^)]*\)")
spawn = body(LOCAL, r"void\s+CRagEffect::SpawnBlueFall\s*\([^)]*\)")
bands = body(LOCAL, r"void\s+ConfigureBlueFallBand\s*\([^)]*\)")
require(ref is not None and spawn is not None and bands is not None,
        "shared Ref or local waterfall helper missing")
for token in ("if(m_stateCnt==0)", "m_pos=m_master->m_pos", "LaunchEffectPrim(PP_WATERFALL)",
              "m_renderFlag=RF_EFFECT_OM", "m_totalTexture=3", "waterfall11.tga",
              "waterfall12.tga", "waterfall13.tga", "m_GI[ec].life=1", "m_GI[ec].process=0",
              "m_GI[ec].alphaB=120", "m_GI[ec].distance=36+(ec*1)",
              "m_GI[ec].flag1[1]=1"):
    require(token in compact(ref), f"secondary Ref BlueFall payload changed: missing {token}")
for token in ("m_effectPos = master->m_pos", "LaunchEffectPrim(PP_WATERFALL",
              "prim->m_renderFlag = 5u", "prim->m_duration = m_duration", "prim->m_totalTexture = 3",
              'GetTexture("effect\\\\waterfall11.tga", false)',
              'GetTexture("effect\\\\waterfall12.tga", false)',
              'GetTexture("effect\\\\waterfall13.tga", false)'):
    require(token in spawn, f"local BlueFall emitter missing: {token}")
require("prim->m_alpha" not in spawn and "prim->m_maxAlpha" not in spawn and "prim->m_tintColor" not in spawn,
        "BlueFall emitter should retain constructor alpha/tint defaults")
for token in ("band.active = true", "band.alpha = 120.0f", "band.distance = 36.0f +",
              "band.radius =", "band.maxHeight = heightMode == 1", "band.modes[0] =",
              "band.modes[1] = 1", "segmentIndex < 20", "kWaterfallSegmentHeight"):
    require(token in bands, f"local BlueFall band profile changed: missing {token}")
for effect_id, axis, height in variants:
    route = f"case {effect_id}: SpawnBlueFall({axis}, {height}); return true;"
    require(route in DISPATCH and re.search(rf'{effect_id}:\s*"SpawnBlueFall\({axis}, {height}\); return true;"', GENERATOR),
            f"effect{effect_id} generated route mismatch")
print("effects557-560 BlueFall source comparison passed; caller and waterfall profiles agree")
