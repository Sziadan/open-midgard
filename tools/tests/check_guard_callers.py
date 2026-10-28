"""Compare Guard/Guard2 launchers and their callers against Ref and PE."""
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
main2 = (ROOT / "src/world/RagEffectOnProcess2Cases.generated.inc").read_text(encoding="utf-8")
main3 = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")

for sig in (
    "void __thiscall CRagEffect::Guard(CRagEffect *this, char *tName, char F1)",
    "void __thiscall CRagEffect::Guard2(CRagEffect *this, char *tName)",
):
    if compact(function(ref, sig)) != compact(function(pe, sig)):
        raise SystemExit(f"Ref/PE helper mismatch: {sig}")

for effect, caller, route in (
    (336, "CRagEffect::Guard(this, aEffectGuardkTg, 0);", "case 336: return ProcessGuardEffects();"),
    (496, "CRagEffect::Guard2(this, aEffectA01Bmp);", "case 496: return ProcessGuardEffects();"),
    (675, "CRagEffect::Guard(this, aEffectGuardkTg, 2);", "case 675: return ProcessGuardEffects();"),
):
    if caller not in pe or caller not in ref_callers:
        raise SystemExit(f"canonical/Ref caller missing for effect {effect}")
    dispatch = main2 if effect == 336 else main3
    if route not in dispatch: raise SystemExit(f"dispatcher route missing for {effect}")

if "ProcessGuardEffects" not in header or "SpawnGuard(int variant)" not in header:
    raise SystemExit("Guard local declaration missing")
spawn = function(local, "void CRagEffect::SpawnGuard(int variant)")
for token in (
    "PP_GUARD", '"effect\\\\kyrie_guard.wav"',
    '"effect\\\\black_maximize_power_sword_bic.wav"',
    '"effect\\\\guardK.tga"', '"effect\\\\guardK2.tga"',
    '"effect\\\\a01.bmp"', "prim->m_renderFlag = 5", "prim->m_duration = m_duration",
    "prim->m_size = 11.0f", "gi.maxHeight = variant == 1 ? 10.0f : 7.8000002f",
    "gi.flag[0] = static_cast<u8>(variant == 1 ? 1 : (variant == 2 ? 2 : 0))",
):
    if token not in spawn: raise SystemExit(f"Guard setup anchor missing: {token}")
if "UpdateReferenceGuard(this)" not in local or "RenderGuard(*this" not in local:
    raise SystemExit("existing PP_GUARD update/render path missing")
print("Guard/Guard2 parity: helpers, callers 336/496/675, assets, setup and routes passed")
