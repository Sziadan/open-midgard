#!/usr/bin/env python3
"""Compare effect 695's map fireworks route and payload with the PE/Ref."""
from __future__ import annotations

import re
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage  # noqa: E402

RAG = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
DISPATCH = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")

if "case 695: return ProcessEffect695MapFireworks();" not in DISPATCH:
    raise AssertionError("effect 695 is not routed to its dedicated map fireworks handler")
ref_case = re.search(r"case\s+0x2B7:(.*?)(?=case\s+0x2B8:)", REF, re.S)
if ref_case is None or not re.search(
    r"Map_MagicZone\(this,\s*aEffectGlow04Bm,\s*5\);.*?for\s*\(\s*m\s*=\s*2;\s*m\s*<\s*21;\s*m\s*\+=\s*2\s*\).*?Map_Particle\(this,\s*aEffectMpa3Tga,\s*m,\s*2\)",
    ref_case.group(1), re.S,
):
    raise AssertionError("Ref effect 695 no longer emits its glow04 zone followed by ten mpa3 particles")

image = ExeImage()
expected_resources = {
    0x719E24: b"effect\\glow04.bmp",
    0x719E14: b"effect\\mpa3.tga",
}
for va, expected in expected_resources.items():
    if image.read_cstring_at(va).raw != expected:
        raise AssertionError(f"effect 695 PE resource at {va:#x} changed")

start = RAG.find("bool CRagEffect::ProcessEffect695MapFireworks()")
end = RAG.find("// Ref/dhxj/RagEffect2.cpp:8393-8470", start)
if start < 0 or end < 0:
    raise AssertionError("missing effect 695 implementation")
helper = RAG[start:end]
for token in (
    "m_stateCnt != 0", "LaunchEffectPrim(PP_MAPMAGICZONE", "m_renderFlag = 5",
    "m_duration = m_duration", "m_size = 112.0f", "effect\\\\glow04.bmp",
    "life = 1", "distance = 30.0f", "alphaB = 250", "height[0] = -0.5f",
    "height[4] = 5.0f", "particleIndex <= 10", "LaunchEffectPrim(PP_MAPPARTICLE",
    "effect\\\\mpa3.tga", "maxHeight = 0.15000001f", "distance = 1.0f",
    "offsetY = -4.0f", "gi.flag[0] = 1", "gi.flag[1] = 2",
):
    if token not in helper:
        raise AssertionError(f"effect 695 payload is missing {token}")
if helper.count("rand() % 360") != 5:
    raise AssertionError("each map particle must consume eight RNG values in the source field order")
if not re.search(r"for\s*\(int index = 0; index < 4; \+\+index\).*?height\[4\].*?height\[5\].*?height\[6\].*?height\[7\]", helper, re.S):
    raise AssertionError("effect 695 RNG values are not assigned in the Ref order")

print("effect 695 source check passed: one PE-backed glow04 zone, ten ordered mpa3 particles, 320 frame-zero RNG draws")
