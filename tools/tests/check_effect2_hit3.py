"""Compare effect2 Hit3 PE/Ref body, local route and lifecycle evidence."""
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


signature = "void __thiscall CRagEffect::Hit3(CRagEffect *this)"
pe_handler = body(PE, signature)
ref_handler = body(REF, signature)
pe_process = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
legacy = body(SRC, "bool CRagEffect::ProcessEffectFrameLegacy()")
spawn = body(SRC, "void CRagEffect::SpawnHit3()")
particle = body(SRC, "void LaunchHitParticle(")

normalize = lambda value: re.sub(r"\s+", " ", value).strip()
if normalize(pe_handler) != normalize(ref_handler):
    raise SystemExit("canonical PE and primary Ref Hit3 bodies differ")
if not re.search(r"case\s+2\s*:\s*CRagEffect::Hit3\(this\)", pe_process):
    raise SystemExit("canonical PE effect2 route to Hit3 is missing")
if "case 2: return ProcessEffectFrameLegacy();" not in DISPATCH:
    raise SystemExit("local generated effect2 route is missing")
if not re.search(r"if\s*\(m_type\s*==\s*2\)\s*\{\s*SpawnHit3\(\);\s*return\s+true;\s*\}", legacy):
    raise SystemExit("local legacy effect2 route no longer dispatches to SpawnHit3")

ordered = (
    "if (m_stateCnt != 0) return", "ResolveCurrentMaster()", "GetCurrentGameMode()",
    'PlayWave("effect\\\\EF_hit3.wav"', "int facing = static_cast<int>(180.0 - master->m_roty)",
    "if (facing < 0) facing += 360", "for (int i = 0; i < 2; ++i)",
    "LaunchEffectPrim(PP_3DCYLINDER", "prim->m_duration = 15", "prim->m_outerSize = i == 0 ? 1.5f : 4.0f",
    '"effect\\\\lens2.tga"', "for (int i = 0; i < 8; ++i)",
    "LaunchHitParticle(this,facing,false,false,true,120,80)",
)
cursor = 0
for token in ordered:
    found = spawn.find(token, cursor)
    if found < 0:
        raise SystemExit(f"local Hit3 operation order missing: {token}")
    cursor = found + len(token)
for token in ("rand() % 80", "rand() % 100 - 50", "rand() % 4 + 4", "rand() % speedRange + speedBase", "rand() % 24 + 6", "rand() % 100 + 60"):
    if token not in particle:
        raise SystemExit(f"shared Hit3 particle profile is missing {token}")

for phrase in (
    "Hit3 emits two cylinders then eight linear sprites with forty-eight draws despite reduced effects",
    "Hit3 plays exact stored-origin horizontal wave before particle draws",
    "Hit3 binds original cylinder and linear reference paths",
    "Hit3 preserves shared source orientation offset and cylinder duration",
    "Hit3 preserves distinct outer radii and constructor-zero cylinder heights",
    "Hit3 cylinders preserve distinct rise speeds common acceleration and fade",
    "Hit3 uses source faster speed range and six-draw linear profile",
    "Hit3 initializes shrinking particles and all three full trail slots",
    "Hit3 preserves stored origin and eight exact resource-pair requests",
    "Hit3 does not repeat zero-frame burst or audio",
    "Hit3 cleanup releases all eight sprite/action pairs",
):
    if phrase not in FIXTURE:
        raise SystemExit(f"Hit3 lifecycle fixture missing {phrase!r}")

print("effect2 Hit3 PE/Ref body, PE/local route, operation order and lifecycle coverage passed")
