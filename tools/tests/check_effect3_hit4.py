"""Compare effect3 Hit4 PE/Ref body, local route and lifecycle evidence."""
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


signature = "void __thiscall CRagEffect::Hit4(CRagEffect *this)"
pe_handler = body(PE, signature)
ref_handler = body(REF, signature)
pe_process = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
legacy = body(SRC, "bool CRagEffect::ProcessEffectFrameLegacy()")
spawn = body(SRC, "void CRagEffect::SpawnHit4()")
particle = body(SRC, "void LaunchHitParticle(")

normalize = lambda value: re.sub(r"\s+", " ", value).strip()
if normalize(pe_handler) != normalize(ref_handler):
    raise SystemExit("canonical PE and primary Ref Hit4 bodies differ")
if not re.search(r"case\s+3\s*:\s*CRagEffect::Hit4\(this\)", pe_process):
    raise SystemExit("canonical PE effect3 route to Hit4 is missing")
if "case 3: return ProcessEffectFrameLegacy();" not in DISPATCH:
    raise SystemExit("local generated effect3 route is missing")
if not re.search(r"if\s*\(m_type\s*==\s*3\)\s*\{\s*SpawnHit4\(\);\s*return\s+true;\s*\}", legacy):
    raise SystemExit("local legacy effect3 route no longer dispatches to SpawnHit4")

ordered = (
    "ResolveCurrentMaster()", "GetCurrentGameMode()", "ProjectReferenceEffectVertex", "if (m_stateCnt != 0) return",
    'PlayWave("effect\\\\EF_hit4.wav"', "int facing = static_cast<int>(180.0 - master->m_roty)",
    "if (facing < 0) facing += 360", "LaunchEffectPrim(PP_3DCYLINDER", "prim->m_duration = 15",
    "prim->m_outerSize = 4.0f; prim->m_innerSize = 0.5f", "prim->m_heightSpeed = 0.25f; prim->m_heightAccel = 0.15000001f",
    '"effect\\\\lens2.tga"', "for (int i = 0; i < 5; ++i)",
    "LaunchHitParticle(this,facing,false,false,true)",
)
cursor = 0
for token in ordered:
    found = spawn.find(token, cursor)
    if found < 0:
        raise SystemExit(f"local Hit4 operation order missing: {token}")
    cursor = found + len(token)
for token in ("rand() % 80", "rand() % 100 - 50", "rand() % 4 + 4", "rand() % speedRange + speedBase", "rand() % 24 + 6", "rand() % 100 + 60"):
    if token not in particle:
        raise SystemExit(f"shared Hit4 particle profile is missing {token}")

for phrase in (
    "Hit4 emits cylinder then five shrinking sprites with thirty draws despite reduced effects",
    "Hit4 projects actor x/y while preserving stored world origin and z",
    "Hit4 plays exact stored-origin horizontal wave before sprite draws",
    "Hit4 preserves truncated wrapped facing and reference cylinder/linear bindings",
    "Hit4 restores source cylinder radii initial height growth acceleration and fade",
    "Hit4 preserves source cylinder motion offset and exact lens texture",
    "Hit4 preserves shared six-draw source sprite profile",
    "Hit4 initializes all three trail slots after five exact resource-pair requests",
    "Hit4 refreshes actor projection before nonzero emission gate without repeat burst/audio",
    "Hit4 cleanup balances all sprite/action locks",
):
    if phrase not in FIXTURE:
        raise SystemExit(f"Hit4 lifecycle fixture missing {phrase!r}")

print("effect3 Hit4 PE/Ref body, PE/local route, operation order and lifecycle coverage passed")
