"""Compare the effect224 WIND emitter, resources, route and lifecycle fixture."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
SECONDARY = (ROOT / "Ref/dhxj/RagEffect2.cpp").read_text(encoding="cp949", errors="replace")
SRC = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
FIXTURE = (ROOT / "tools/tests/EffectLifecycleTests.cpp").read_text(encoding="utf-8")
CALLSITES = (ROOT / "docs/skill/generated/playwave_callsites.tsv").read_text(encoding="utf-8")


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


def named_body(text: str, method: str) -> str:
    match = re.search(rf"void\s+CRagEffect\s*::\s*{method}\s*\(", text)
    if not match:
        raise SystemExit(f"{method} method is missing")
    opening = text.index("{", match.start())
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[match.start() : pos + 1]
    raise AssertionError(f"unterminated method: {method}")


pe_process = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
if not re.search(r"case 224:\s*CRagEffect::WIND\(this\);\s*break;", pe_process):
    raise SystemExit("PE effect224 caller no longer dispatches to WIND")
if "case 224: ApplyWind(); return true;" not in DISPATCH:
    raise SystemExit("local generated effect224 route changed")
if re.search(r"CRagEffect\s*::\s*WIND\s*\(", REF):
    raise SystemExit("primary Ref unexpectedly gained a WIND method; re-audit parity attribution")

pe_wind = body(PE, "void __thiscall CRagEffect::WIND(CRagEffect *this)")
ref_wind = named_body(SECONDARY, "WIND")
for label, source in (("PE", pe_wind), ("secondary Ref", ref_wind)):
    for marker in ("PP_WIND", "m_stateCnt", "m_duration", "full_display_angle",
                   "max_height", "RotStart", "alphaB", "distance", "rise_angle", "height"):
        if marker not in source:
            raise SystemExit(f"{label} WIND helper lacks source marker {marker!r}")
if "flag1" not in ref_wind or "_BYTE *)&g_prim->m_GI" not in pe_wind:
    raise SystemExit("PE/secondary Ref WIND flag-array clearing changed")
if "aEffectCloud11T" not in pe_wind or "effect\\\\cloud11.tga" not in ref_wind:
    raise SystemExit("PE/secondary Ref WIND texture mapping changed")
    for values in (("90", "3.0"), ("70", "3.2"), ("50", "3.4"), ("30", "3.6")):
        if not all(value in source for value in values):
            raise SystemExit(f"{label} WIND helper changed one of its ordered GI tuples")
    if "_heal_effect.wav" not in source:
        raise SystemExit(f"{label} WIND helper no longer plays the healing wave")
if not re.search(r"char\s+aHealEffectWav\[\]\s*=\s*\"_heal_effect\.wav\";", PE):
    raise SystemExit("PE healing wave resource literal changed")
if "0x62A468\tCRagEffect::WIND\t0x712840\t_heal_effect.wav\tok" not in CALLSITES:
    raise SystemExit("generated PE PlayWave callsite/resource VA no longer identifies WIND")
if "aEffectCloud11T" not in pe_wind or not re.search(r'char\s+aEffectCloud11T\[\]\s*=\s*"effect\\\\cloud11\.tga";', PE):
    raise SystemExit("PE cloud11 texture symbol/string evidence is missing")

local_wind = body(SRC, "void CRagEffect::ApplyWind()")
for ordered in (
    "if (m_stateCnt != 0)", "kPrimTypeWind", "prim->m_renderFlag = 5;", "prim->m_duration = m_duration;",
    "prim->m_size = 4.0f;", "prim->m_pos = origin;", 'g_texMgr.GetTexture("effect\\\\cloud11.tga", false)',
    "const short angles[4] = { 90, 70, 50, 30 };", "const short spins[4] = { 0, 10, 20, 30 };",
    "const float distances[4] = { 3.0f, 3.2f, 3.4000001f, 3.5999999f };",
    "const float heights[4] = { 3.0f, 2.5f, 2.0f, 1.5f };", "gi.life = 1;", "gi.process = 0;",
    "gi.fullDisplayAngle = angles[band];", "gi.maxHeight = 8.0f;", "gi.rotStart = spins[band];",
    "gi.distance = distances[band];", "gi.height.fill(heights[band]);", "gi.flag.fill(0);",
    'wavePos.y = mode->m_world->m_player->m_pos.y;', 'TryPlayEffectWaveAt(wavePos, "_heal_effect.wav");',
):
    if ordered not in local_wind:
        raise SystemExit(f"local ApplyWind route/payload changed at {ordered!r}")

fixture_start = FIXTURE.index("CRagEffect wind;")
fixture_end = FIXTURE.index("g_world.m_player=oldPlayer;", fixture_start)
fixture = FIXTURE[fixture_start:fixture_end]
for phrase in (
    "WIND effect224 emits four exact bands and frame-zero listener-relative wave without RNG",
    "WIND effect224 emits and plays audio only at state zero",
):
    if phrase not in fixture:
        raise SystemExit(f"effect224 lifecycle assertions missing {phrase!r}")

print("effect224 PE/secondary-Ref WIND profiles, local route/resources and lifecycle coverage passed")
