"""Compare ShieldBoomerang launchers and routed effect ids with the canonical sources."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage  # noqa: E402


def require(ok: bool, message: str) -> None:
    if not ok:
        raise SystemExit(message)


def function(source: str, signature: str) -> str:
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[start:pos + 1]
    raise AssertionError(signature)


def compact(source: str) -> str:
    return re.sub(r"\s+", "", source)


ref = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
pe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
src = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8", errors="replace")
dispatcher = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")
main_dispatcher = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")

helper_sigs = (
    "void __thiscall CRagEffect::ShieldBoomerang(CRagEffect *this, char *tName)",
    "void __thiscall CRagEffect::ShieldBoomerang3(CRagEffect *this, char *tName)",
)
for sig in helper_sigs:
    body = function(ref, sig)
    require(compact(function(pe, sig)) == compact(body), f"canonical PE helper differs: {sig}")

require("bool CRagEffect::ProcessShieldBoomerangEffects()" in src, "handler implementation missing")
require("ProcessShieldBoomerangEffects" in header, "handler declaration missing")
for case, table in ((249, main_dispatcher), (494, dispatcher), (520, dispatcher)):
    require(f"case {case}: return ProcessShieldBoomerangEffects();" in table,
            f"effect {case} dispatcher route missing")

# Branch order, cadence, call counts and primitive initialization are source anchors.
handler = function(src, "bool CRagEffect::ProcessShieldBoomerangEffects()")
for token in (
    "m_effectPos = master->m_pos", "m_stateCnt == 0 || m_stateCnt == 3 || m_stateCnt == 6 || m_stateCnt == 9 || m_stateCnt == 12",
    "m_stateCnt == 0", "m_type == 494 ? \"effect\\\\\" : \"effect\\\\shield_boomerang.bmp\"",
    "PP_SHIELDBOOMERANG3 : PP_SHIELDBOOMERANG", "rand() % 360", "gi.height[1] / gi.maxHeight",
    "for (size_t index = 1; index < prim->m_gi.size(); ++index)",
):
    require(token in handler, f"handler behavior anchor missing: {token}")

image = ExeImage()
resources = {
    0x71C33C: b"effect\\cru_shield boomerang.wav",
    0x719648: b"effect\\shield_boomerang.bmp",
}
for address, expected in resources.items():
    value = image.read_cstring_at(address)
    require(value is not None and value.raw == expected, f"PE asset mismatch at {address:#x}")

for label, text in (("main", main_dispatcher), ("OnProcess3", dispatcher)):
    for effect in ((249,) if label == "main" else (494, 520)):
        require(re.search(rf"case {effect}: return ProcessShieldBoomerangEffects\(\);", text),
                f"{label} ordered case list missing id {effect}")

print("ShieldBoomerang/3 caller source parity: 2 launcher bodies, routes 249/494/520, cadence/RNG/assets passed")
