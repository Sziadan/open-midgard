#!/usr/bin/env python3
"""Compare effect434's HitLine3(F1=1) profile to PE and Ref2."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage  # noqa: E402

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


require(re.search(r"case 434:\s*CRagEffect::HitLine3\(this, 1\);", PE) is not None,
        "PE effect434 caller mapping changed")
texture = ExeImage().read_cstring_at(0x713CC8)
require(texture is not None and texture.text == "effect\\white02.bmp",
        "PE HitLine3 texture bytes changed")
pe = body(PE, r"void __thiscall CRagEffect::HitLine3\([^)]*\)(?=\s*\{)")
ref = body(REF, r"void\s+CRagEffect\s*::\s*HitLine3\([^)]*\)")
local = body(LOCAL, r"bool\s+CRagEffect::ProcessHitLine3Basic434\s*\(\)")
require(pe is not None and ref is not None and local is not None,
        "PE, secondary Ref, or local HitLine3 body missing")
for token in ("if ( (_BYTE)F1 == 1 )", "v4 = 2", "m_stateCnt >= v3", "m_stateCnt < v4",
              "m_renderFlag = 5", "aEffectWhite02B", "m_size = 0.0", "v7->m_GI[v6].life = 1",
              "g_prim->m_GI[v6].process", "m_GI[v6].max_height = 0.1", "m_GI[v6].alphaB",
              "m_GI[v6].rise_angle = rand() % 360", "m_GI[v6].distance = (double)(rand() % 5) + 5.0",
              "m_GI[v6].vecT_now.y = 3.0", "rand() % 7", "m_GI[v6].alphaT = (char)F1",
              "v22 = 10", "> 180.0"):
    require(token in pe, f"PE helper missing {token}")
for token in ("start=0", "uNum=2", "F1 == 1", "random(11)", "random(51)",
              "random(360)", "random(5)", "height[0]+=6", "height[i-1]"):
    require(token in ref, f"secondary Ref helper missing {token}")
ordered = ("gi.process = -(rand() % 11)", "gi.alphaB = static_cast<short>(rand() % 51 + 125)",
           "gi.riseAngle = static_cast<short>(rand() % 360)", "gi.distance = static_cast<float>(rand() % 5) + 5.0f",
           "gi.vecTNowX =", "gi.sparkX = origin.x", "history <= 20", "gi.alphaT = 1",
           "step < 10", "gi.height[0] += 6.0f", "gi.height[history] = gi.height[history - 1]")
cursor = 0
for token in ordered:
    pos = local.find(token, cursor)
    require(pos >= 0, f"local HitLine3 F1=1 operation missing or reordered: {token}")
    cursor = pos + len(token)
require("case 434: return ProcessHitLine3Basic434();" in DISPATCH
        and re.search(r'434:\s*"return ProcessHitLine3Basic434\(\);"', GENERATOR),
        "effect434 generated route mismatch")
require(re.search(r"CRagEffect\s*::\s*HitLine3\s*\(", REF1) is None,
        "primary Ref now contains HitLine3; re-audit source provenance")
print("effect434 HitLine3(F1=1) source comparison passed; secondary Ref corroborates PE")
