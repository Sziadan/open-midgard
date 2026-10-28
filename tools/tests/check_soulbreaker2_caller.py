"""Check SoulBreaker2 caller/helper, field aliases and existing primitive paths."""
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


ref = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
ref_callers = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
pe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
local = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")

sig = "void __thiscall CRagEffect::SoulBreaker2(CRagEffect *this, __int16 time)"
if compact(function(ref, sig)) != compact(function(pe, sig)):
    raise SystemExit("Ref/PE SoulBreaker2 helper mismatch")
if "SoulBreaker2(this" in ref_callers:
    raise SystemExit("Ref caller omission changed; review canonical-only caller evidence")

pe_main = function(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
start = pe_main.index("case 409:")
end = pe_main.index("case 410:", start)
branch = pe_main[start:end]
for token in ("aEffect_106", "m_stateCnt <= 0xAu", "m_BodyLight & 1", "SetArgb(*p_m_master, -1, 255, 0, 255)",
              "jj = 0; jj < 360; jj += 45", "SoulBreaker2(this, jj)"):
    if token not in branch: raise SystemExit(f"canonical effect409 behavior missing: {token}")
if "return ProcessSoulBreaker2Effect();" not in dispatch:
    raise SystemExit("effect409 route missing")
if "SpawnSoulBreaker2(short time)" not in header:
    raise SystemExit("local declaration missing")
spawn = function(local, "void CRagEffect::SpawnSoulBreaker2(short time)")
for token in (
    "PP_SOULBREAKER2", '"effect\\\\purpleslash.tga"', "prim->m_renderFlag = 5",
    "prim->m_duration = m_duration", "rand() % 6", "gi.process = static_cast<int>(lane) - shoot - 28",
    "gi.distance = 20.0f", "gi.sparkX = m_effectPos.x", "gi.sparkZ = m_effectPos.z",
    "rand() % 5", "gi.rotStart = time", "gi.height[1] = 0.8f", "gi.flag[0] = 0",
):
    if token not in spawn: raise SystemExit(f"SoulBreaker2 emitter anchor missing: {token}")
caller = function(local, "bool CRagEffect::ProcessSoulBreaker2Effect()")
for token in ('"effect\\\\"', "m_stateCnt <= 10", "master->m_BodyLight & 1",
              "master->SetArgb(-1, 255, 0, 255)", "time < 360", "time + 45", "SpawnSoulBreaker2(time)"):
    if token not in caller: raise SystemExit(f"SoulBreaker2 caller anchor missing: {token}")
if "UpdateReferenceSoulBreaker2(this)" not in local or "PP_SOULBREAKER2) {\n        RenderReferenceSoulBreaker" not in local:
    raise SystemExit("existing SoulBreaker2 update/render support missing")
print("SoulBreaker2 parity: helper, effect409 caller, eight times, RNG aliases and primitive paths passed")
