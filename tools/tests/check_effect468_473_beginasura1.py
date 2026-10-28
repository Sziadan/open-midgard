#!/usr/bin/env python3
"""Compare BeginAsura1 texture variants 468-473 to PE and Ref2."""
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
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
GENERATOR = (ROOT / "tools/generate_effect_dispatch.py").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(message)


def body(source: str, signature: str) -> str | None:
    match = re.search(signature, source)
    if match is None:
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


symbols = SymbolTable(ExeImage())
variants = (
    (468, "aEffectHanmoon2", 0x719C30, b"effect\\hanmoon2.tga"),
    (469, "aEffectHanmoon3", 0x71A6BC, b"effect\\hanmoon3.tga"),
    (470, "aEffectHanmoon4", 0x71A6A8, b"effect\\hanmoon4.tga"),
    (471, "aEffectHanmoon7", 0x71A694, b"effect\\hanmoon7.tga"),
    (472, "aEffectHanmoon5", 0x71A680, b"effect\\hanmoon5.tga"),
    (473, "aEffectHanmoon6", 0x71A66C, b"effect\\hanmoon6.tga"),
)
for effect_id, symbol, va, raw in variants:
    require(re.search(rf"case {effect_id}:\s*CRagEffect::BeginAsura1\(this, {symbol}\);", PE) is not None,
            f"PE effect{effect_id} caller mapping changed")
    resolved = symbols.resolve_name(symbol)
    require(resolved is not None and (resolved.va, resolved.raw) == (va, raw),
            f"effect{effect_id} texture bytes changed")
    route = f'case {effect_id}: return ProcessBeginAsura1Variant("{raw.decode().replace(chr(92), chr(92) * 2)}");'
    generated = re.search(rf"(?m)^\s*{effect_id}:\s*'([^']*)'", GENERATOR)
    expected_generator_value = f'return ProcessBeginAsura1Variant("{raw.decode().replace(chr(92), chr(92) * 4)}");'
    require(route in DISPATCH and generated is not None and generated.group(1) == expected_generator_value,
            f"effect{effect_id} local/generated route mismatch")

pe = body(PE, r"void __thiscall CRagEffect::BeginAsura1\([^)]*\)(?=\s*\{)")
ref = body(REF, r"void\s+CRagEffect\s*::\s*BeginAsura1\s*\([^)]*\)")
local = body(LOCAL, r"bool\s+CRagEffect::ProcessBeginAsura1Variant\s*\(const char\* textureName\)")
require(pe is not None and ref is not None and local is not None, "PE, Ref, or local shared emitter missing")
for token in ("m_stateCnt", "PP_ASURACASTING", "m_renderFlag", "m_duration", "m_totalTexture",
              "m_size", "m_GI[0].life", "m_GI[0].distance", "m_GI[0].flag1[4]", "height[0]"):
    require(token in pe, f"PE BeginAsura1 helper missing {token}")
for token in ("m_stateCnt==0", "PP_ASURACASTING", "m_size=0", "distance=18.0f", "flag1[4]=1"):
    require(re.sub(r"\s+", "", token) in re.sub(r"\s+", "", ref),
            f"secondary Ref shared profile missing {token}")
for token in ("if (m_stateCnt != 0)", "LaunchEffectPrim(PP_ASURACASTING", "prim->m_renderFlag = 5",
              "prim->m_duration = m_duration", "GetTexture(textureName, false)", "prim->m_size = 0.0f",
              "prim->m_gi[0].distance = 18.0f", "prim->m_gi[0].flag[4] = 1", "history <= 20",
              "prim->m_gi[lane].life = 0"):
    require(token in local, f"local shared BeginAsura1 profile missing {token}")
require(re.search(r"CRagEffect\s*::\s*BeginAsura1\s*\(", REF1) is None,
        "primary Ref now contains BeginAsura1; re-audit provenance")
print("effects468-473 BeginAsura1 source comparison passed; PE asset order and shared helper verified")
