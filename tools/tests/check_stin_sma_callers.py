#!/usr/bin/env python3
"""Compare STIN/STIN2/SMA emitters and effect callers with canonical sources."""
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
ref_callers = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
local = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8", errors="replace")
onprocess3 = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")

helpers = (
    ("STIN", "void __thiscall CRagEffect::STIN(CRagEffect *this, char *tName)", "SpawnStin"),
    ("STIN2", "void __thiscall CRagEffect::STIN2(CRagEffect *this, char *tName, char F1)", "SpawnStin2"),
    ("SMA", "void __thiscall CRagEffect::SMA(CRagEffect *this, char F1)", "SpawnSma"),
)
for name, signature, local_name in helpers:
    require(compact(function(exe, signature)) == compact(function(ref, signature)),
            f"Ref and canonical {name} helper differ")
    require(f"void CRagEffect::{local_name}(" in local, f"{local_name} implementation missing")
    require(f"{local_name}" in header, f"{local_name} declaration missing")

main3_sig = "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)"
pe_callers = function(exe, main3_sig)
ref_callers_body = function(ref_callers, main3_sig)
for source, label in ((pe_callers, "PE"), (ref_callers_body, "Ref")):
    for operation in (
        "STIN(this, aEffect270)", "STIN2(this, aEffect270, 0)",
        "STIN2(this, aEffect270, 1)", "SMA(this, 1)",
    ):
        require(operation in source, f"{label} caller missing {operation}")
    require("PlayWave(aEffectT_18, xbf, 0.0, zbf, 250, 40, 1.0)" in source,
            f"{label} STIN sound branch mismatch")
    require("PlayWave(aEffectT_1, 0.0, v164, 0.0, 250, 40, 1.0)" in source,
            f"{label} STIN2 sound branch mismatch")
    require("PlayWave(aEffectT_14, xbd, 0.0, zbd, 250, 40, 1.0)" in source,
            f"{label} SMA sound branch mismatch")

image = ExeImage()
resources = {
    0x719A60: b"effect\\270\xB9\xD9\xB6\xF7.tga",
    0x7195D0: b"effect\\T_\xB9\xD9\xB6\xF7\xB8\xB6\xB9\xFD.wav",
    0x7195B8: b"effect\\T_\xB3\xAF\xB6\xF3\xC2\xF7\xB1\xE2.wav",
    0x71961C: b"effect\\T_\xBF\xA1\xB3\xCA\xC1\xF6\xB9\xE6\xC3\xE2.wav",
}
for address, expected in resources.items():
    value = image.read_cstring_at(address)
    require(value is not None and value.raw == expected, f"PE resource at {address:#x} mismatch")

dispatch = function(local, "bool CRagEffect::ProcessStinSmaEffects()")
for effect_id in (547, 553, 555):
    require(f"case {effect_id}: return ProcessStinSmaEffects();" in onprocess3,
            f"effect {effect_id} route missing")
for token in (
    "case 547:", "m_stateCnt == 10", "effect\\\\T_\\xB9\\xD9\\xB6\\xF7\\xB8\\xB6\\xB9\\xFD.wav",
    "SpawnStin()", "case 553:", "m_stateCnt == 5 || m_stateCnt == 20 || m_stateCnt == 35 || m_stateCnt == 50 || m_stateCnt == 65",
    "effect\\\\T_\\xB3\\xAF\\xB6\\xF3\\xC2\\xF7\\xB1\\xE2.wav", "SpawnStin2(0)", "SpawnStin2(1)",
    "case 555:", "effect\\\\T_\\xBF\\xA1\\xB3\\xCA\\xC1\\xF6\\xB9\\xE6\\xC3\\xE2.wav", "SpawnSma(1)",
):
    require(token in dispatch, f"caller operation missing: {token}")

stin = function(local, "void CRagEffect::SpawnStin()")
for token in (
    "LaunchEffectPrim(PP_STIN", "prim->m_renderFlag = 5", "prim->m_duration = m_duration",
    "effect\\\\270\\xB9\\xD9\\xB6\\xF7.tga", "prim->m_deltaPos = m_deltaPos", "prim->m_pos = m_effectPos",
    "gi.process = static_cast<int>(index) - 3", "gi.distance = 20.0f", "gi.alphaB = 0",
    "gi.sparkX = m_effectPos.x", "gi.sparkZ = m_effectPos.z", "rand() % 5", "gi.rotStart = 0",
    "gi.riseAngle = static_cast<short>(index)", "gi.height[1] = 0.80000001f", "gi.flag[0] = 0", "gi.flag[3] = 0",
):
    require(token in stin, f"STIN setup missing: {token}")

stin2 = function(local, "void CRagEffect::SpawnStin2(int variant)")
for token in (
    "LaunchEffectPrim(PP_STIN2", "effect\\\\270\\xB9\\xD9\\xB6\\xF7.tga", "gi.distance = 25.0f",
    "gi.sparkY = m_effectPos.y - 10.0f", "gi.riseAngle = static_cast<short>(index)", "gi.height[1] = 2.0f",
    "gi.flag[2] = static_cast<u8>(variant == 0 ? 0 : 1)", "gi.flag[3] = 0",
):
    require(token in stin2, f"STIN2 setup missing: {token}")
require("rand()" not in stin2, "STIN2 must not consume RNG")

sma = function(local, "void CRagEffect::SpawnSma(int variant)")
for token in (
    "LaunchEffectPrim(PP_SMA", "prim->m_totalTexture = 0", "gi.process = static_cast<int>(index) - 3",
    "rand() % 5", "gi.sparkY", "gi.height[1] = 0.80000001f", "gi.flag[0] = static_cast<u8>(variant)",
):
    require(token in sma, f"SMA setup missing: {token}")
print("STIN/STIN2/SMA helper, caller, resource, RNG and route checks passed")
