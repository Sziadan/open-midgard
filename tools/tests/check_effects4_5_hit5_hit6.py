"""Compare effect4/5 Hit5/Hit6 PE/Ref helpers and shared local emitter."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
SRC = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
FIXTURE = (ROOT / "tools/tests/EffectLifecycleTests.cpp").read_text(encoding="utf-8")


def body(text: str, signature: str) -> str:
    begin = text.rindex(signature)
    opening = text.index("{", begin)
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[begin : pos + 1]
    raise AssertionError(f"unterminated body: {signature}")


normalize = lambda value: re.sub(r"\s+", " ", value).strip()
signature = "void __thiscall CRagEffect::Hit{}(CRagEffect *this)"
for effect_id, name in ((4, "Hit5"), (5, "Hit6")):
    canonical_signature = signature.format(effect_id + 1)
    pe_helper = body(PE, canonical_signature)
    ref_helper = body(REF, canonical_signature)
    if normalize(pe_helper) != normalize(ref_helper):
        raise SystemExit(f"canonical PE and primary Ref {name} bodies differ")
    process = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
    if not re.search(rf"case\s+{effect_id}\s*:\s*CRagEffect::{name}\(this\)", process):
        raise SystemExit(f"canonical PE effect{effect_id} route to {name} is missing")
    if f"case {effect_id}: return ProcessEffectFrameLegacy();" not in DISPATCH:
        raise SystemExit(f"local generated effect{effect_id} route is missing")
    local_wrapper = body(SRC, f"void CRagEffect::Spawn{name}()")
    if f"SpawnCrossLensHit({'true' if effect_id == 5 else 'false'});" not in local_wrapper:
        raise SystemExit(f"local {name} wrapper selects the wrong shared emitter variant")
    if f"m_type == {effect_id}" not in body(SRC, "bool CRagEffect::ProcessEffectFrameLegacy()"):
        raise SystemExit(f"local effect{effect_id} legacy route is missing")

shared = body(SRC, "void CRagEffect::SpawnCrossLensHit(bool compact)")
ordered = (
    "ResolveCurrentMaster()", "GetCurrentGameMode()", "ProjectReferenceEffectVertex", "if (m_stateCnt != 0) return",
    '"effect\\\\EF_hit6.wav" : "effect\\\\EF_hit5.wav"', "rand() % 360", "for (int i = 0; i < 2; ++i)",
    "LaunchEffectPrim(PP_2DTEXTURE", "prim->m_duration = 17", "angle + 90 * i", "rand() % 5 + (compact ? 10 : 20)",
    "rand() % (compact ? 5 : 10) + (compact ? 15 : 30)", "prim->m_heightSpeed = compact ? 1.7f : 2.5f",
    '"effect\\\\lens2.tga"',
)
cursor = 0
for token in ordered:
    found = shared.find(token, cursor)
    if found < 0:
        raise SystemExit(f"shared Hit5/Hit6 emitter order missing: {token}")
    cursor = found + len(token)

for phrase in (
    "Hit5/6 consume one shared angle draw then two size draws per streak",
    "Hit5/6 use reference screen-texture state and renderer",
    "Hit5/6 preserve unwrapped perpendicular angles and centered constructor pattern",
    "Hit5/6 preserve source lifetime anchor roll acceleration and fade",
    "Hit5/6 retain distinct source width and height RNG ranges",
    "Hit5/6 preserve distinct growth speeds and original alpha animation GI defaults",
    "Hit5/6 load exact lens-two texture for each streak",
    "Hit5/6 use distinct exact stored-origin waves before RNG",
    "Hit5/6 update actor projection while preserving stored origin and z",
    "Hit5/6 refresh projection on nonzero frames without repeated emission sound or draws",
):
    if phrase not in FIXTURE:
        raise SystemExit(f"Hit5/Hit6 lifecycle fixture missing {phrase!r}")

print("effects4/5 Hit5/Hit6 PE/Ref helpers, routes, shared emitter and lifecycle coverage passed")
