#!/usr/bin/env python3
"""Compare effect 430's HitLine5 setup and caller against the canonical sources."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage  # noqa: E402


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(message)


def function(source: str, signature: str) -> str:
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 0
    for cursor in range(opening, len(source)):
        if source[cursor] == "{":
            depth += 1
        elif source[cursor] == "}":
            depth -= 1
            if depth == 0:
                return source[start:cursor + 1]
    raise AssertionError(f"unterminated function {signature}")


def compact(source: str) -> str:
    return re.sub(r"\s+", "", source)


exe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
ref = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
local = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8", errors="replace")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")

signature = "void __thiscall CRagEffect::HitLine5(CRagEffect *this, char F1)"
exe_helper = function(exe, signature)
ref_helper = function(ref, signature)
require(compact(exe_helper) == compact(ref_helper), "Ref and canonical HitLine5 helper differ")
method = function(local, "bool CRagEffect::ProcessHitLine5Effect()")
require("ProcessHitLine5Effect" in header, "HitLine5 declaration missing")
require("case 430: return ProcessHitLine5Effect();" in dispatch, "effect 430 route missing")
texture = ExeImage().read_cstring_at(0x713CC8)
require(texture is not None and texture.text == "effect\\white02.bmp", "HitLine5 PE texture mismatch")

tokens = (
    "if (m_stateCnt <= 2)", "LaunchEffectPrim(PP_HITLINE3", "prim->m_renderFlag = 5",
    "prim->m_duration = m_duration", 'GetTexture("effect\\\\white02.bmp", false)',
    "prim->m_deltaPos = m_deltaPos", "prim->m_pos = origin", "gi.process = -25 - rand() % 21",
    "gi.maxHeight = 0.059999999f", "rand() % 121 + 125",
    "GetReferenceAngle(origin.x - m_deltaPos.x, origin.z - m_deltaPos.z)",
    "rand() % 121 - 60", "if (riseAngle >= 360)", "if (riseAngle < 0)",
    "rand() % 7", "gi.vecTNowX = static_cast<float>(static_cast<double>(rand() % 7) * 0.1 + 0.80000001)",
    "gi.sparkX = origin.x", "gi.sparkY = origin.y",
    "gi.sparkZ = origin.z", "gi.vecTNowY = 3.0f", "history <= 20", "gi.alphaT = 3",
)
cursor = 0
for token in tokens:
    pos = method.find(token, cursor)
    require(pos >= 0, f"local HitLine5 operation missing or reordered: {token}")
    cursor = pos + len(token)

main_onprocess = exe.index("unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)\n{")
case_start = exe.index("          case 430:", main_onprocess)
case_end = exe.index("          case 431:", case_start)
caller = exe[case_start:case_end]
require("CRagEffect::HitLine5(this, 3);" in caller, "canonical effect 430 call changed")
require("PlayWave" not in caller, "unexpected effect 430 audio")
print("HitLine5 helper Ref/PE comparison and effect 430 caller checks passed")
