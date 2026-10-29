"""Compare effect34 Teleportation PE/Ref bodies with the local route and fixture."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage, SymbolTable


PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
SRC = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
FIXTURE = (ROOT / "tools/tests/EffectLifecycleTests.cpp").read_text(encoding="utf-8")


def body(text: str, signature: str, start: int | None = None) -> str:
    begin = text.index(signature, start or 0)
    opening = text.index("{", begin)
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[begin:pos + 1]
    raise AssertionError(f"unterminated body: {signature}")


signature = "void __thiscall CRagEffect::Teleportation(CRagEffect *this)"
pe_start = PE.index(signature + "; // idb") + 1
pe_handler = body(PE, signature, pe_start)
ref_handler = body(REF, signature)
local_handler = body(SRC, "void CRagEffect::ApplyTeleportation()")
pe_process = body(
    PE,
    "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)",
    PE.index("unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this); // idb") + 1,
)
legacy_process = body(SRC, "bool CRagEffect::ProcessEffectFrameLegacy()")

if re.sub(r"\s+", " ", pe_handler).strip() != re.sub(r"\s+", " ", ref_handler).strip():
    raise SystemExit("canonical PE and primary Ref Teleportation bodies differ")
if "case 34:" not in pe_process or "CRagEffect::Teleportation(this)" not in pe_process:
    raise SystemExit("canonical PE effect34 route to Teleportation is missing")
if "case 34: return ProcessEffectFrameLegacy();" not in DISPATCH:
    raise SystemExit("local generated effect34 route is missing")
if not re.search(r"if\s*\(m_type\s*==\s*34\)\s*\{\s*ApplyTeleportation\(\);\s*return\s+true;\s*\}", legacy_process):
    raise SystemExit("local legacy route no longer dispatches effect34 to ApplyTeleportation")
if "void ApplyTeleportation();" not in HEADER:
    raise SystemExit("local ApplyTeleportation declaration is missing")

ordered = (
    "if (m_stateCnt != 0) return", 'PlayWave("effect\\\\EF_Teleportation.wav"',
    "m_effectPos.x-listener.x", "m_effectPos.z-listener.z",
    "LaunchEffectPrim(PP_3DCYLINDER", "prim->m_duration = 60", "prim->m_pattern |= 1",
    "prim->m_outerSize = 4.0f", "prim->m_heightSpeed = 5.0f",
    "prim->m_heightAccel = 0.25f", "prim->m_longSpeed = 2.0f",
    "prim->m_fadeOutCnt = 0", 'GetTexture("effect\\\\ring_blue.tga"',
)
cursor = 0
for token in ordered:
    found = local_handler.find(token, cursor)
    if found < 0:
        raise SystemExit(f"local Teleportation source behavior missing/out of order: {token}")
    cursor = found + len(token)
if re.search(r"\brand\s*\(", pe_handler) or re.search(r"\brand\s*\(", local_handler):
    raise SystemExit("Teleportation unexpectedly consumes RNG")

symbols = SymbolTable(ExeImage(ROOT / "HighPriest.exe"))
for name, va, raw in (
    ("aEffectEfTelepo", 0x71B744, b"effect\\EF_Teleportation.wav"),
    ("aEffectRingBlue", 0x71943C, b"effect\\ring_blue.tga"),
):
    resolved = symbols.resolve_name(name)
    if resolved is None or resolved.va != va or resolved.raw != raw:
        raise SystemExit(f"Teleportation PE resource bytes changed for {name}: {resolved!r}")

for phrase in (
    "Teleportation emits one reference cylinder without RNG despite reduced effects",
    "Teleportation plays exact stored-origin horizontal wave before cylinder setup",
    "Teleportation preserves source cylinder motion profile and constructor height cap arc",
    "Teleportation full-cylinder update grows source height, double-advances longitude, and sets outer shrink speed",
    "Teleportation processes duration frame sixty then terminates with faded alpha",
):
    if phrase not in FIXTURE:
        raise SystemExit(f"Teleportation lifecycle fixture missing {phrase!r}")

print("effect34 Teleportation PE/Ref body, local route, resource bytes and lifecycle coverage passed")
