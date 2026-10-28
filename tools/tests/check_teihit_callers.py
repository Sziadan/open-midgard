#!/usr/bin/env python3
"""Compare the TeiHit setup helpers and effect callers with canonical sources."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage  # noqa: E402


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(message)


def function(source: str, signature: str) -> str:
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 0
    for cursor in range(opening, len(source)):
        if source[cursor] == "{":
            depth += 1
        elif source[cursor] == "}":
            depth -= 1
            if depth == 0:
                return source[start:cursor + 1]
    raise AssertionError(f"unterminated function {signature}")


def compact(source: str) -> str:
    return re.sub(r"\s+", "", source)


exe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
ref = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
local = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8", errors="replace")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")

helpers = (
    ("TEIHIT1", "void __thiscall CRagEffect::TEIHIT1(CRagEffect *this, char *tName, char F1)", "SpawnTeiHit1"),
    ("TEIHIT2", "void __thiscall CRagEffect::TEIHIT2(CRagEffect *this)", "SpawnTeiHit2"),
    ("TEIHIT3", "void __thiscall CRagEffect::TEIHIT3(CRagEffect *this, char *tName, char F1)", "SpawnTeiHit3"),
)
for name, signature, local_name in helpers:
    require(compact(function(exe, signature)) == compact(function(ref, signature)),
            f"Ref and canonical {name} helper differ")
    require(f"void CRagEffect::{local_name}(" in local, f"{local_name} implementation missing")
    require(f"{local_name}" in header, f"{local_name} declaration missing")

resources = {
    0x7192FC: "effect\\alpha_center.tga",
    0x71A490: "effect\\lens1.tga",
    0x719388: "effect\\thunder_center.bmp",
    0x71A474: "effect\\rog_back stap.wav",
    0x71A3A4: "effect\\lens2.tga",
}
image = ExeImage()
for address, expected in resources.items():
    value = image.read_cstring_at(address)
    require(value is not None and value.text == expected, f"PE resource at {address:#x} mismatch")

main = exe.index("unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)\n{")
start = exe.index("          case 262:", main)
end = exe.index("          case 277:", start)
caller = exe[start:end]
for expected in (
    "TEIHIT1(this, aEffectAlphaCen, 0)", "TEIHIT2(this)",
    "TEIHIT1(this, aEffectLens1Tga, 1)", "TEIHIT3(this, aEffectLens2Tga, 2)",
    "PlayWave(aEffectRogBackS, xba, 0.0, zba, 250, 40, 1.0)",
):
    require(expected in caller, f"canonical caller missing {expected}")

method = function(local, "bool CRagEffect::ProcessTeiHitEffects()")
for effect_id in (262, 264, 266, 275, 276):
    require(f"case {effect_id}:" in method, f"effect {effect_id} caller missing")
    require(f"case {effect_id}: return ProcessTeiHitEffects();" in dispatch,
            f"effect {effect_id} route missing")
require("for (int call = 0; call < 20; ++call) SpawnTeiHit1(\"effect\\\\alpha_center.tga\", 0)" in method,
        "effect 262 call count/resource mismatch")
require("for (int call = 0; call < 5; ++call) SpawnTeiHit2()" in method,
        "effect 264 call count mismatch")
require("for (int call = 0; call < 24; ++call) SpawnTeiHit1(\"effect\\\\lens1.tga\", 1)" in method,
        "effect 266 call count/resource mismatch")
case275 = method[method.index("case 275:"):method.index("case 276:")]
require("if (m_stateCnt == 20)" in case275 and case275.index("TryPlaySingleEffectWave") < case275.index("SpawnTeiHit2()"),
        "effect 275 wave timing/order mismatch")
require("for (int call = 0; call < 20; ++call) SpawnTeiHit3(\"effect\\\\lens2.tga\", 2)" in method,
        "effect 276 call count/resource mismatch")

for local_name in ("SpawnTeiHit1", "SpawnTeiHit2", "SpawnTeiHit3"):
    body = function(local, f"void CRagEffect::{local_name}(")
    require("index + 1 < prim->m_gi.size()" in body,
            f"{local_name} does not safely model the source's incremented flat stores")
    require("for (size_t index = 0; index < prim->m_gi.size(); ++index)" in body,
            f"{local_name} must retain all four source iterations and their RNG draws")
    require("next." in body, f"{local_name} shifted stores missing")
print("TeiHit helper, resource, caller, shifted-store and route checks passed")
