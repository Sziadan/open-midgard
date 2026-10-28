#!/usr/bin/env python3
"""Compare effect467 BeginAsura1 variant 0 to PE and secondary Ref."""
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


require(re.search(r"case 467:\s*CRagEffect::BeginAsura1\(this, aEffectHanmoon1\);", PE) is not None,
        "PE effect467 caller changed")
resolved = SymbolTable(ExeImage()).resolve_name("aEffectHanmoon1")
require(resolved is not None and (resolved.va, resolved.raw) == (0x719C44, b"effect\\hanmoon1.tga"),
        "effect467 texture bytes changed")
pe = body(PE, r"void __thiscall CRagEffect::BeginAsura1\([^)]*\)(?=\s*\{)")
ref = body(REF, r"void\s+CRagEffect\s*::\s*BeginAsura1\s*\([^)]*\)")
local = body(LOCAL, r"bool\s+CRagEffect::ProcessBeginAsura1_467\s*\(\)")
require(pe is not None and ref is not None and local is not None, "PE, Ref, or local BeginAsura1 missing")
for label, source in (("PE", pe), ("secondary Ref", ref)):
    for token in ("m_stateCnt", "PP_ASURACASTING", "m_renderFlag", "m_duration", "m_totalTexture",
                  "m_size", "life", "process", "distance", "alphaB", "height[0]", "flag1[0]", "flag1[4]"):
        require(token in source, f"{label} helper missing {token}")
for token in ("m_GI[0].distance = 18.0", "m_GI[0].flag1[4] = 1", "m_GI[0].height[0] = 0.0"):
    require(token in pe, f"PE BeginAsura1 payload changed: {token}")
for token in ("distance = 18.0f", "flag1[4]=1", "height[i]=0"):
    require(token in ref, f"secondary Ref BeginAsura1 payload changed: {token}")
ordered = ("m_effectPos = origin", "if (m_stateCnt != 0)", "LaunchEffectPrim(PP_ASURACASTING",
           "prim->m_renderFlag = 5", "prim->m_duration = m_duration", "prim->m_totalTexture = 1",
           'GetTexture("effect\\\\hanmoon1.tga", false)', "prim->m_size = 0.0f",
           "prim->m_gi[0].life = 1", "prim->m_gi[0].process = 0", "prim->m_gi[0].distance = 18.0f",
           "prim->m_gi[0].alphaB = 0", "prim->m_gi[0].height[0] = 0.0f",
           "prim->m_gi[0].flag[0] = 0", "prim->m_gi[0].flag[4] = 1", "history <= 20",
           "prim->m_gi[lane].life = 0")
cursor = 0
for token in ordered:
    pos = local.find(token, cursor)
    require(pos >= 0, f"local BeginAsura1 operation missing or reordered: {token}")
    cursor = pos + len(token)
require("case 467: return ProcessBeginAsura1_467();" in DISPATCH
        and re.search(r'467:\s*"return ProcessBeginAsura1_467\(\);"', GENERATOR),
        "effect467 generated route mismatch")
require(re.search(r"CRagEffect\s*::\s*BeginAsura1\s*\(", REF1) is None,
        "primary Ref now contains BeginAsura1; re-audit provenance")
print("effect467 BeginAsura1 source parity passed; secondary Ref corroborates PE")
