"""Compare ParticleTime/ParticleSpread emitters and callers to Ref and PE."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
def require(ok: bool, message: str) -> None:
    if not ok: raise SystemExit(message)


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
dispatch = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")
pe_onprocess3 = function(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")

for sig in (
    "void __thiscall CRagEffect::ParticleTime(CRagEffect *this, char *tName, char F1)",
    "void __thiscall CRagEffect::ParticleSpread(CRagEffect *this, char *tName, char F1)",
):
    require(compact(function(ref, sig)) == compact(function(pe, sig)), f"Ref/PE helper mismatch: {sig}")

for effect, variant, helper in ((543, 1, "ParticleTime"), (546, 2, "ParticleTime"), (572, 3, "ParticleTime"), (518, 0, "ParticleSpread")):
    call = f"CRagEffect::{helper}(this, "
    require(call in ref_callers and call in pe, f"caller helper missing for {effect}")
    require(f"case {effect}: return ProcessParticle{'Time' if helper == 'ParticleTime' else 'Spread'}Effects();" in dispatch,
            f"route missing: {effect}")
for effect, case_id, target in ((518, "0x206", "ParticleSpread(this, aEffectWhitelig, 0)"),
                                (543, "0x21F", "ParticleTime(this, aEffectWhite02B, 1)"),
                                (546, "0x222", "ParticleTime(this, aEffectWhite02B, 2)"),
                                (572, "0x23C", "ParticleTime(this, aEffectWhite02B, 3)")):
    start = pe_onprocess3.index(f"case {case_id}:")
    next_case = pe_onprocess3.find("case ", start + 1)
    branch = pe_onprocess3[start:next_case if next_case >= 0 else len(pe_onprocess3)]
    require(f"CRagEffect::{target};" in branch, f"PE caller variant mismatch for {effect}")
    if effect == 518:
        require("v62 = 4;" in branch and "PlayWave(aEffectWizardFi" in branch
                and branch.index("PlayWave(aEffectWizardFi") < branch.index("CRagEffect::ParticleSpread"),
                "ParticleSpread repeat/sound order mismatch")
    if effect == 543:
        require("m_BodyLight += 0x4000" in branch and "m_BodyTime2 = -81 - v49" in branch,
                "ParticleTime effect543 actor flags/timing changed")
    if effect == 572:
        require("PlayWave(aEffectT_7" in branch and branch.index("PlayWave(aEffectT_7") < branch.index("CRagEffect::ParticleTime"),
                "ParticleTime effect572 sound order changed")
require("SpawnParticleTime(int variant)" in header and "SpawnParticleSpread()" in header,
        "local emitter declarations missing")

time = function(local, "void CRagEffect::SpawnParticleTime(int variant)")
for token in (
    "PP_PARTICLE_UP", '"effect\\\\pok1.tga"', '"effect\\\\pok3.tga"',
    "m_stateCnt % 4", "variant == 2 ? 40 : 0", "variant == 2 ? 120 : 80",
    "gi.height[7] = 0.0f", "gi.flag[1] = 1", "variant == 0 ? 11 : (variant == 3 ? 13 : 1)",
): require(token in time, f"ParticleTime setup anchor missing: {token}")
spread = function(local, "void CRagEffect::SpawnParticleSpread()")
for token in (
    "PP_3DAURA_3", '"effect\\\\whitelight.tga"', "rand() % 6", "rand() % 360",
    "rand() % 180", "gi.height[12] = 230.0f", "gi.height[13] = 230.0f",
    "gi.height[14] = 230.0f", "gi.height[15] = static_cast<float>(rand() % 3 + 1)",
    "gi.sparkX = 0.0f; gi.sparkY = 0.0f; gi.sparkZ = 0.0f",
): require(token in spread, f"ParticleSpread setup anchor missing: {token}")

time_caller = function(local, "bool CRagEffect::ProcessParticleTimeEffects()")
spread_caller = function(local, "bool CRagEffect::ProcessParticleSpreadEffects()")
for token in ('"effect\\\\T_"', '"effect\\\\wizard_fire_pillar_b.wav"',
              "master->m_BodyTime2 = -81 - m_stateCnt", "repeat < 4"):
    require(token in time_caller + spread_caller, f"caller behavior missing: {token}")

require("UpdateReferenceParticleSpread(this)" in local and "PP_PARTICLE_UP" in local,
        "existing particle update paths missing")
require('char aEffectT_7[] = "effect\\\\T_";' in pe,
        "effect572 wave prefix changed in canonical PE")
print("ParticleTime/ParticleSpread parity: helpers, callers 518/543/546/572, assets, RNG fields and routes passed")
