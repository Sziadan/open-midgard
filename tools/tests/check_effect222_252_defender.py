#!/usr/bin/env python3
"""Compare Defender/Reflect Shield variants and verify their shared origin refresh."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage, SymbolTable  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF2 = (ROOT / "Ref/dhxj/RagEffect2.cpp").read_text(encoding="latin-1")
REF_CALLS = (ROOT / "Ref/dhxj/RagEffect.cpp").read_text(encoding="latin-1")
REF_PRIMARY = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
TEST = (ROOT / "tools/tests/EffectLifecycleTests.cpp").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(message)


def function(source: str, signature: str) -> str:
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{": depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[start:pos + 1]
    raise AssertionError(signature)


pe = function(PE, "void __thiscall CRagEffect::DEFENDER(CRagEffect *this, char *tName)")
ref = function(REF2, "void\tCRagEffect :: DEFENDER(char *tName)")
for label, body in (("PE", pe), ("secondary Ref", ref)):
    require("m_stateCnt" in body and body.index("m_master->m_pos") < body.index("m_stateCnt"),
            f"{label} DEFENDER no longer refreshes master position before the state gate")
    require("PP_3DCASTING_2" in body and "m_duration" in body and "m_size" in body,
            f"{label} DEFENDER primitive header changed")
ref_compact = re.sub(r"\s+", "", ref)
for token in ("full_display_angle=360", "max_height=40.0", "RotStart=180",
              "alphaB=0", "distance=8.0", "rise_angle=90", "height[i]=0.0", "flag1[i]=0"):
    require(token in ref_compact, f"secondary Ref DEFENDER profile missing {token}")
require("case\tEF_DEFENDER:" in REF_CALLS and "DEFENDER(\"effect\\\\ring_black.tga\")" in REF_CALLS
        and "EF_REFLECTSHIELD" in REF_CALLS and "DEFENDER(\"effect\\\\ring_yellow.tga\")" in REF_CALLS,
        "secondary Ref Defender/Reflect Shield caller mapping changed")
require("case 222:" in PE and "CRagEffect::DEFENDER(this, aEffectRingBlac);" in PE
        and "case 252:" in PE and "CRagEffect::DEFENDER(this, aEffectRingYell);" in PE,
        "PE effect222/252 caller mapping changed")
require("CRagEffect::DEFENDER" not in REF_PRIMARY,
        "primary Ref unexpectedly contains DEFENDER caller/helper")
for effect, path in ((222, "effect\\\\ring_black.tga"), (252, "effect\\\\ring_yellow.tga")):
    require(f'case {effect}: ApplyDefender("{path}"); return true;' in DISPATCH,
            f"local effect{effect} route changed")

local = function(LOCAL, "void CRagEffect::ApplyDefender(const char* textureName)")
require(local.index("m_effectPos = origin") < local.index("m_stateCnt != 0"),
        "local Defender origin refresh must precede the state-zero gate")
for token in ("m_cachedPos = origin", "kPrimTypeCasting2", "prim->m_renderFlag = 5",
              "prim->m_duration = m_duration", "const short spins[4] = { 180, 270, 0, 90 }",
              "const float heights[4] = { 40.0f, 39.0f, 38.0f, 37.0f }",
              "const float distances[4] = { 8.0f, 7.9000001f, 7.8000002f, 7.6999998f }"):
    require(token in local, f"local Defender profile missing {token}")

symbols = SymbolTable(ExeImage())
for name, expected in (("aEffectRingBlac", b"effect\\ring_black.tga"),
                       ("aEffectRingYell", b"effect\\ring_yellow.tga")):
    resolved = symbols.resolve_name(name)
    require(resolved is not None and resolved.raw == expected, f"PE texture bytes changed: {name}")
for phrase in ("Defender variant emits source casting", "Defender retains all four source GI band",
               "Defender refreshes moved master after spawn gate"):
    require(phrase in TEST, f"Defender lifecycle coverage disappeared: {phrase}")
print("effects222/252 Defender parity passed: PE/secondary-Ref profile, variant routes/resources and lifecycle refresh")
