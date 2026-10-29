"""Compare effect33 Ruwach PE/Ref helper and local route with lifecycle evidence."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage


PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
SRC = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
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


signature = "void __thiscall CRagEffect::Ruwach(CRagEffect *this)"
pe_handler = body(PE, signature, PE.index(signature + "; // idb") + 1)
ref_handler = body(REF, signature)
local_handler = body(SRC, "void CRagEffect::SpawnRuwach()")
pe_process = body(
    PE,
    "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)",
    PE.index("unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this); // idb") + 1,
)

if re.sub(r"\s+", " ", pe_handler).strip() != re.sub(r"\s+", " ", ref_handler).strip():
    raise SystemExit("canonical PE and primary Ref Ruwach helper bodies differ")
if not re.search(r"case\s+33\s*:\s*CRagEffect::Ruwach\(this\)", pe_process):
    raise SystemExit("canonical PE effect33 route to Ruwach is missing")
if "case 33: SpawnRuwach(); return true;" not in DISPATCH:
    raise SystemExit("local generated effect33 route is missing")

ordered = (
    "if (!master) return", "m_effectPos = master->m_pos", "if (m_stateCnt % 3 != 0) return",
    "static_cast<double>(m_stateCnt) * 0.7692307692307692 * -5.0",
    "const float sine = GetSin(angle)", "const float cosine = GetCos(angle)",
    "GetRadian(angle)", "LaunchEffectPrim(PP_3DPARTICLE", "prim->m_duration = 25",
    'particle2.spr', 'particle2.act', "prim->m_fadeOutCnt = prim->m_duration - 6",
    "ShiftCircleSegments(prim)", "LaunchEffectPrim(PP_3DPARTICLE", "prim->m_duration = 25",
    'SetParticleResources("Shadow.spr", "Shadow.act")', "prim->m_animSpeed = 2",
    "prim->m_fadeOutCnt = prim->m_duration - 6", "ShiftCircleSegments(prim)",
)
cursor = 0
for token in ordered:
    found = local_handler.find(token, cursor)
    if found < 0:
        raise SystemExit(f"local Ruwach behavior missing or out of order: {token}")
    cursor = found + len(token)
if re.search(r"\brand\s*\(", pe_handler) or re.search(r"\brand\s*\(", local_handler):
    raise SystemExit("Ruwach unexpectedly consumes RNG")

image = ExeImage(ROOT / "HighPriest.exe")
for va, expected in (
    (0x71B334, bytes.fromhex("c0ccc6d1c6ae5c7061727469636c65322e737072")),
    (0x71B31C, bytes.fromhex("c0ccc6d1c6ae5c7061727469636c65322e616374")),
    (0x71B558, b"Shadow.spr"),
    (0x71B54C, b"Shadow.act"),
):
    resolved = image.read_cstring_at(va)
    if resolved is None or resolved.raw != expected:
        raise SystemExit(f"Ruwach PE resource bytes changed at 0x{va:X}: {resolved!r}")

for phrase in (
    "Ruwach copies master position even on non-emission frames",
    "Ruwach emits two particles every third frame without RNG or wave calls",
    "Ruwach uses original sprite/action names and sprite-then-action load order",
    "Ruwach angle truncates toward zero to integer degrees before trig lookup",
    "Ruwach particle/shadow share horizontal offset with distinct source heights",
    "Ruwach retains twenty-five-frame duration and frame-nineteen fade",
    "Ruwach restores reference constructor motion and alpha defaults",
):
    if phrase not in FIXTURE:
        raise SystemExit(f"Ruwach lifecycle fixture missing {phrase!r}")

print("effect33 Ruwach PE/Ref helper, effect33 route, resource bytes and lifecycle coverage passed")
