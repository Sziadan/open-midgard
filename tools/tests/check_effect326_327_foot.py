#!/usr/bin/env python3
"""Compare effects326/327 Foot(F1=0) against the PE and Ref implementation."""
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
DISPATCH = (ROOT / "src/world/RagEffectOnProcess2Cases.generated.inc").read_text(encoding="utf-8")
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


require(re.search(r"case 0x146:\s*CRagEffect::Foot\(this, aEffectFootLBTg, 0\);", PE)
        and re.search(r"case 0x147:\s*CRagEffect::Foot\(this, aEffectFootRBTg, 0\);", PE),
        "PE F1=0 caller mapping changed")
for name, va, raw in (("aEffectFootLBTg", 0x719428, b"effect\\foot_l_b.tga"),
                      ("aEffectFootRBTg", 0x719414, b"effect\\foot_r_b.tga")):
    symbol = SymbolTable(ExeImage()).resolve_name(name)
    require(symbol is not None and (symbol.va, symbol.raw) == (va, raw),
            f"{name} resource bytes changed")
require("case 0x146:\n      CRagEffect::Foot(this, aEffectFootLBTg, 0);" in REF1
        and "case 0x147:\n      CRagEffect::Foot(this, aEffectFootRBTg, 0);" in REF1,
        "primary Ref callers changed")
ref = body(REF, r"void\s+CRagEffect\s*::\s*Foot\s*\([^)]*\)")
local = body(LOCAL, r"bool\s+CRagEffect::ProcessFoot326_327\s*\([^)]*\)")
require(ref is not None and local is not None, "shared Ref or local Foot helper missing")
for token in ("m_stateCnt==0", "GetTexture(tName)", "m_size=0", "distance=5.0f",
              "RotStart=m_deltaPos.x", "alphaB=150", "height[0]=-0.2f"):
    require(token in compact(ref), f"shared Ref F1=0 profile changed: missing {token}")
ordered = ("m_effectPos = origin", "if (m_stateCnt != 0)", "LaunchEffectPrim(PP_FOOT",
           "prim->m_renderFlag = 1", "prim->m_duration = m_duration",
           "GetTexture(textureName, false)", "prim->m_size = 0.0f",
           "prim->m_gi[0].life = 1", "prim->m_gi[1].life = 0", "prim->m_gi[2].life = 0",
           "prim->m_gi[3].life = 0", "prim->m_gi[0].process = 0", "prim->m_gi[0].distance = 5.0f",
           "prim->m_gi[0].rotStart", "prim->m_gi[0].alphaB = 150",
           "prim->m_gi[0].height[0] = -0.2f")
cursor = 0
for token in ordered:
    pos = local.find(token, cursor)
    require(pos >= 0, f"local Foot326_327 operation missing or reordered: {token}")
    cursor = pos + len(token)
for effect_id, texture in ((326, "foot_l_b"), (327, "foot_r_b")):
    expected = f'case {effect_id}: return ProcessFoot326_327("effect\\\\{texture}.tga");'
    require(expected in DISPATCH and re.search(
        rf'{effect_id}:.*ProcessFoot326_327.*{texture}\.tga', GENERATOR),
            f"effect{effect_id} generated route mismatch")
require(re.search(r"CRagEffect\s*::\s*Foot\s*\([^)]*\)\s*\{", REF1) is None,
        "primary Ref now has a Foot helper; re-audit provenance")
print("effects326/327 Foot(F1=0) source comparison passed; shared PE/Ref profile agrees")
