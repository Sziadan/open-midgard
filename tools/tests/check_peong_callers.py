"""Compare the canonical Peong/PeongUp helpers, callers and emitted fields."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def function(source: str, signature: str, occurrence: int = 0) -> str:
    positions = [m.start() for m in re.finditer(re.escape(signature), source)]
    start = positions[occurrence] if occurrence else positions[-1]
    opening = source.index("{", start)
    depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{": depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0: return source[start:pos + 1]
    raise AssertionError(signature)


pe = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
ref = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="latin-1")
ref_dispatch = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
local = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")

up_sig = "void __thiscall CRagEffect::PeongUp(CRagEffect *this, char *tName, __int16 F1)"
pe_up = function(pe, up_sig)
ref_up = function(ref, up_sig)
if re.sub(r"\s+", "", pe_up) != re.sub(r"\s+", "", ref_up):
    raise SystemExit("Ref/PE PeongUp helper mismatch")

pe_peong = function(pe, "void __thiscall CRagEffect::Peong(CRagEffect *this, char *tName, int time)")
if "PeongMiddle(this" in ref_dispatch or "Peong(this" in ref_dispatch:
    raise SystemExit("Ref caller mapping changed; review PE-only effect411 branch")
if "void __thiscall CRagEffect::Peong(CRagEffect *this, char *tName, int time)" in ref:
    raise SystemExit("Ref Peong helper unexpectedly exists; review canonical mismatch")

main = function(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
branch = main[main.index("case 411:"):main.index("case 412:")]
ordered = [
    "v75 = 30", "PeongUp(this, aEffectWhitelig, 0)", "kk = 0; kk < 4; ++kk",
    "Peong(this, aEffectPeong1Tg, 90 * kk)", "Peong(this, aEffectPeong2Tg, 90 * kk)",
    "Peong(this, aEffectPeong3Tg, 90 * kk)", "Peong(this, aEffectPeong2Tg, 90 * kk)",
]
cursor = 0
for token in ordered:
    found = branch.find(token, cursor)
    if found < 0: raise SystemExit(f"canonical effect411 order missing: {token}")
    cursor = found + len(token)

if "case 411: return ProcessPeongEffects();" not in dispatch:
    raise SystemExit("effect411 route missing")
for signature in ("bool CRagEffect::ProcessPeongEffects()", "void CRagEffect::SpawnPeongUp(", "void CRagEffect::SpawnPeong("):
    declaration = signature.split("::", 1)[1].replace("(", "(") if "::" in signature else signature
    if signature not in local or declaration not in header:
        raise SystemExit(f"local Peong implementation/declaration missing: {signature}")
up_local = function(local, "void CRagEffect::SpawnPeongUp(")
for token in ("PP_PEONGUP", "m_duration", "variant == 0 ? 35", "variant == 1 ? 0 : variant",
              "rand() % 16", "rand() % 11", "rand() % 26",
              "rand() % 8", "rand() % 360", "gi.flag[0] = 1", "gi.flag[0] = 2"):
    if token not in up_local: raise SystemExit(f"PeongUp payload anchor missing: {token}")
if up_local.count("rand() %") != 18:
    raise SystemExit("PeongUp must preserve two branches of nine ordered RNG draws")

peong_local = function(local, "void CRagEffect::SpawnPeong(")
for token in ("PP_PEONG", "m_stateCnt != 35", "rand() % 11", "rand() % 90", "rand() % 11",
              "gi.riseAngle", "static_cast<float>(0.029999999)", "gi.sparkY = -4.0f", "gi.height[3]"):
    if token not in peong_local: raise SystemExit(f"Peong payload anchor missing: {token}")
if peong_local.count("rand() %") != 7:
    raise SystemExit("Peong loop must preserve eight RNG draws per GI")

caller = function(local, "bool CRagEffect::ProcessPeongEffects()")
for token in ('SpawnPeongUp("effect\\\\whitelight.tga", 0)',
              'SpawnPeong("effect\\\\peong1.tga", time)',
              'SpawnPeong("effect\\\\peong2.tga", time)',
              'SpawnPeong("effect\\\\peong3.tga", time)', "index < 30", "quadrant < 4", "90 * quadrant"):
    if token not in caller: raise SystemExit(f"effect411 caller anchor missing: {token}")
print("Peong/PeongUp parity: PE helper/caller mapping, route, trigger states, assets, fields and RNG anchors passed")
