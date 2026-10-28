"""Compare GrandCross callers and launcher helpers with Ref and the canonical PE."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage  # noqa: E402


def require(ok: bool, message: str) -> None:
    if not ok: raise SystemExit(message)


def function(source: str, signature: str) -> str:
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{": depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0: return source[start:pos + 1]
    raise AssertionError(signature)


def compact(text: str) -> str:
    return re.sub(r"\s+", "", text)


ref = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
ref_calls = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
pe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
pe_onprocess = function(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
src = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
head = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8", errors="replace")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")

signatures = (
    "void __thiscall CRagEffect::GRANDCROSS(CRagEffect *this, __int16 time, char F1)",
    "void __thiscall CRagEffect::GRANDCROSS2(CRagEffect *this, char F1)",
)
for sig in signatures:
    require(compact(function(ref, sig)) == compact(function(pe, sig)), f"Ref/PE helper mismatch: {sig}")

for effect, variant in ((226, 0), (450, 1)):
    require(f"case {effect}: return ProcessGrandCrossEffects();" in dispatch, f"route {effect} missing")
    branch_start = pe_onprocess.index(f"case {effect}:")
    branch_end = pe_onprocess.index(f"case {effect + 1}:", branch_start)
    branch = pe_onprocess[branch_start:branch_end]
    call = f"CRagEffect::GRANDCROSS(this, 0, {variant});"
    followup = f"CRagEffect::GRANDCROSS2(this, {variant});"
    require(call in branch and followup in branch and branch.index(call) < branch.index(followup),
            f"canonical caller order/variant mismatch for {effect}")
require("GRANDCROSS(this" not in ref_calls, "Ref caller omission changed; review comparator evidence")

require("bool CRagEffect::ProcessGrandCrossEffects()" in src and "SpawnGrandCross(int variant)" in head,
        "local implementation/declaration missing")
launcher = function(src, "void CRagEffect::SpawnGrandCross(int variant)")
for token in (
    "PP_GI_3", "PP_GRANDCROSS2", "m_renderFlag |= 1u", "m_renderFlag |= 5u",
    '"effect\\\\ring_white.tga"', '"effect\\\\ring_yellow.tga"',
    '"effect\\\\ring_black.tga"', "grandCross->m_size = black ? 12.0f : 1.0f",
    "gi.flag[4] = static_cast<u8>(variant)", "gi.maxHeight = 19.9f", "gi.distance = 120.0f",
    "gi.flag[0] = static_cast<u8>(flagX[lane])", "gi.flag[1] = static_cast<u8>(flagY[lane])",
    "gi.distance = 60.0f", "gi.maxHeight = 24.0f", "gi.height[1] = 3.2f",
    "grandCross2->m_gi[2].life = 0", "grandCross2->m_gi[3].life = 0",
): require(token in launcher, f"local setup anchor missing: {token}")
require("rand()" not in launcher, "GrandCross local launcher unexpectedly consumes RNG")
proc = function(src, "bool CRagEffect::ProcessGrandCrossEffects()")
for token in (
    '"effect\\\\cru_grand cross.wav"', "m_stateCnt == 0", "SpawnGrandCross(m_type == 450 ? 1 : 0)",
): require(token in proc, f"caller behavior anchor missing: {token}")

image = ExeImage()
assets = {
    0x71C99C: b"effect\\cru_grand cross.wav",
    0x719480: b"effect\\ring_white.tga",
    0x7198D0: b"effect\\ring_black.tga",
    0x7193E8: b"effect\\ring_yellow.tga",
}
for address, expected in assets.items():
    value = image.read_cstring_at(address)
    require(value is not None and value.raw == expected, f"PE asset mismatch at {address:#x}")

print("GrandCross caller parity: Ref/PE helpers, 226/450 order, variants, fields, routes, assets and primitive support passed")
