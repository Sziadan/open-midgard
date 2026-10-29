#!/usr/bin/env python3
"""Compare effect506 SoulParticle caller, helper, route, and asset to the PE/Ref."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage  # noqa: E402


def require(ok: bool, message: str) -> None:
    if not ok:
        raise SystemExit(message)


def function(source: str, signature: str) -> str:
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[start:pos + 1]
    raise AssertionError(signature)


def compact(source: str) -> str:
    return re.sub(r"\s+", "", source)


pe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
ref2 = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
ref = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
src = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
dispatch = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")

sig = "void __thiscall CRagEffect::SoulParticle(CRagEffect *this, char *tName)"
require(compact(function(pe, sig)) == compact(function(ref2, sig)), "PE and Ref SoulParticle helpers differ")
require("case 506: return ProcessSoulParticle506();" in dispatch, "effect506 route missing")
require('506: "return ProcessSoulParticle506();"' in (ROOT / "tools/generate_effect_dispatch.py").read_text(encoding="utf-8"),
        "effect506 generator route missing")

for source, label in ((pe, "PE"), (ref, "Ref")):
    start = source.index("case 0x1FA:")
    end = source.index("case 0x1FD:", start)
    caller = source[start:end]
    require("CRagEffect::SoulParticle(this, aEffectWhitelig);" in caller and "return 1;" in caller,
            f"{label} effect506 OnProcess3 caller changed")

handler = function(src, "bool CRagEffect::ProcessSoulParticle506()")
ordered = (
    "m_effectPos = master->m_pos", "if (m_stateCnt != 0)", "LaunchEffectPrim(PP_SOULPARTICLE",
    "prim->m_renderFlag = 5", "prim->m_duration = m_duration",
    'GetTexture("effect\\\\whitelight.tga", false)', "prim->m_size = 0.0f",
    "for (EffectGi& gi : prim->m_gi) gi.alphaT = 0", "gi.distance =",
    "gi.vecBPreX = m_deltaPos.x", "index < 4", "gi.height[12] = 0.0f",
    "gi.sparkX = m_deltaPos.x", "index = 4", "index < 8", "gi.height[13] = 0.0f",
    "gi.vecTPreX = m_deltaPos.x", "index = 8", "index < 11", "gi.height[11] =",
    "gi.height[14] = 0.0f", "gi.height[15] = 0.0f",
)
cursor = 0
for token in ordered:
    pos = handler.find(token, cursor)
    require(pos >= 0, f"effect506 local operation missing or reordered: {token}")
    cursor = pos + len(token)

asset = ExeImage().read_cstring_at(0x719780)
require(asset is not None and asset.text == "effect\\whitelight.tga", "PE texture bytes mismatch at 0x719780")
print("SoulParticle effect506 caller/helper, branch order, route, and executable texture bytes passed")
