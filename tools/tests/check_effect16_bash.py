"""Compare effect16 Bash PE/Ref body, local route and lifecycle coverage."""
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


signature = "void __thiscall CRagEffect::Bash(CRagEffect *this)"
pe_handler = body(PE, signature, PE.index(signature + "; // idb") + 1)
ref_handler = body(REF, signature)
local_handler = body(SRC, "void CRagEffect::ApplyBash()")
pe_process = body(
    PE,
    "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)",
    PE.index("unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this); // idb") + 1,
)
legacy_process = body(SRC, "bool CRagEffect::ProcessEffectFrameLegacy()")

normalize = lambda value: re.sub(r"\s+", " ", value).strip()
if normalize(pe_handler) != normalize(ref_handler):
    raise SystemExit("canonical PE and primary Ref Bash bodies differ")
if not re.search(r"case\s+16\s*:\s*CRagEffect::Bash\(this\)", pe_process):
    raise SystemExit("canonical PE effect16 route to Bash is missing")
if "case 16: return ProcessEffectFrameLegacy();" not in DISPATCH:
    raise SystemExit("local generated effect16 route is missing")
if not re.search(r"if\s*\(m_type\s*==\s*16\)\s*\{\s*ApplyBash\(\);\s*return\s+true;\s*\}", legacy_process):
    raise SystemExit("local legacy route no longer dispatches effect16 to ApplyBash")

ordered = (
    "if (g_session.m_isMinEffect) return", "ResolveCurrentMaster()", "GetCurrentGameMode()",
    "ProjectReferenceEffectVertex(master->m_pos", "m_tlvertX = projected.x", "if (m_stateCnt != 0) return",
    'PlayWave("effect\\\\EF_Bash.wav"', "LaunchEffectPrim(PP_2DCIRCLE", "prim->m_duration = 40",
    "prim->m_radius = 100.0f", '"effect\\\\alpha_down.tga"',
    "const int count = g_session.m_isMinEffect ? 10 : 20", "for (int i = 0; i < count; ++i)",
    "LaunchEffectPrim(PP_2DFLASH", "rand() % 360", "rand() % 60 + 10", "rand() % 25 + 5",
    "rand() % 40 + 20", "rand() % 30 + 20", '"EFFECT\\\\alpha_center.tga"',
)
cursor = 0
for token in ordered:
    found = local_handler.find(token, cursor)
    if found < 0:
        raise SystemExit(f"local Bash operation order missing: {token}")
    cursor = found + len(token)
if len(re.findall(r"\brand\s*\(", pe_handler)) != 5:
    raise SystemExit("Bash per-ray random-draw count changed")

for phrase in (
    "Bash reduced effects suppresses entire helper including audio projection RNG and emission",
    "Bash emits source disc then twenty five-draw reference rays in normal effects mode",
    "Bash projects actor x/y while retaining stored world origin z and parent lifetime",
    "Bash rays preserve ordered angle speed arc length growth draws and source alpha fade without change point",
    "Bash preserves disc and ray texture case source order without sprite resources",
):
    if phrase not in FIXTURE:
        raise SystemExit(f"Bash lifecycle fixture missing {phrase!r}")

print("effect16 Bash PE/Ref body, local route and lifecycle coverage passed")
