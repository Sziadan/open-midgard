#!/usr/bin/env python3
"""Compare effect 376's HitLine helper and caller to Ref and the executable."""
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
dispatch2 = (ROOT / "src/world/RagEffectOnProcess2Cases.generated.inc").read_text(encoding="utf-8")

exe_helper = function(exe, "void __thiscall CRagEffect::HitLine(CRagEffect *this, char F1)")
ref_helper = function(ref, "void __thiscall CRagEffect::HitLine(CRagEffect *this, char F1)")
require(compact(exe_helper) == compact(ref_helper), "Ref and canonical HitLine helper differ")
method = function(local, "bool CRagEffect::ProcessHitLineEffect(short f1, bool applyBodyLight)")
require("ProcessHitLineEffect(short f1 = 2, bool applyBodyLight = true)" in header, "HitLine helper declaration missing")
require("case 376: return ProcessHitLineEffect();" in dispatch, "effect 376 dispatcher route missing")
require("case 330: return ProcessHitLineEffect(0, false);" in dispatch2,
        "effect 330 must call HitLine with F1=0 and suppress effect376 caller tint")
resolved_texture = ExeImage().read_cstring_at(0x713CC8)
require(resolved_texture is not None and resolved_texture.text == "effect\\white02.bmp",
        "canonical HitLine texture at VA 0x713CC8 did not resolve")

ordered = (
    "if (m_stateCnt < 4)", "LaunchEffectPrim(PP_HITLINE", "prim->m_renderFlag = 0",
    "prim->m_renderFlag |= 1u", "prim->m_renderFlag |= 5u", "prim->m_duration = m_duration",
    'GetTexture("effect\\\\white02.bmp", false)', "for (EffectGi& gi : prim->m_gi)",
    "gi.process = -(rand() % 26)", "gi.maxHeight = 0.1f", "rand() % 51 + 125",
    "rand() % 360", "rand() % 8", "gi.sparkX = origin.x", "gi.sparkY = origin.y",
    "gi.sparkZ = origin.z", "gi.alphaT = f1",
    "applyBodyLight && m_stateCnt >= 10 && m_stateCnt <= 20", "master->m_BodyLight += 8",
    "master->SetArgb(-1, 255, 120, 50)",
)
cursor = 0
for token in ordered:
    pos = method.find(token, cursor)
    require(pos >= 0, f"local HitLine operation missing or reordered: {token}")
    cursor = pos + len(token)

case_start = exe.index("          case 376:", exe.index("CRagEffect::OnProcess3"))
case_end = exe.index("          case 377:", case_start)
caller = exe[case_start:case_end]
require("CRagEffect::HitLine(this, 2);" in caller, "canonical 376 HitLine(2) call missing")
require("v42 >= 10 && v42 <= 20" in caller, "canonical body-light frame window changed")
require("m_BodyLight & 8" in caller and "m_BodyLight += 8" in caller,
        "canonical body-light bit 8 behavior changed")
require("SetArgb(*p_m_master, -1, 255, 120, 50)" in caller,
        "canonical HitLine tint changed")
require("PlayWave" not in caller and "PlaySound" not in caller, "unexpected effect 376 audio")

onprocess2 = exe[exe.index("unsigned __int8 __thiscall CRagEffect::OnProcess2(CRagEffect *this)"):]
effect330_start = onprocess2.index("case 0x14A:")
effect330_end = onprocess2.index("case 0x14B:", effect330_start)
effect330 = onprocess2[effect330_start:effect330_end]
require("CRagEffect::HitLine(this, 0);" in effect330 and "return 1;" in effect330,
        "canonical effect330 OnProcess2 caller changed")
ref_onprocess = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
ref_start = ref_onprocess.index("case 0x14A:")
ref_end = ref_onprocess.index("case 0x14B:", ref_start)
require("CRagEffect::HitLine(this, 0);" in ref_onprocess[ref_start:ref_end],
        "primary Ref OnProcess2 effect330 caller changed")

print("HitLine helper Ref/PE identity and effect330/effect376 caller checks passed")
