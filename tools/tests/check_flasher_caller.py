"""Compare Flasher helper and verify effect99 projection, payload, RNG and renderer routes."""
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
ref = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
local = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")

signature = "void __thiscall CRagEffect::Flasher(CRagEffect *this)"
if compact(function(pe, signature)) != compact(function(ref, signature)):
    raise SystemExit("Ref/PE Flasher helper mismatch")
main = function(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
case = main[main.index("case 99:"):main.index("case 100:")]
if "Flasher(this)" not in case: raise SystemExit("canonical effect99 caller changed")
if "Flasher(this" in ref: raise SystemExit("Ref caller omission changed; review PE-only caller evidence")
if "bool ProcessFlasher99();" not in header or "case 99: return ProcessFlasher99();" not in dispatch:
    raise SystemExit("effect99 route/declaration missing")

handler = function(local, "bool CRagEffect::ProcessFlasher99()")
for token in ("m_stateCnt == 0", '"effect\\\\hunter_flasher.wav"', "ProjectReferenceEffectVertex",
              "m_tlvertX =", "m_tlvertY =", "PP_3DTEXTURE", "m_pattern = 65", "m_duration = 70",
              "m_renderFlag |= 2u", "m_deltaPos2.y = -10.0f", "m_widthSize = 20.0f", "m_heightSize = 20.0f",
              "m_maxAlpha = 200.0f", "m_AlphaShow = true", "m_chPoint = {40.0f, 100.0f}",
              "m_chVal1 = {120.0f, 0.0f}", "m_chVal2 = {200.0f, 80.0f}", '"effect\\\\thunder_center.bmp"',
              "index < 20", "PP_2DFLASH", "m_deltaPos2.y = -40.0f", "rand() % 360", "rand() % 60 + 10",
              "rand() % 25 + 5", "rand() % 40 + 20", "rand() % 30 + 50", '"EFFECT\\\\alpha_center.tga"'):
    if token not in handler: raise SystemExit(f"Flasher payload missing {token}")
loop = handler[handler.index("for (int index = 0; index < 20"):]
if [loop.index(token) for token in ("rand() % 360", "rand() % 60 + 10", "rand() % 25 + 5", "rand() % 40 + 20", "rand() % 30 + 50")] != sorted(loop.index(token) for token in ("rand() % 360", "rand() % 60 + 10", "rand() % 25 + 5", "rand() % 40 + 20", "rand() % 30 + 50")):
    raise SystemExit("Flasher flash RNG order changed")
texture_family = function(local, "bool CEffectPrim::UsesReferenceTexture() const")
flash_family = function(local, "bool CEffectPrim::UsesReferenceFlash() const")
if "GetEffectType() == 99" not in texture_family or "GetEffectType() == 99" not in flash_family:
    raise SystemExit("effect99 reference texture/flash renderer family missing")
print("Flasher99 parity: Ref/PE helper, caller, route, projection, texture payload, 20x5 RNG and renderer families passed")
