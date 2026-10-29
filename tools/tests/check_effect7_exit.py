"""Compare effect7 Exit PE/Ref body, local route and lifecycle evidence."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
SRC = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
HEADER = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
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


signature = "void __thiscall CRagEffect::Exit(CRagEffect *this)"
pe_helper = body(PE, signature)
ref_helper = body(REF, signature)
normalize = lambda value: re.sub(r"\s+", "", value)
if normalize(pe_helper) != normalize(ref_helper):
    raise SystemExit("canonical PE and primary Ref Exit helpers differ")

process = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
case7 = process[process.index("case 7:") : process.index("case 8:")]
if "CRagEffect::Exit(this);" not in case7:
    raise SystemExit("canonical PE effect7 caller no longer routes to Exit")
if "Exit(this)" in REF:
    raise SystemExit("primary Ref caller omission changed; review PE-only effect7 provenance")
if "bool ProcessExit7();" not in HEADER or "case 7: return ProcessExit7();" not in DISPATCH:
    raise SystemExit("local effect7 declaration or generated route is missing")

local = body(SRC, "bool CRagEffect::ProcessExit7()")
ordered = (
    "ResolveCurrentMaster()", "if (master == nullptr)", "m_effectPos = master->m_pos", "if (m_stateCnt == 0)",
    "LaunchEffectPrim(PP_3DCYLINDER", "m_duration = 100", "cylinder->m_outerSize = 4.5f",
    '"effect\\\\alpha_down.tga"', 'PlayWave("_heal_effect.wav"',
    "if (!g_session.m_isMinEffect && (m_stateCnt % 9) == 0)", "LaunchEffectPrim(PP_3DPARTICLEORBIT",
    "(void)rand()", "orbit->m_radius = 3.0f", "orbit->m_duration = 50", "rand() % 360",
    '"\\xC0\\xCC\\xC6\\xD1\\xC6\\xAE\\\\particle1.spr"',
    '"\\xC0\\xCC\\xC6\\xD1\\xC6\\xAE\\\\particle1.act"',
    "orbit->m_numSegments = 1", "InitReferenceSegments(orbit)",
)
cursor = 0
for token in ordered:
    found = local.find(token, cursor)
    if found < 0:
        raise SystemExit(f"local Exit7 operation order missing: {token}")
    cursor = found + len(token)

image = ExeImage(ROOT / "HighPriest.exe")
for va, expected in (
    (0x719498, b"effect\\alpha_down.tga"),
    (0x71B200, b"\xc0\xcc\xc6\xd1\xc6\xae\\particle1.spr"),
    (0x71B1E8, b"\xc0\xcc\xc6\xd1\xc6\xae\\particle1.act"),
):
    value = image.read_cstring_at(va)
    if value is None or value.raw != expected:
        raise SystemExit(f"effect7 executable asset differs at 0x{va:X}: {value!r}")
if 'char aHealEffectWav[] = "_heal_effect.wav"' not in PE:
    raise SystemExit("effect7 source wave literal is missing from the executable decompilation")

for phrase in (
    "Exit7 refreshes the stored effect origin from its master and remains active",
    "Exit7 frame zero launches its cylinder before the cadence orbit",
    "Exit7 initializes the exact source cylinder profile and alpha-down texture",
    "Exit7 plays its frame-zero listener-relative wave before orbit RNG",
    "Exit7 discards the first orbit draw then preserves angle and gravity profile",
    "Exit7 orbit loads the exact particle sprite/action pair and initializes one trail segment",
    "Exit7 initializes its orbit trail from the primitive's current alpha",
    "Exit7 refreshes its parent each frame without repeating frame-zero output",
    "Exit7 emits another orbit every ninth frame with the source two-draw sequence",
    "Exit7 reduced-effects mode suppresses cadence orbits while still refreshing origin",
    "Exit7 cleanup releases orbit sprite/action resources",
):
    if phrase not in FIXTURE:
        raise SystemExit(f"Exit7 lifecycle fixture missing {phrase!r}")

print("effect7 Exit PE/Ref helper, PE/local route, exact assets and lifecycle coverage passed")
