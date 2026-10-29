"""Compare effect18 Steal PE/Ref body, local route and lifecycle fixture."""
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


signature = "void __thiscall CRagEffect::Steal(CRagEffect *this)"
pe_handler = body(PE, signature, PE.index(signature + "; // idb") + 1)
ref_handler = body(REF, signature)
local_handler = body(SRC, "void CRagEffect::SpawnSteal()")
pe_process = body(
    PE,
    "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)",
    PE.index("unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this); // idb") + 1,
)
legacy_process = body(SRC, "bool CRagEffect::ProcessEffectFrameLegacy()")

normalize = lambda value: re.sub(r"\s+", " ", value).strip()
if normalize(pe_handler) != normalize(ref_handler):
    raise SystemExit("canonical PE and primary Ref Steal bodies differ")
if not re.search(r"case\s+18\s*:\s*CRagEffect::Steal\(this\)", pe_process):
    raise SystemExit("canonical PE effect18 route to Steal is missing")
if "case 18: return ProcessEffectFrameLegacy();" not in DISPATCH:
    raise SystemExit("local generated effect18 route is missing")
if not re.search(r"if\s*\(m_type\s*==\s*18\)\s*\{\s*SpawnSteal\(\);\s*return\s+true;\s*\}", legacy_process):
    raise SystemExit("local legacy route no longer dispatches effect18 to SpawnSteal")

ordered = (
    "if (m_stateCnt != 0) return", 'PlayWave("effect\\\\EF_Steal.wav"',
    "for (int i = 0; i < 10; ++i)", "LaunchEffectPrim(PP_3DPARTICLEGRAVITY",
    "rand() % 360", "rand() % 100 - 50", "const double radius = rand() % 4 + 4",
    "prim->m_deltaPos2.y = static_cast<float>(prim->m_deltaPos3.y - 10.0)",
    "rand() % 50 + 50", "rand() % 90 + 60", "rand() % 100 + 100",
    '"\\xC0\\xCC\\xC6\\xD1\\xC6\\xAE\\\\particle7.spr"',
    '"\\xC0\\xCC\\xC6\\xD1\\xC6\\xAE\\\\particle7.act"', "ShiftCircleSegments(prim)",
)
cursor = 0
for token in ordered:
    found = local_handler.find(token, cursor)
    if found < 0:
        raise SystemExit(f"local Steal operation order missing: {token}")
    cursor = found + len(token)
if len(re.findall(r"\brand\s*\(", pe_handler)) != 6:
    raise SystemExit("Steal per-particle random-draw count changed")

for phrase in (
    "Steal emits ten gravity sprites with six RNG draws each",
    "Steal plays stored-origin horizontal wave before all emission draws",
    "Steal preserves parent origin and binds original gravity primitive to reference update/rendering",
    "Steal keeps tilted initial offset but replaces motion matrix with Y-only rotation",
    "Steal preserves source speed deceleration vertical gravity and acceleration arithmetic",
    "Steal loads exact particle-seven sprite/action in source order",
    "Steal emits only on parent frame zero",
    "Steal cleanup balances all ten sprite/action locks",
):
    if phrase not in FIXTURE:
        raise SystemExit(f"Steal lifecycle fixture missing {phrase!r}")

print("effect18 Steal PE/Ref body, local route, resources and lifecycle coverage passed")
