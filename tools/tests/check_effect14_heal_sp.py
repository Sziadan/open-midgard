"""Compare effect14 HealSP PE/Ref body, local route and lifecycle fixture."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
SRC = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
FIXTURE = (ROOT / "tools/tests/EffectLifecycleTests.cpp").read_text(encoding="utf-8")


def body(text: str, signature: str, start: int = 0) -> str:
    begin = text.index(signature, start)
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


signature = "void __thiscall CRagEffect::HealSP(CRagEffect *this)"
pe_handler = body(PE, signature, PE.index(signature + "; // idb") + 1)
ref_handler = body(REF, signature)
local_handler = body(SRC, "void CRagEffect::ApplyHealSP()")
pe_process = body(
    PE,
    "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)",
    PE.index("unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this); // idb") + 1,
)
legacy_process = body(SRC, "bool CRagEffect::ProcessEffectFrameLegacy()")

normalize = lambda value: re.sub(r"\s+", " ", value).strip()
if normalize(pe_handler) != normalize(ref_handler):
    raise SystemExit("canonical PE and primary Ref HealSP bodies differ")
if not re.search(r"case\s+14\s*:\s*CRagEffect::HealSP\(this\)", pe_process):
    raise SystemExit("canonical PE effect14 route to HealSP is missing")
if "case 14: return ProcessEffectFrameLegacy();" not in DISPATCH:
    raise SystemExit("local generated effect14 route is missing")
if not re.search(r"if\s*\(m_type\s*==\s*14\)\s*\{\s*ApplyHealSP\(\);\s*return\s+true;\s*\}", legacy_process):
    raise SystemExit("local legacy route no longer dispatches effect14 to ApplyHealSP")

ordered = (
    "m_effectPos = master->m_pos", "if (m_stateCnt == 0)", "static const float radii[]",
    "for (int i = 0; i < 3; ++i)", "LaunchEffectPrim(PP_3DCYLINDER",
    "if (i == 0) m_duration = 100", "prim->m_outerSize = radii[i]", "prim->m_innerSize = radii[i]",
    "prim->m_heightSize = heights[i]", "prim->m_maxAlpha = maxima[i]", '"effect\\\\alpha_down.tga"',
    'PlayWave("_heal_effect.wav"', "if (m_stateCnt % 2 != 0) return",
    "LaunchEffectPrim(PP_3DPARTICLEORBIT", "rand();", "rand() % 360", "m_gravSpeed = -1.2f",
    '"\\xC0\\xCC\\xC6\\xD1\\xC6\\xAE\\\\particle2.spr"',
    '"\\xC0\\xCC\\xC6\\xD1\\xC6\\xAE\\\\particle2.act"', "InitReferenceSegments(prim)",
)
cursor = 0
for token in ordered:
    found = local_handler.find(token, cursor)
    if found < 0:
        raise SystemExit(f"local HealSP operation order missing: {token}")
    cursor = found + len(token)
if len(re.findall(r"\brand\s*\(", pe_handler)) != 2:
    raise SystemExit("HealSP PE random-draw count changed")

for phrase in (
    "HealSP emits three cylinders then orbit sprite with discarded draw and duration one hundred despite reduced effects",
    "HealSP copies actor origin and plays exact root-level wave before orbit RNG",
    "HealSP cylinder radii heights distinct alpha caps source blue tint and rise fade profiles match reference",
    "HealSP orbit retains post-discard longitude gravity deceleration size duration and fade",
    "HealSP uses exact particle2 sprite then action and constructor animation profile",
    "HealSP odd frames refresh actor origin without repeated emission sound or RNG",
    "HealSP later even frame emits only a two-draw orbit sprite without cylinders or repeated audio",
    "HealSP cleanup balances each particle2 resource lock",
):
    if phrase not in FIXTURE:
        raise SystemExit(f"HealSP lifecycle fixture missing {phrase!r}")

print("effect14 HealSP PE/Ref body, local route, resources and lifecycle coverage passed")
