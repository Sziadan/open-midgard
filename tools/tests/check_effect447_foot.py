#!/usr/bin/env python3
"""Compare effect447's Foot(F1=1) caller and fixed primitive payload."""
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


require(re.search(r"case 447:\s*CRagEffect::Foot\(this, aEffectFootL2Tg, 1\);", PE) is not None,
        "PE effect447 caller mapping changed")
resolved = SymbolTable(ExeImage()).resolve_name("aEffectFootL2Tg")
require(resolved is not None and (resolved.va, resolved.raw) == (0x719ECC, b"effect\\foot_l2.tga"),
        "effect447 texture symbol bytes changed")
pe = body(PE, r"void __thiscall CRagEffect::Foot\([^)]*\)(?=\s*\{)")
ref = body(REF, r"void\s+CRagEffect\s*::\s*Foot\s*\([^)]*\)")
local = body(LOCAL, r"bool\s+CRagEffect::ProcessFoot447\s*\(\)")
require(pe is not None and ref is not None and local is not None, "PE, Ref, or local Foot helper missing")
for label, source in (("PE", pe), ("secondary Ref", ref)):
    for token in ("m_stateCnt", "PP_FOOT", "m_renderFlag", "m_duration", "m_size", "distance",
                  "RotStart", "alphaB", "height[0]"):
        require(token in source, f"{label} Foot helper missing {token}")
for token in ("if(F1==1)", "distance=6.0", "RotStart=(__int64)this->m_deltaPos.x",
              "alphaB=150", "height[0]=-0.2"):
    require(token in compact(pe), f"PE F1=1 profile changed: missing {token}")
for token in ("m_stateCnt==0", "GetTexture(tName)", "m_size=100", "distance=6.0f",
              "RotStart=m_deltaPos.x", "alphaB=150", "height[0]=-0.2f"):
    require(token in compact(ref), f"secondary Ref F1=1 profile changed: missing {token}")
ordered = ("m_effectPos = origin", "if (m_stateCnt != 0)", "LaunchEffectPrim(PP_FOOT",
           "prim->m_renderFlag = 1", "prim->m_duration = m_duration",
           'GetTexture("effect\\\\foot_l2.tga", false)', "prim->m_size = 100.0f",
           "prim->m_gi[0].life = 1", "prim->m_gi[1].life = 0", "prim->m_gi[2].life = 0",
           "prim->m_gi[3].life = 0", "prim->m_gi[0].process = 0", "prim->m_gi[0].distance = 6.0f",
           "prim->m_gi[0].rotStart", "prim->m_gi[0].alphaB = 150", "prim->m_gi[0].height[0] = -0.2f")
cursor = 0
for token in ordered:
    pos = local.find(token, cursor)
    require(pos >= 0, f"local Foot operation missing or reordered: {token}")
    cursor = pos + len(token)
require("case 447: return ProcessFoot447();" in DISPATCH
        and re.search(r'447:\s*"return ProcessFoot447\(\);"', GENERATOR),
        "effect447 generated route mismatch")
require(re.search(r"CRagEffect\s*::\s*Foot\s*\([^)]*\)\s*\{", REF1) is None,
        "primary Ref now has a Foot helper; re-audit provenance")
print("effect447 Foot(F1=1) source comparison passed; PE and secondary Ref agree")
