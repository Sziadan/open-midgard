"""Compare effect26 IceArrow PE/Ref body, local route and lifecycle fixture."""
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


signature = "void __thiscall CRagEffect::IceArrow(CRagEffect *this)"
pe_handler = body(PE, signature, PE.index(signature + "; // idb") + 1)
ref_handler = body(REF, signature)
local_handler = body(SRC, "void CRagEffect::ApplyIceArrow()")
pe_process = body(
    PE,
    "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)",
    PE.index("unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this); // idb") + 1,
)
legacy_process = body(SRC, "bool CRagEffect::ProcessEffectFrameLegacy()")

normalize = lambda value: re.sub(r"\s+", " ", value).strip()
if normalize(pe_handler) != normalize(ref_handler):
    raise SystemExit("canonical PE and primary Ref IceArrow bodies differ")
if not re.search(r"case\s+26\s*:\s*CRagEffect::IceArrow\(this\)", pe_process):
    raise SystemExit("canonical PE effect26 route to IceArrow is missing")
if "case 26: return ProcessEffectFrameLegacy();" not in DISPATCH:
    raise SystemExit("local generated effect26 route is missing")
if not re.search(r"if\s*\(m_type\s*==\s*26\)\s*\{\s*ApplyIceArrow\(\);\s*return\s+true;\s*\}", legacy_process):
    raise SystemExit("local legacy route no longer dispatches effect26 to ApplyIceArrow")

ordered = (
    "m_effectPos = master->m_pos", "if (m_stateCnt == 12)",
    '"effect\\\\EF_IceArrow%d.wav"', "m_count *= 10", "const int flight = m_stateCnt - 12",
    "flight % 10 == 0 && flight < m_count", "LaunchEffectPrim(PP_3DCROSSTEXTURE",
    "const int spawnZ = rand() % 10", "const int spawnX = rand() % 10",
    "const int aimZ = rand() % 4", "const int aimX = rand() % 4 - 2",
    "prim->m_widthSize = 11.5f", "prim->m_heightSize = 3.8f",
    "prim->m_animSpeed = 4", '"effect\\\\icearrow.tga"', "if (m_flag != 0)",
    "m_flag = 0", "LaunchEffectPrim(PP_3DCIRCLE", '"effect\\\\ring_blue.tga"',
)
cursor = 0
for token in ordered:
    found = local_handler.find(token, cursor)
    if found < 0:
        raise SystemExit(f"local IceArrow branch or operation order missing: {token}")
    cursor = found + len(token)
if len(re.findall(r"\brand\s*\(", pe_handler)) != 5:
    raise SystemExit("IceArrow PE random-draw count changed")

for phrase in (
    "IceArrow copies actor before frame twelve without emission count scaling or audio",
    "IceArrow selects runtime wave and scales count on frame twelve before projectile gate",
    "IceArrow wave uses horizontal actor-listener delta before projectile RNG",
    "IceArrow draws Z spawn before X and consumes wave plus four projectile draws",
    "IceArrow preserves original cross-texture dimensions duration motion pattern and fade",
    "IceArrow contact clears only ground bit sets hit flag and replaces state with duration-minus-twenty-five before increment",
    "IceArrow consumes hit flag and emits reference circle on next effect call independent of flight gate",
    "IceArrow count bound excludes flight equal to scaled count",
):
    if phrase not in FIXTURE:
        raise SystemExit(f"IceArrow lifecycle fixture missing {phrase!r}")

print("effect26 IceArrow PE/Ref body, local route and lifecycle coverage passed")
