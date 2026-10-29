"""Compare effect1 Hit2 option0 PE/Ref body, route and lifecycle evidence."""
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


signature = "void __thiscall CRagEffect::Hit2(CRagEffect *this, int option)"
pe_handler = body(PE, signature)
ref_handler = body(REF, signature)
pe_process = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
legacy = body(SRC, "bool CRagEffect::ProcessEffectFrameLegacy()")
spawn = body(SRC, "void CRagEffect::SpawnHit2(int option)")

normalize = lambda value: re.sub(r"\s+", " ", value).strip()
if normalize(pe_handler) != normalize(ref_handler):
    raise SystemExit("canonical PE and primary Ref Hit2 bodies differ")

# Case1 jumps to the common label also reached by effect401, which invokes Hit2(0).
case1 = pe_process.index("case 1:")
case2 = pe_process.index("case 2:", case1)
case1_body = pe_process[case1:case2]
if not re.search(r"case\s+1\s*:\s*goto\s+\$L146731\s*;", case1_body):
    raise SystemExit("canonical PE effect1 route no longer jumps to the Hit2 label")
label = pe_process.index("$L146731:")
if "CRagEffect::Hit2(this, 0);" not in pe_process[label : label + 120]:
    raise SystemExit("canonical PE effect1 shared label no longer calls Hit2(option0)")
if "case 1: return ProcessEffectFrameLegacy();" not in DISPATCH:
    raise SystemExit("local generated effect1 route is missing")
if not re.search(r"if\s*\(m_type\s*==\s*1\)\s*\{\s*SpawnHit2\(0\);\s*return\s+true;\s*\}", legacy):
    raise SystemExit("local legacy effect1 route no longer calls SpawnHit2(0)")

ordered = (
    "ResolveCurrentMaster()", "ProjectReferenceEffectVertex", "if (m_stateCnt != 0) return",
    'PlayWave("effect\\\\EF_hit2.wav"', "for (int i = 0; i < 8; ++i)",
    "prim->m_duration = rand() % 20 + 10", "rand() % 30", "rand() % 45 + 5",
    "rand() % 15 + 5", "rand() % 20 + 20", "rand() % 5", "prim->m_deltaPos2.y = static_cast<float>(prim->m_deltaPos2.y - 20.0)",
    'i % 2 == 0 ? "effect\\\\lens1.tga" : "effect\\\\lens2.tga"',
)
cursor = 0
for token in ordered:
    found = spawn.find(token, cursor)
    if found < 0:
        raise SystemExit(f"local Hit2 option0 operation order missing: {token}")
    cursor = found + len(token)
if "option != 0" not in spawn:
    raise SystemExit("local Hit2 option selector missing")

for phrase in (
    "Hit2 emits eight screen streaks with six ordered RNG draws each",
    "Hit2 plays exact case-sensitive stored-origin wave before emission draws",
    "Hit2 projects actor x/y without replacing stored origin or z",
    "Hit2 preserves duration roll speed width height offset draw order and bottom pivot profile",
    "Hit2 restores original width shrink height acceleration alpha rise and constructor fade",
    "Hit2 alternates exact lens textures in original streak order",
    "Hit2 nonzero calls refresh projection without repeat emission/audio",
):
    if phrase not in FIXTURE:
        raise SystemExit(f"Hit2 lifecycle fixture missing {phrase!r}")

print("effect1 Hit2 PE/Ref body, PE/local route, operation order and lifecycle coverage passed")
