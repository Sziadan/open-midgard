#!/usr/bin/env python3
"""Compare effect142 HeavensDrive PE/Ref helpers, route and resource bytes."""
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


signature = r"void __thiscall CRagEffect::HeavensDrive\(CRagEffect \*this\)"
pe_helper = function_body(PE, signature)
ref_helper = function_body(REF, signature)
if compact(pe_helper) != compact(ref_helper):
    raise AssertionError("canonical PE and primary Ref HeavensDrive helper bodies differ")

pe_onprocess = function_body(PE, r"unsigned __int8 __thiscall CRagEffect::OnProcess\(CRagEffect \*this\)")
if not re.search(r"case\s+142:\s*CRagEffect::HeavensDrive\(this\);\s*break;", pe_onprocess):
    raise AssertionError("canonical PE effect142 caller changed")
if "ApplyHeavensDrive" not in HEADER or "case 142: return ProcessEffectFrameLegacy();" not in DISPATCH:
    raise AssertionError("local effect142 route or declaration is missing")
local_frame = function_body(LOCAL, r"bool CRagEffect::ProcessEffectFrameLegacy\(\)")
if not re.search(r"if\s*\(m_type\s*==\s*142\)\s*\{\s*ApplyHeavensDrive\(\);\s*return true;\s*\}", local_frame):
    raise AssertionError("local legacy frame route no longer maps effect142 to ApplyHeavensDrive")

local_helper = function_body(LOCAL, r"void CRagEffect::ApplyHeavensDrive\(\)")
for token in ("if (m_stateCnt != 0) return;", '"effect\\\\wizard_earthspike.wav"',
              "mode->SendMsg(135,0,0,0)", "m_duration += 50;", "z = -10; z < 15; z += 5",
              "x = -10; x < 15; x += 5", "PP_3DQUADHORN", "prim->m_pattern = 3",
              '"effect\\\\stone.bmp"'):
    if token not in local_helper:
        raise AssertionError(f"local HeavensDrive behavior missing {token}")
quad_horn = function_body(LOCAL, r"bool CEffectPrim::UsesReferenceQuadHorn\(\) const")
if not re.search(r"m_master->GetEffectType\(\)\s*==\s*142", quad_horn):
    raise AssertionError("effect142 missing from the source-backed quad-horn family")

image = ExeImage(ROOT / "HighPriest.exe")
for va, expected in ((0x711E58, b"effect\\wizard_earthspike.wav"),
                     (0x71B858, b"effect\\stone.bmp")):
    actual = image.read_cstring_at(va)
    if actual is None or actual.raw != expected:
        raise AssertionError(f"canonical HeavensDrive resource bytes differ at VA 0x{va:X}")

print("effect142 HeavensDrive parity passed: PE/Ref helper, PE caller, local route/payload/family and exact wave/texture bytes")
