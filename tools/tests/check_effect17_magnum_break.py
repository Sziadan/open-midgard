"""Compare effect17 MagnumBreak PE/Ref body, local route and lifecycle fixture."""
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


signature = "void __thiscall CRagEffect::MagnumBreak(CRagEffect *this)"
pe_handler = body(PE, signature, PE.index(signature + "; // idb") + 1)
ref_handler = body(REF, signature)
local_handler = body(SRC, "void CRagEffect::ApplyMagnumBreak()")
pe_process = body(
    PE,
    "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)",
    PE.index("unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this); // idb") + 1,
)
legacy_process = body(SRC, "bool CRagEffect::ProcessEffectFrameLegacy()")

normalize = lambda value: re.sub(r"\s+", " ", value).strip()
if normalize(pe_handler) != normalize(ref_handler):
    raise SystemExit("canonical PE and primary Ref MagnumBreak bodies differ")
if not re.search(r"case\s+17\s*:\s*CRagEffect::MagnumBreak\(this\)", pe_process):
    raise SystemExit("canonical PE effect17 route to MagnumBreak is missing")
if "case 17: return ProcessEffectFrameLegacy();" not in DISPATCH:
    raise SystemExit("local generated effect17 route is missing")
if not re.search(r"if\s*\(m_type\s*==\s*17\)\s*\{\s*ApplyMagnumBreak\(\);\s*return\s+true;\s*\}", legacy_process):
    raise SystemExit("local legacy route no longer dispatches effect17 to ApplyMagnumBreak")

ordered = (
    "if (m_stateCnt != 0) return", 'PlayWave("effect\\\\EF_MagnumBreak.wav"',
    "const auto circle = [&](int duration)", "LaunchEffectPrim(PP_3DCIRCLE", "prim->m_duration = duration",
    "prim->m_radiusSpeed = 1.75f", "prim->m_innerSize = 12.0f", "prim->m_fadeOutCnt = prim->m_duration-15",
    '"effect\\\\ring_yellow.tga"', "circle(m_duration)", "LaunchEffectPrim(PP_3DSPHERE",
    "prim->m_radiusSpeed = 1.15f", "m_longSpeed = 3.0f", '"effect\\\\\\xB4\\xEB\\xC6\\xF8\\xB9\\xDF.tga"',
    "circle(30)",
)
cursor = 0
for token in ordered:
    found = local_handler.find(token, cursor)
    if found < 0:
        raise SystemExit(f"local MagnumBreak operation order missing: {token}")
    cursor = found + len(token)
if re.search(r"\brand\s*\(", pe_handler):
    raise SystemExit("MagnumBreak source unexpectedly contains random draws")

for phrase in (
    "MagnumBreak emits ring sphere ring in original order without RNG despite reduced effects",
    "MagnumBreak plays exact stored-origin horizontal wave before emission",
    "MagnumBreak binds reference primitive paths and distinct parent-based versus fixed durations",
    "MagnumBreak loads exact recovered sphere texture between two yellow ring textures",
    "MagnumBreak nonzero calls preserve stored origin and do not repeat burst audio or resources",
):
    if phrase not in FIXTURE:
        raise SystemExit(f"MagnumBreak lifecycle fixture missing {phrase!r}")

print("effect17 MagnumBreak PE/Ref body, local route and lifecycle coverage passed")
