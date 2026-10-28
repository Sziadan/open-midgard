"""Compare Yufitel helpers and verify effect93/94/452 routing and texture sequences."""
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

for name, signature in (
    ("Yufitel", "void __thiscall CRagEffect::Yufitel(CRagEffect *this, char F1)"),
    ("YufitelHit", "void __thiscall CRagEffect::YufitelHit(CRagEffect *this, char F1)"),
):
    if compact(function(pe, signature)) != compact(function(ref, signature)):
        raise SystemExit(f"Ref/PE {name} helper mismatch")
pe_main = function(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
for effect, call in ((93, "Yufitel(this, 0)"), (94, "YufitelHit(this, 0)"), (452, "YufitelHit(this, 1)")):
    case = pe_main[pe_main.index(f"case {effect}:"):pe_main.index(f"case {effect + 1}:")]
    if call not in case: raise SystemExit(f"canonical effect{effect} caller changed")
if "Yufitel(this" in ref or "YufitelHit(this" in ref:
    raise SystemExit("Ref caller omission changed; review PE-only caller evidence")
for effect, route in ((93, "ProcessYufitel93()"), (94, "ProcessYufitelHit(0)"), (452, "ProcessYufitelHit(1)")):
    if f"case {effect}: return {route};" not in dispatch:
        raise SystemExit(f"effect{effect} route missing")
if "bool ProcessYufitel93();" not in header or "bool ProcessYufitelHit(int variant);" not in header:
    raise SystemExit("Yufitel handler declaration missing")

yufitel = function(local, "bool CRagEffect::ProcessYufitel93()")
for token in ('"effect\\\\hunter_shockwavetrap.wav"', "m_stateCnt != 0", "PP_3DTEXTURE",
              "m_pattern |= 0x440u", "m_renderFlag |= 2u", "m_widthSize = size", "std::atan2(dx, -dz)",
              "m_speed = static_cast<float>(-distance", "m_deltaPos2.y = -5.0f", "m_alpha = 170.0f",
              '"effect\\\\thunder_center.bmp"', "m_repeatAnim = true", "m_totalTexture = 6",
              '"effect\\\\thunder_ball_a.bmp"', '"effect\\\\thunder_ball_f.bmp"'):
    if token not in yufitel: raise SystemExit(f"Yufitel payload missing {token}")
if "rand()" in yufitel: raise SystemExit("Yufitel source helper has no RNG draws")

hit = function(local, "bool CRagEffect::ProcessYufitelHit(int variant)")
for token in ("m_effectPos = master->m_pos", "m_stateCnt % 20", "m_duration = 10", "m_pattern |= 0x40u",
              "m_renderFlag |= 0x202u", "m_changePoint = 10", "m_fadeOutCnt = 1",
              '"effect\\\\pokjuk_d.bmp"', '"effect\\\\thunder_pang.bmp"', "m_stateCnt == 10",
              "m_duration = 300", "m_repeatAnim = true", "m_totalTexture = 5",
              '"effect\\\\thunder_plazma_blast_a.bmp"', '"effect\\\\twirl_soft.bmp"',
              '"effect\\\\thunder_ball_f.bmp"'):
    if token not in hit: raise SystemExit(f"YufitelHit payload missing {token}")
if "rand()" in hit: raise SystemExit("YufitelHit source helper has no RNG draws")
family = function(local, "bool CEffectPrim::UsesReferenceTexture() const")
for effect in (93, 94, 452):
    if f"GetEffectType() == {effect}" not in family:
        raise SystemExit(f"effect{effect} reference texture renderer family route missing")
print("Yufitel93/94/452 parity: Ref/PE helpers, callers, routes, cadence, waves, texture sequences and renderer family passed")
