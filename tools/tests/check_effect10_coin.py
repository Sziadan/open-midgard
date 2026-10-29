"""Compare effect10 Coin PE/Ref body, local route and lifecycle fixture."""
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


signature = "bool __thiscall CRagEffect::Coin(CRagEffect *this)"
pe_handler = body(PE, signature, PE.rindex(signature))
ref_handler = body(REF, signature)
local_handler = body(SRC, "bool CRagEffect::ProcessCoin()")
pe_process = body(
    PE,
    "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)",
    PE.index("unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this); // idb") + 1,
)
effect_frame = body(SRC, "bool CRagEffect::ProcessEffectFrameLegacy()")

normalize = lambda value: re.sub(r"\s+", " ", value).strip()
if normalize(pe_handler) != normalize(ref_handler):
    raise SystemExit("canonical PE and primary Ref Coin bodies differ")
if not re.search(r"case\s+10\s*:\s*result\s*=\s*CRagEffect::Coin\(this\)", pe_process):
    raise SystemExit("canonical PE effect10 route to Coin is missing")
if "case 10: return ProcessEffectFrameLegacy();" not in DISPATCH:
    raise SystemExit("local generated effect10 route is missing")
if "if (m_type == 10) return ProcessCoin();" not in effect_frame:
    raise SystemExit("local dispatcher no longer routes effect10 to ProcessCoin")

ordered = (
    "if (!g_session.m_isMinEffect && m_stateCnt == 20)", "for (int i = 0; i < 5; ++i)",
    "LaunchEffectPrim(PP_3DPARTICLEGRAVITY", "prim->m_duration = rand() % 30 + 40",
    "prim->m_longitude = static_cast<float>(30 * (rand() % 12))", "const double radius = rand() % 2 + 2",
    "prim->m_speed = static_cast<float>(static_cast<double>(rand() % 50 + 50)",
    "prim->m_gravSpeed = static_cast<float>(static_cast<double>(rand() % 100 + 50)",
    "prim->m_playWaveType = 1", "prim->SetParticleResources(", "prim->m_numSegments = 2",
    "InitReferenceSegments(prim)", "ProcessEZ2STR(0.0f,-80.0f,0.0f)", "return m_stateCnt < 100",
)
cursor = 0
for token in ordered:
    found = local_handler.find(token, cursor)
    if found < 0:
        raise SystemExit(f"local Coin operation order missing: {token}")
    cursor = found + len(token)
if len(re.findall(r"\brand\s*\(", pe_handler)) != 5:
    raise SystemExit("Coin per-particle random-draw count changed")

for phrase in (
    "Coin remains active before emission despite missing STR",
    "Coin emits five gravity particles with five ordered draws each",
    "Coin preserves random duration thirty-degree heading steps and radius offset",
    "Coin initializes both segments to full current state before any trail attenuation",
    "Coin actual particle audio flag triggers original bounce wave and one variant draw",
    "Coin burst is restricted to frame twenty",
    "Coin terminates exactly at one hundred independently of STR state",
    "Coin still processes original STR offset when reduced effects suppress its particle burst",
):
    if phrase not in FIXTURE:
        raise SystemExit(f"Coin lifecycle fixture missing {phrase!r}")

print("effect10 Coin PE/Ref body, local route and lifecycle coverage passed")
