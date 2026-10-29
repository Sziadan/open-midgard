"""Compare effect0 Hit1 PE/Ref body, local route and lifecycle evidence."""
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


signature = "void __thiscall CRagEffect::Hit1(CRagEffect *this)"
pe_handler = body(PE, signature, PE.rindex(signature))
ref_handler = body(REF, signature)
pe_process = body(
    PE,
    "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)",
    PE.rindex("unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)"),
)
spawn_hit = body(SRC, "void CRagEffect::SpawnHit(bool dark)")
hit1 = body(SRC, "void CRagEffect::SpawnHit1()")
particle = body(SRC, "void LaunchHitParticle(")
legacy = body(SRC, "bool CRagEffect::ProcessEffectFrameLegacy()")

normalize = lambda value: re.sub(r"\s+", " ", value).strip()
if normalize(pe_handler) != normalize(ref_handler):
    raise SystemExit("canonical PE and primary Ref Hit1 bodies differ")
if not re.search(r"case\s+0\s*:\s*CRagEffect::Hit1\(this\)", pe_process):
    raise SystemExit("canonical PE effect0 route to Hit1 is missing")
if "case 0: return ProcessEffectFrameLegacy();" not in DISPATCH:
    raise SystemExit("local generated effect0 route is missing")
if not re.search(r"if\s*\(m_type\s*==\s*0\)\s*\{\s*SpawnHit1\(\);\s*return\s+true;\s*\}", legacy):
    raise SystemExit("local legacy route no longer dispatches effect0 to SpawnHit1")
if "SpawnHit(false);" not in hit1:
    raise SystemExit("local Hit1 wrapper no longer selects the normal hit variant")

spawn_order = (
    "if (m_stateCnt != 0) return", "ResolveCurrentMaster()", "int facing = static_cast<int>(180.0 - master->m_roty)",
    "if (facing < 0) facing += 360", "LaunchEffectPrim(PP_3DCYLINDER", "prim->m_duration = 10",
    '"effect\\\\ring_blue.tga"', "if (dark || !g_session.m_isMinEffect)",
    "LaunchHitParticle(this, facing, false, dark, false)", "LaunchHitParticle(this, facing, true, dark, false)",
)
cursor = 0
for token in spawn_order:
    found = spawn_hit.find(token, cursor)
    if found < 0:
        raise SystemExit(f"local Hit1 operation order missing: {token}")
    cursor = found + len(token)
if spawn_hit.count("LaunchHitParticle(this, facing, false, dark, false)") != 1 or spawn_hit.count("LaunchHitParticle(this, facing, true, dark, false)") != 1:
    raise SystemExit("Hit1 must retain two linear and two gravity particle launches")
particle_order = (
    "facing + rand() % 80 + (gravity ? 140 : -40)", "rand() % 100 - 50", "const double radius = rand() % 4 + 4",
    "prim->m_deltaPos2.y = static_cast<float>(prim->m_deltaPos3.y - 10.0)",
    "if (gravity)", "rand() % 24 + 6", "rand() % speedRange + speedBase",
    "if (!gravity) prim->m_duration = rand() % 24 + 6", "if (gravity)", "rand() % 90 + 30",
    "rand() % 100 + 60", "prim->SetParticleResources(", "prim->m_numSegments = 3", "InitReferenceSegments(prim)",
)
cursor = 0
for token in particle_order:
    found = particle.find(token, cursor)
    if found < 0:
        raise SystemExit(f"local Hit1 particle operation order missing: {token}")
    cursor = found + len(token)

for phrase in (
    "Hit1 emits cylinder then two linear then two gravity sprites with ordered twenty-six draws",
    "Hit1 truncates negative source facing before adding three-sixty and initializes cylinder orientation",
    "Hit1 linear group preserves yaw pitch radius speed duration size draw order",
    "Hit1 gravity group preserves unwrapped longitude and duration-before-speed draw order",
    "Hit1 initializes all three trail slots at full current state",
    "Hit1 reduced-effects mode preserves cylinder and skips all particle draws/resources",
):
    if phrase not in FIXTURE:
        raise SystemExit(f"Hit1 lifecycle fixture missing {phrase!r}")

print("effect0 Hit1 PE/Ref body, local route, particle order and lifecycle coverage passed")
