"""Compare effect27 FrostDiver PE/Ref body, local route and fixture."""
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


signature = "void __thiscall CRagEffect::FrostDiver(CRagEffect *this)"
pe_handler = body(PE, signature, PE.index(signature + "; // idb") + 1)
ref_handler = body(REF, signature)
local_handler = body(SRC, "void CRagEffect::ApplyFrostDiver()")
pe_process = body(
    PE,
    "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)",
    PE.index("unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this); // idb") + 1,
)
legacy_process = body(SRC, "bool CRagEffect::ProcessEffectFrameLegacy()")

normalize = lambda value: re.sub(r"\s+", " ", value).strip()
if normalize(pe_handler) != normalize(ref_handler):
    raise SystemExit("canonical PE and primary Ref FrostDiver bodies differ")
if not re.search(r"case\s+27\s*:\s*CRagEffect::FrostDiver\(this\)", pe_process):
    raise SystemExit("canonical PE effect27 route to FrostDiver is missing")
if "case 27: return ProcessEffectFrameLegacy();" not in DISPATCH:
    raise SystemExit("local generated effect27 route is missing")
if not re.search(r"if\s*\(m_type\s*==\s*27\s*\|\|\s*m_type\s*==\s*28\)\s*\{\s*if\s*\(m_type\s*==\s*27\)\s*ApplyFrostDiver\(\);\s*else\s+ApplyFrostDiver2\(\);\s*return\s+true;\s*\}", legacy_process):
    raise SystemExit("local legacy route no longer dispatches effect27 to ApplyFrostDiver")

ordered = (
    "if (g_session.m_isMinEffect) return", "if (m_stateCnt == 0)", "std::atan2(",
    "MatrixAppendYRotation(direction,GetRadian(static_cast<int>(-m_roty)))",
    "m_deltaPos3 = {m_deltaPos3.x-m_deltaPos2.x", "if (m_flag != 0) return",
    "if (std::sqrt(static_cast<double>(squared)) <= 2.5) m_flag = 1",
    "LaunchEffectPrim(PP_3DQUADHORN", "rand() % 360", "rand() % 10 + 5",
    "rand() % 40 + 60", "rand() % 30 + 150", "rand() % 360", "rand() % 30 + 75",
    "prim->m_rotMat = prim->m_matrix", '"effect\\\\ice.tga"',
    "m_deltaPos3 = {m_deltaPos3.x-m_deltaPos2.x",
)
cursor = 0
for token in ordered:
    found = local_handler.find(token, cursor)
    if found < 0:
        raise SystemExit(f"local FrostDiver operation order missing: {token}")
    cursor = found + len(token)
if len(re.findall(r"\brand\s*\(", pe_handler)) != 6:
    raise SystemExit("FrostDiver spike random-draw count changed")

for phrase in (
    "FrostDiver reduced effects suppresses all initialization movement RNG and emission",
    "FrostDiver initializes five-unit leading cursor then two-unit travel preserving stored origin",
    "FrostDiver retains six-draw size height longitude latitude profiles",
    "FrostDiver preserves original spike timing stop fade alpha and exact ice texture",
    "FrostDiver emits final arrival spike inside distance threshold and advances cursor once more",
    "FrostDiver arrival flag prevents further spikes and cursor movement",
    "FrostDiver arrival includes exact distance two-point-five and still emits final spike",
):
    if phrase not in FIXTURE:
        raise SystemExit(f"FrostDiver lifecycle fixture missing {phrase!r}")

print("effect27 FrostDiver PE/Ref body, local route and lifecycle coverage passed")
