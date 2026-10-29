"""Compare effect21 Detoxication PE/Ref body, routes, resources and fixture."""
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


signature = "void __thiscall CRagEffect::Detoxication(CRagEffect *this)"
pe_handler = body(PE, signature, PE.index(signature + "; // idb") + 1)
ref_handler = body(REF, signature)
local_handler = body(SRC, "bool CRagEffect::ProcessDetoxication21()")
pe_process = body(
    PE,
    "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)",
    PE.index("unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this); // idb") + 1,
)
process_frame = body(SRC, "bool CRagEffect::ProcessEffectFrame()")

normalize = lambda value: re.sub(r"\s+", " ", value).strip()
if normalize(pe_handler) != normalize(ref_handler):
    raise SystemExit("canonical PE and primary Ref Detoxication bodies differ")
if not re.search(r"case\s+21\s*:\s*CRagEffect::Detoxication\(this\)", pe_process):
    raise SystemExit("canonical PE effect21 route to Detoxication is missing")
if "case 21: return ProcessDetoxication21();" not in DISPATCH:
    raise SystemExit("local generated effect21 route is missing")
if '#include "RagEffectOnProcessCases.generated.inc"' not in process_frame:
    raise SystemExit("local effect dispatcher no longer includes generated case routes")

ordered = (
    "const int interval = g_session.m_isMinEffect ? 15 : 5",
    "if ((m_stateCnt % interval) != 0)", "if (m_stateCnt == 0",
    'PlayWave("effect\\\\EF_Detoxication.wav"', "LaunchEffectPrim(PP_3DPARTICLE",
    "prim->m_duration = 40", "prim->m_alpha = 0.0f", "const int angle = rand() % 360",
    "const double radial = rand() % 6 + 2", "prim->m_speed = static_cast<float>((rand() % 30 + 30)",
    "prim->m_size = static_cast<float>((rand() % 50 + 40)", "prim->m_rollSpeed = 3.0f",
    "prim->m_alphaSpeed = static_cast<float>(prim->m_maxAlpha * 0.1)",
    "prim->m_fadeOutCnt = prim->m_duration - prim->m_duration / 5",
    '"\\xC0\\xCC\\xC6\\xD1\\xC6\\xAE\\\\particle2.spr"',
    '"\\xC0\\xCC\\xC6\\xD1\\xC6\\xAE\\\\particle2.act"',
    "prim->m_numSegments = 1", "InitReferenceSegments(prim)",
)
cursor = 0
for token in ordered:
    found = local_handler.find(token, cursor)
    if found < 0:
        raise SystemExit(f"local Detoxication operation order missing: {token}")
    cursor = found + len(token)

for phrase in (
    "Detoxication state zero emits one reduced-effects particle with four ordered random draws",
    "Detoxication plays its canonical state-zero wave at effect-minus-listener X/Z",
    "Detoxication particle preserves source orbit offset, motion, fade, resources and reference family",
    "Detoxication reduced-effects cadence skips frame one without wave or random draws",
    "Detoxication reduced-effects cadence emits again at frame fifteen without repeating audio",
    "Detoxication normal-effects cadence emits at frame five",
):
    if phrase not in FIXTURE:
        raise SystemExit(f"Detoxication lifecycle fixture missing {phrase!r}")

print("effect21 Detoxication PE/Ref body, local route, resources and lifecycle coverage passed")
