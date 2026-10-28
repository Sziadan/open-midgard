#!/usr/bin/env python3
"""Compare effect448's right Foot(F1=1) profile to the shared PE/Ref helper."""
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


def compact(source: str) -> str:
    return re.sub(r"\s+", "", source)


require(re.search(r"case 448:\s*CRagEffect::Foot\(this, aEffectFootR2Tg, 1\);", PE) is not None,
        "PE effect448 caller mapping changed")
resolved = SymbolTable(ExeImage()).resolve_name("aEffectFootR2Tg")
require(resolved is not None and (resolved.va, resolved.raw) == (0x719EB8, b"effect\\foot_r2.tga"),
        "effect448 texture symbol bytes changed")
ref = body(REF, r"void\s+CRagEffect\s*::\s*Foot\s*\([^)]*\)")
local = body(LOCAL, r"bool\s+CRagEffect::ProcessFoot448\s*\(\)")
require(ref is not None and local is not None, "shared Ref or local Foot helper missing")
for token in ("m_stateCnt==0", "GetTexture(tName)", "m_size=100", "distance=6.0f",
              "RotStart=m_deltaPos.x", "alphaB=150", "height[0]=-0.2f"):
    require(token in compact(ref), f"shared secondary Ref F1=1 profile changed: missing {token}")
ordered = ("m_effectPos = origin", "if (m_stateCnt != 0)", "LaunchEffectPrim(PP_FOOT",
           "prim->m_renderFlag = 1", "prim->m_duration = m_duration",
           'GetTexture("effect\\\\foot_r2.tga", false)', "prim->m_size = 100.0f",
           "prim->m_gi[0].life = 1", "prim->m_gi[1].life = 0", "prim->m_gi[2].life = 0",
           "prim->m_gi[3].life = 0", "prim->m_gi[0].process = 0", "prim->m_gi[0].distance = 6.0f",
           "prim->m_gi[0].rotStart", "prim->m_gi[0].alphaB = 150", "prim->m_gi[0].height[0] = -0.2f")
cursor = 0
for token in ordered:
    pos = local.find(token, cursor)
    require(pos >= 0, f"local Foot448 operation missing or reordered: {token}")
    cursor = pos + len(token)
require("case 448: return ProcessFoot448();" in DISPATCH
        and re.search(r'448:\s*"return ProcessFoot448\(\);"', GENERATOR),
        "effect448 generated route mismatch")
require(re.search(r"CRagEffect\s*::\s*Foot\s*\([^)]*\)\s*\{", REF1) is None,
        "primary Ref now has a Foot helper; re-audit provenance")
print("effect448 Foot(F1=1) source comparison passed; shared PE/Ref profile agrees")
