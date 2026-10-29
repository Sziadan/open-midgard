#!/usr/bin/env python3
"""Compare effect128 OverThrust PE/Ref helpers, local route and resource bytes."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")


def function_body(source: str, signature: str) -> str:
    matches = list(re.finditer(signature, source))
    if not matches:
        raise AssertionError(f"missing {signature}")
    opening = source.find("{", matches[-1].end())
    depth = 0
    for offset in range(opening, len(source)):
        if source[offset] == "{":
            depth += 1
        elif source[offset] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:offset]
    raise AssertionError(f"unterminated {signature}")


def compact(text: str) -> str:
    return re.sub(r"\s+", "", text)


signature = r"void __thiscall CRagEffect::OverThrust\(CRagEffect \*this\)"
if compact(function_body(PE, signature)) != compact(function_body(REF, signature)):
    raise AssertionError("canonical PE and primary Ref OverThrust helper bodies differ")
pe_onprocess = function_body(PE, r"unsigned __int8 __thiscall CRagEffect::OnProcess\(CRagEffect \*this\)")
if not re.search(r"case\s+128:\s*CRagEffect::OverThrust\(this\);\s*break;", pe_onprocess):
    raise AssertionError("canonical PE effect128 caller changed")
if "void ApplyOverThrust();" not in HEADER or "case 128: return ProcessEffectFrameLegacy();" not in DISPATCH:
    raise AssertionError("local effect128 route or declaration is missing")
local_frame = function_body(LOCAL, r"bool CRagEffect::ProcessEffectFrameLegacy\(\)")
if not re.search(r"if\s*\(m_type\s*==\s*128\)\s*\{\s*ApplyOverThrust\(\);\s*return true;\s*\}", local_frame):
    raise AssertionError("local legacy frame route no longer maps effect128 to ApplyOverThrust")
local_helper = function_body(LOCAL, r"void CRagEffect::ApplyOverThrust\(\)")
for token in ('"effect\\\\black_overthrust.wav"', '"effect\\\\EF_StoneCurse.wav"',
              "m_stateCnt == 0", "ProjectReferenceEffectVertex", "m_tlvertX = projected.x",
              "m_tlvertY = projected.y", "m_duration = 9999", "PP_2DCIRCLE",
              "prim->m_duration = 40", "prim->m_deltaPos2.y = -20.0f", "prim->m_arcAngle = 10.0f",
              "prim->m_innerSize = 10.0f", "prim->m_radiusSpeed = 7.0f", "prim->m_fadeOutCnt = 0",
              '"effect\\\\alpha_center.tga"'):
    if token not in local_helper:
        raise AssertionError(f"local OverThrust behavior missing {token}")
image = ExeImage(ROOT / "HighPriest.exe")
for va, expected in ((0x71A598, b"effect\\black_overthrust.wav"),
                     (0x71BB9C, b"effect\\EF_StoneCurse.wav"),
                     (0x7192FC, b"effect\\alpha_center.tga")):
    actual = image.read_cstring_at(va)
    if actual is None or actual.raw != expected:
        raise AssertionError(f"canonical OverThrust resource bytes differ at VA 0x{va:X}")
print("effect128 OverThrust parity passed: PE/Ref helper, PE caller, local route/payload and three exact resource strings")
