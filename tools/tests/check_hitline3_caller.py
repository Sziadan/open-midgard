#!/usr/bin/env python3
"""Compare effect 431's HitLine3(F1=3) branch against canonical sources."""
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

signature = "void __thiscall CRagEffect::HitLine3(CRagEffect *this, int F1)"
exe_helper = function(exe, signature)
ref_helper = function(ref, signature)
require(compact(exe_helper) == compact(ref_helper), "Ref and canonical HitLine3 helper differ")
method = function(local, "bool CRagEffect::ProcessHitLine3Effect()")
require("ProcessHitLine3Effect" in header, "HitLine3 declaration missing")
require("case 431: return ProcessHitLine3Effect();" in dispatch, "effect 431 route missing")
texture = ExeImage().read_cstring_at(0x713CC8)
require(texture is not None and texture.text == "effect\\white02.bmp", "HitLine3 PE texture mismatch")

tokens = (
    "if (m_stateCnt >= 0 && m_stateCnt < 5)", "LaunchEffectPrim(PP_HITLINE3",
    "prim->m_renderFlag = 5", "prim->m_duration = m_duration",
    'GetTexture("effect\\\\white02.bmp", false)', "gi.process = -30 - rand() % 41",
    "gi.maxHeight = 0.07f", "rand() % 121 + 125", "rand() % 360", "rand() % 6",
    "gi.vecTNowY = 4.0f", "gi.height[0] = 0.0f",
    "gi.vecTNowX = static_cast<float>(static_cast<double>(rand() % 7) * 0.1 + 0.60000002)",
    "gi.sparkX = origin.x", "gi.sparkY = origin.y", "gi.sparkZ = origin.z",
    "history <= 20", "gi.alphaT = 3",
)
cursor = 0
for token in tokens:
    pos = method.find(token, cursor)
    require(pos >= 0, f"local HitLine3 operation missing or reordered: {token}")
    cursor = pos + len(token)

main_onprocess = exe.index("unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)\n{")
case_start = exe.index("          case 431:", main_onprocess)
case_end = exe.index("          case 432:", case_start)
caller = exe[case_start:case_end]
require("CRagEffect::HitLine3(this, 3);" in caller, "canonical effect 431 call changed")
require("PlayWave" not in caller, "unexpected effect 431 audio")
print("HitLine3 helper Ref/PE comparison and effect 431 caller checks passed")
