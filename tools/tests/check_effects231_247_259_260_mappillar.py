"""Compare MapPillar source profiles, local routes and lifecycle fixtures."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/dhxj/RagEffect2.cpp").read_text(encoding="cp949", errors="replace")
SRC = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
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


def ref_body() -> str:
    match = re.search(r"void\s+CRagEffect\s*::\s*MAPPILLAR\s*\(\s*char\s*\*tName\s*,\s*char\s*PW\s*\)", REF)
    if not match:
        raise SystemExit("secondary Ref MAPPILLAR helper is missing")
    opening = REF.index("{", match.start())
    depth = 0
    for pos in range(opening, len(REF)):
        if REF[pos] == "{":
            depth += 1
        elif REF[pos] == "}":
            depth -= 1
            if depth == 0:
                return REF[match.start() : pos + 1]
    raise AssertionError("unterminated secondary Ref MAPPILLAR body")


pe_helper = body(PE, "void __thiscall CRagEffect::MAPPILLAR(CRagEffect *this, char *tName, char PW)")
ref_helper = ref_body()
for label, source in (("PE", pe_helper), ("secondary Ref", ref_helper)):
    required = (
        "m_stateCnt", "PP_MAPPILLAR", "m_renderFlag", "m_duration", "GetTexture",
        "m_size", "m_GI", "full_display_angle", "max_height", "RotStart", "alphaB",
        "distance", "alphaT", "rise_angle", "height",
    )
    if not all(marker in source for marker in required):
        raise SystemExit(f"{label} MAPPILLAR helper lacks a required source payload field")
if "flag1" not in ref_helper or "_BYTE *)&g_prim->m_GI" not in pe_helper:
    raise SystemExit("PE/secondary Ref MAPPILLAR height and flag-array clearing is not present")
    if not re.search(r"if\s*\(\s*!this->m_stateCnt\s*\)", source) if label == "PE" else "if (m_stateCnt == 0)" not in source:
        raise SystemExit(f"{label} MAPPILLAR state-zero gate changed")

for effect, call in {
    231: r"MAPPILLAR\(this,\s*aEffectRingBlue,\s*0\)",
    247: r"MAPPILLAR\(this,\s*aEffectRingBlue,\s*1\)",
    259: r"MAPPILLAR\(this,\s*aEffectMagicGre,\s*2\)",
    260: r"MAPPILLAR\(this,\s*aEffectRingRedT,\s*3\)",
}.items():
    if not re.search(rf"case\s+{effect}\s*:\s*CRagEffect::{call};", PE):
        raise SystemExit(f"canonical PE caller mapping for MAPPILLAR effect{effect} changed")

for effect, call in {
    231: r'ApplyMapPillar\("effect\\\\ring_blue\.tga",\s*4\.0f,\s*false\)',
    247: r'ApplyMapPillar\("effect\\\\ring_blue\.tga",\s*4\.0f,\s*true\)',
    259: r'ApplyMapPillar\("effect\\\\magic_green\.tga",\s*7\.0f,\s*false\)',
    260: r'ApplyMapPillar\("effect\\\\ring_red\.tga",\s*6\.0f,\s*false\)',
}.items():
    if not re.search(rf"case\s+{effect}:\s*{call};\s*return\s+true;", DISPATCH):
        raise SystemExit(f"local generated MAPPILLAR profile for effect{effect} changed")

local_helper = body(SRC, "void CRagEffect::ApplyMapPillar(const char* textureName, float size, bool wide)")
ordered = ("m_stateCnt != 0", "kPrimTypeMapPillar", "prim->m_renderFlag = 5;", "prim->m_duration = m_duration;",
           "prim->m_size = size;", "prim->m_pos = origin;", "prim->m_texture.push_back", "gi.life = 1;",
           "gi.process = band * 30;", "gi.maxHeight = 120.0f;", "gi.alphaB = static_cast<short>(wide ? 70 : 50);",
           "gi.distance = static_cast<float>(band) * 0.5f + (wide ? 11.0f : 2.0f);", "gi.height.fill(0.0f);", "gi.flag.fill(0);")
cursor = 0
for marker in ordered:
    found = local_helper.find(marker, cursor)
    if found < 0:
        raise SystemExit(f"local ApplyMapPillar payload/order changed at {marker!r}")
    cursor = found + len(marker)

fixture_start = FIXTURE.index("struct MapPillarCase")
fixture_end = FIXTURE.index("rngValues.clear(); rngCursor=0;", fixture_start)
fixture = FIXTURE[fixture_start:fixture_end]
for effect in (231, 247, 259, 260):
    if not re.search(rf"\{{{effect},", fixture):
        raise SystemExit(f"MAPPILLAR lifecycle fixture lacks effect{effect}")
for phrase in (
    "MAPPILLAR source profile preserves route size texture position and four GI bands",
    "MAPPILLAR emits only at state zero without repeat resource RNG or audio work",
):
    if phrase not in fixture:
        raise SystemExit(f"MAPPILLAR lifecycle assertion missing {phrase!r}")

print("MAPPILLAR PE callers, secondary Ref helper, local route and lifecycle profiles passed")
