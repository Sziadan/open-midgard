#!/usr/bin/env python3
"""Compare effect340 CALLZONE's frame-100 gate and emitter against PE/Ref."""
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


require(re.search(r"case 0x154:\s*CRagEffect::CALLZONE\(this, aEffectWhite02B\);", PE)
        is not None, "PE effect340 caller mapping changed")
ref_case = re.search(r"case 0x154:\s*CRagEffect::CALLZONE\(this, aEffectWhite02B\);", REF1)
require(ref_case is not None, "primary Ref effect340 caller changed")
symbol = SymbolTable(ExeImage()).resolve_name("aEffectWhite02B")
require(symbol is not None and (symbol.va, symbol.raw) == (0x713CC8, b"effect\\white02.bmp"),
        "effect340 texture symbol bytes changed")
ref = body(REF, r"void\s+CRagEffect\s*::\s*CALLZONE\s*\([^)]*\)")
local = body(LOCAL, r"bool\s+CRagEffect::ProcessCallZone340\s*\(\)")
require(ref is not None and local is not None, "shared Ref or local CALLZONE helper missing")
for token in ("m_stateCnt==100", "m_pos=m_master->m_pos", "LaunchEffectPrim(PP_CALLZONE)",
              "m_renderFlag=RF_EFFECT", "GetTexture(tName)", "m_pos.y-=10.0f",
              "m_GI[ec].life=0", "m_GI[ec].life=1", "distance=8.0f",
              "alphaB=0", "full_display_angle=360"):
    require(token in compact(ref), f"secondary Ref CALLZONE profile changed: missing {token}")
ordered = ("if (m_stateCnt != 100)", "ResolveCurrentMaster()", "m_effectPos = origin",
           "LaunchEffectPrim(PP_CALLZONE", "prim->m_renderFlag = 5",
           "prim->m_duration = m_duration", "prim->m_totalTexture = 1",
           'GetTexture("effect\\\\white02.bmp", false)', "m_effectPos.y -= 10.0f",
           "prim->m_gi[index].life = 0", "prim->m_gi[0].life = 1",
           "prim->m_gi[0].process = 0", "prim->m_gi[0].distance = 8.0f",
           "prim->m_gi[0].alphaB = 0", "prim->m_gi[0].fullDisplayAngle = 360")
cursor = 0
for token in ordered:
    pos = local.find(token, cursor)
    require(pos >= 0, f"local CALLZONE operation missing or reordered: {token}")
    cursor = pos + len(token)
require("case 340: return ProcessCallZone340();" in DISPATCH
        and re.search(r'340:\s*"return ProcessCallZone340\(\);"', GENERATOR),
        "effect340 generated route mismatch")
require(re.search(r"CRagEffect\s*::\s*CALLZONE\s*\([^)]*\)\s*\{", REF1) is None,
        "primary Ref now defines CALLZONE; re-audit provenance")
print("effect340 CALLZONE source comparison passed; frame gate and payload agree")
