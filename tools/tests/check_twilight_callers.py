"""Compare Twilight emitters, caller counts, resources, RNG stores and routes."""
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
ref_callers = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
pe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
local = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
dispatch = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")

sig = "void __thiscall CRagEffect::Twilight(CRagEffect *this, char F1)"
require(compact(function(ref, sig)) == compact(function(pe, sig)), "Ref/PE Twilight helper mismatch")

for effect, variant in ((573, 0), (574, 1), (575, 2)):
    caller = f"case 0x{effect:X}:"
    pe_onprocess = function(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
    require(caller in pe_onprocess, f"PE case missing: {effect}")
    branch = pe_onprocess[pe_onprocess.index(caller):]
    require(f"CRagEffect::Twilight(this, {variant});" in branch[:branch.index("case ", 1)], f"PE variant mismatch {effect}")
    require("v40 = 80;" in branch[:branch.index("case ", 1)] or "v41 = 80;" in branch[:branch.index("case ", 1)] or "v42 = 80;" in branch[:branch.index("case ", 1)], f"PE caller count mismatch {effect}")
    require(f"CRagEffect::Twilight(this, {variant});" in ref_callers, f"Ref caller missing {effect}")
    require(f"case {effect}: return ProcessTwilightEffects();" in dispatch, f"route missing {effect}")

require("bool CRagEffect::ProcessTwilightEffects()" in local and "SpawnTwilight(int variant)" in header,
        "local Twilight declaration/implementation missing")
spawn = function(local, "void CRagEffect::SpawnTwilight(int variant)")
for token in (
    "PP_TWILIGHT", "m_effectPos = player->m_pos", "prim->m_renderFlag = 5",
    "prim->m_duration = m_duration", "rand() % 3", "rand() % 160 - 80", "rand() % 66",
    "rand() % 11", "gi.height[9] = static_cast<float>(rand() % 360)", "gi.flag[2] = 7",
    "UpdateReferenceTwilight(this)", "RenderReferenceCloud(*this, *viewMatrix)",
): require(token in (spawn if "UpdateReference" not in token and "RenderReference" not in token else local), f"Twilight anchor missing: {token}")

image = ExeImage()
expected = {
    0x71C200: bytes.fromhex("c0 af c0 fa c0 ce c5 cd c6 e4 c0 cc bd ba 5c 69 74 65 6d 5c c7 cf be e1 c7 e3 ba ea 2e 62 6d 70"),
    0x71C1D8: bytes.fromhex("c0 af c0 fa c0 ce c5 cd c6 e4 c0 cc bd ba 5c 69 74 65 6d 5c c8 ad c0 cc c6 ae bd bd b8 b2 c6 f7 bc c7 2e 62 6d 70"),
    0x71992C: bytes.fromhex("c0 af c0 fa c0 ce c5 cd c6 e4 c0 cc bd ba 5c 69 74 65 6d 5c c8 ad bf b0 ba b4 2e 62 6d 70"),
    0x71990C: bytes.fromhex("c0 af c0 fa c0 ce c5 cd c6 e4 c0 cc bd ba 5c 69 74 65 6d 5c bf b0 bb ea ba b4 2e 62 6d 70"),
    0x71C1B8: bytes.fromhex("c0 af c0 fa c0 ce c5 cd c6 e4 c0 cc bd ba 5c 69 74 65 6d 5c be cb c4 da bf c3 2e 62 6d 70"),
}
for address, raw in expected.items():
    value = image.read_cstring_at(address)
    require(value is not None and value.raw == raw, f"PE Twilight asset mismatch at {address:#x}")

require("height[9] = static_cast<float>(rand() % 360)" in spawn and "flag[2] = 7" in spawn,
        "post-increment GI aliases are not represented")
print("Twilight parity: helper, callers 573–575, five resources, GI setup and routes passed")
