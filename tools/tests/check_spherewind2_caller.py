"""Compare SphereWind2 helper/caller, renderer path and GI initialization."""
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

sig = "void __thiscall CRagEffect::SphereWind2(CRagEffect *this, char *tName, __int16 time, char F1)"
if compact(function(ref, sig)) != compact(function(pe, sig)):
    raise SystemExit("Ref/PE SphereWind2 helper mismatch")
if "SphereWind2(this" in ref_callers:
    raise SystemExit("Ref caller omission changed; inspect whether new caller evidence supersedes it")

pe_main = function(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
start = pe_main.index("case 408:")
end = pe_main.index("case 409:", start)
branch = pe_main[start:end]
if "ii = 0; ii < 45; ii += 9" not in branch or "SphereWind2(this, aEffectFireIvyB, ii, 0)" not in branch:
    raise SystemExit("canonical SphereWind2 call times/texture/variant changed")
if "PlayWave(aEffectEfBlessi" not in branch or branch.index("PlayWave(aEffectEfBlessi") > branch.index("SphereWind2(this"):
    raise SystemExit("canonical state-0 sound order changed")
if "return ProcessSphereWind2Effect();" not in dispatch:
    raise SystemExit("effect408 dispatch missing")
if "SpawnSphereWind2(short time)" not in header:
    raise SystemExit("local declaration missing")
spawn = function(local, "void CRagEffect::SpawnSphereWind2(short time)")
for token in (
    "PP_SPHEREWIND2", '"effect\\\\fire_ivy.bmp"', "prim->m_renderFlag = 5",
    "prim->m_duration = m_duration", "prim->m_size = 10.0f", "rand() % 20",
    "rand() % 11", "rand() % 360", "rand() % 91 + 180",
    "time + 45 * static_cast<int>(lane)", "gi.height[1] = static_cast<float>(rand() % 4 + 3)",
):
    if token not in spawn: raise SystemExit(f"SphereWind2 setup anchor missing: {token}")
caller = function(local, "bool CRagEffect::ProcessSphereWind2Effect()")
for token in ('"effect\\\\EF_Blessing.wav"', "time = 0; time < 45; time = static_cast<short>(time + 9)", "SpawnSphereWind2(time)"):
    if token not in caller: raise SystemExit(f"SphereWind2 caller anchor missing: {token}")
if "m_type == PP_SPHEREWIND2) {\n        RenderSphereWind" not in local:
    raise SystemExit("PP_SPHEREWIND2 renderer route missing")
if "UpdateReferenceSphereWind2(this)" not in local:
    raise SystemExit("existing PP_SPHEREWIND2 updater missing")
print("SphereWind2 parity: helper, effect408 caller, five times, RNG fields and renderer route passed")
