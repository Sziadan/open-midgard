"""Compare CartBoost helper and verify the canonical effect456 route/payload."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def function(source: str, signature: str) -> str:
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{": depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0: return source[start:pos + 1]
    raise AssertionError(signature)


def compact(text: str) -> str:
    return re.sub(r"\s+", "", text)


pe = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
ref2 = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="latin-1")
ref = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
local = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")

signature = "void __thiscall CRagEffect::CartBoost(CRagEffect *this)"
if compact(function(pe, signature)) != compact(function(ref2, signature)):
    raise SystemExit("Ref/PE CartBoost helper mismatch")
main = function(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
if "case 456:" not in main or "CartBoost(this)" not in main[main.index("case 456:"):main.index("case 457:")]:
    raise SystemExit("canonical effect456 caller changed")
if "case 456:" in ref:
    raise SystemExit("Ref caller omission changed; review PE-only caller evidence")
if "bool ProcessCartBoost456();" not in header or "case 456: return ProcessCartBoost456();" not in dispatch:
    raise SystemExit("effect456 route/declaration missing")

handler = function(local, "bool CRagEffect::ProcessCartBoost456()")
ordered = ["ProjectReferenceEffectVertex", "m_tlvertX =", "m_effectPos =", "PlayWave(",
           "m_stateCnt % 2", "LaunchEffectPrim(PP_3DCROSSTEXTURE", "rand() % 360",
           "rand() % 7", "MatrixAppendXRotation", "rand() % 50", "rand() % 100",
           "rand() % 4", '"effect\\\\ac_center2.tga"']
positions = [handler.index(token) for token in ordered]
if positions != sorted(positions):
    raise SystemExit("CartBoost projection, wave, even-state emitter, RNG order, or texture changed")
for token in ('"effect\\\\EF_IncAgility.wav"', "m_duration = 50", "m_maxAlpha = 60.0f",
              "m_fadeOutCnt = prim->m_duration - 20", "m_widthSize = static_cast<float>((rand() % 100 + 20) * 0.1)",
              "m_heightSize = static_cast<float>((rand() % 4) * 0.1 + 0.1)"):
    if token not in handler: raise SystemExit(f"CartBoost payload missing {token}")
print("CartBoost456 parity: Ref/PE helper, caller, route, projection, sound, RNG order and primitive payload passed")
