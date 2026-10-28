"""Compare Endure helper and verify the canonical effect11 route and payload."""
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

signature = "void __thiscall CRagEffect::Endure(CRagEffect *this)"
if compact(function(pe, signature)) != compact(function(ref, signature)):
    raise SystemExit("Ref/PE Endure helper mismatch")
pe_main = function(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
case = pe_main[pe_main.index("case 11:"):pe_main.index("case 12:")]
if "Endure(this)" not in case:
    raise SystemExit("canonical effect11 caller changed")
if "Endure(this)" in ref:
    raise SystemExit("Ref caller omission changed; review PE-only caller evidence")
if "bool ProcessEndure11();" not in header or "case 11: return ProcessEndure11();" not in dispatch:
    raise SystemExit("effect11 route/declaration missing")

handler = function(local, "bool CRagEffect::ProcessEndure11()")
for token in ("ProjectReferenceEffectVertex", "m_tlvertX =", "m_tlvertY =", "m_stateCnt == 0",
              '"effect\\\\EF_Endure.wav"', "PP_2DTEXTURE", "m_pattern |= 2u", "m_deltaPos2.y = -20.0f",
              '"effect\\\\endure.tga"', "m_stateCnt < 50", "rand() % 360", "rand() % 100 + 60",
              "rand() % 20 + 30", "rand() % 40 + 100", '"effect\\\\alpha_down.tga"'):
    if token not in handler: raise SystemExit(f"Endure payload missing {token}")
if handler.count("rand()") != 4:
    raise SystemExit("Endure must preserve four ordered random draws on states below 50")
family = function(local, "bool CEffectPrim::UsesReferenceScreenTexture() const")
if "m_master->GetEffectType() == 11" not in family:
    raise SystemExit("effect11 reference screen-texture family route missing")
print("Endure11 parity: Ref/PE helper, caller, route, projection, two emitters, four-draw RNG, assets and screen-texture family passed")
