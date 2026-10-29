"""Compare effect28 FrostDiver2 PE/Ref body, local route and fixture."""
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


signature = "void __thiscall CRagEffect::FrostDiver2(CRagEffect *this)"
pe_handler = body(PE, signature, PE.index(signature + "; // idb") + 1)
ref_handler = body(REF, signature)
local_handler = body(SRC, "void CRagEffect::ApplyFrostDiver2()")
pe_process = body(
    PE,
    "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)",
    PE.index("unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this); // idb") + 1,
)
legacy_process = body(SRC, "bool CRagEffect::ProcessEffectFrameLegacy()")

normalize = lambda value: re.sub(r"\s+", " ", value).strip()
if normalize(pe_handler) != normalize(ref_handler):
    raise SystemExit("canonical PE and primary Ref FrostDiver2 bodies differ")
if not re.search(r"case\s+28\s*:\s*CRagEffect::FrostDiver2\(this\)", pe_process):
    raise SystemExit("canonical PE effect28 route to FrostDiver2 is missing")
if "case 28: return ProcessEffectFrameLegacy();" not in DISPATCH:
    raise SystemExit("local generated effect28 route is missing")
if not re.search(r"if\s*\(m_type\s*==\s*27\s*\|\|\s*m_type\s*==\s*28\)\s*\{\s*if\s*\(m_type\s*==\s*27\)\s*ApplyFrostDiver\(\);\s*else\s+ApplyFrostDiver2\(\);\s*return\s+true;\s*\}", legacy_process):
    raise SystemExit("local legacy route no longer dispatches effect28 to ApplyFrostDiver2")

ordered = (
    "if (m_stateCnt != 0) return", "PlayWave(\"effect\\\\EF_FrostDiver2.wav\"",
    "for (int i = 0; i < 8; ++i)", "LaunchEffectPrim(PP_3DQUADHORN",
    "rand() % 360", "rand() % 4 + 1", "rand() % 250 + 100",
    "rand() % 100 + 200", "rand() % 360", "rand() % 20 + 80",
    "prim->m_rotMat = prim->m_matrix", "prim->m_alpha = 200.0f", '"effect\\\\ice.tga"',
)
cursor = 0
for token in ordered:
    found = local_handler.find(token, cursor)
    if found < 0:
        raise SystemExit(f"local FrostDiver2 operation order missing: {token}")
    cursor = found + len(token)
if len(re.findall(r"\brand\s*\(", pe_handler)) != 6:
    raise SystemExit("FrostDiver2 per-spike random-draw count changed")

for phrase in (
    "FrostDiver2 emits eight spikes with forty-eight draws despite reduced effects",
    "FrostDiver2 plays its distinct stored-origin horizontal wave before emission draws",
    "FrostDiver2 retains distinct radial size height and angle ranges without traveling cursor",
    "FrostDiver2 preserves alpha two-hundred and shared fixed spike motion profile",
    "FrostDiver2 requests exactly eight ice textures and preserves stored world origin",
    "FrostDiver2 does not repeat burst or audio after frame zero",
):
    if phrase not in FIXTURE:
        raise SystemExit(f"FrostDiver2 lifecycle fixture missing {phrase!r}")

print("effect28 FrostDiver2 PE/Ref body, local route and lifecycle coverage passed")
