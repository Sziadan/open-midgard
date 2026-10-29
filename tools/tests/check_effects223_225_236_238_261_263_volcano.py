"""Compare the VOLCANO emitter family across PE, secondary Ref and local code."""
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


def ref_body(name: str) -> str:
    match = re.search(rf"void\s+CRagEffect\s*::\s*{name}\s*\(", REF)
    if not match:
        raise SystemExit(f"secondary Ref {name} helper is missing")
    opening = REF.index("{", match.start())
    depth = 0
    for pos in range(opening, len(REF)):
        if REF[pos] == "{":
            depth += 1
        elif REF[pos] == "}":
            depth -= 1
            if depth == 0:
                return REF[match.start() : pos + 1]
    raise AssertionError(f"unterminated secondary Ref helper {name}")


def require_order(source: str, label: str, *markers: str) -> None:
    cursor = 0
    for marker in markers:
        found = source.find(marker, cursor)
        if found < 0:
            raise SystemExit(f"{label} missing or reorders source marker {marker!r}")
        cursor = found + len(marker)


pe_process = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
caller_maps = {
    223: r"CRagEffect::VOLCANO\(this,\s*aEffectRingWhit,\s*2\)",
    225: r"CRagEffect::VOLCANO\(this,\s*aEffectRingRedT,\s*0\)",
    236: r"CRagEffect::VOLCANO\(this,\s*aEffectRingBlue,\s*0\)",
    237: r"CRagEffect::VOLCANO\(this,\s*aEffectMagicGre,\s*0\)",
    238: r"CRagEffect::VOLCANO\(this,\s*aEffectRingWhit,\s*0\)",
    261: r"CRagEffect::VOLCANO2\(this,\s*aEffectRingYell,\s*0\)",
    263: r"CRagEffect::VOLCANO\(this,\s*aEffectRingYell,\s*1\)",
}
for effect, call in caller_maps.items():
    if not re.search(rf"case\s+{effect}\s*:\s*{call}\s*;", pe_process):
        raise SystemExit(f"PE effect{effect} no longer has its source VOLCANO call profile")

for name, required in (
    ("VOLCANO", ("F1 == 2", "PP_GI_2", "m_renderFlag = 5", "m_size=0", "max_height = 15.0f", "rise_angle=70", "max_height = 25.0f", "rise_angle=80", "RotStart = 90*ec", "alphaT=F1")),
    ("VOLCANO2", ("PP_GI_2", "m_renderFlag = 5", "m_size=0", "m_accel=99", "max_height = 25.0f", "RotStart = 90*ec", "rise_angle=80", "alphaT=F1")),
):
    pe_helper = body(PE, f"void __thiscall CRagEffect::{name}(CRagEffect *this, char *tName, char F1)")
    secondary_ref = ref_body(name)
    if not re.search(r"if\s*\(\s*!this->m_stateCnt\s*\)", pe_helper) or "if (m_stateCnt  == 0)" not in secondary_ref:
        raise SystemExit(f"{name} source state-zero gate changed")
    for marker in required:
        if marker not in pe_helper and marker not in secondary_ref:
            raise SystemExit(f"PE/secondary Ref {name} no longer documents {marker!r}")
    gi_life = "m_GI[i].life = 1" if name == "VOLCANO" else "m_GI[v6].life = 1"
    require_order(pe_helper, f"PE {name}", "m_renderFlag = 5", "m_duration = this->m_duration", "m_texture =", "m_size = 0.0", gi_life)

local_maps = {
    223: r'ApplyVolcano\("effect\\\\ring_white\.tga",\s*2,\s*false\)',
    225: r'ApplyVolcano\("effect\\\\ring_red\.tga",\s*0,\s*false\)',
    236: r'ApplyVolcano\("effect\\\\ring_blue\.tga",\s*0,\s*false\)',
    237: r'ApplyVolcano\("effect\\\\magic_green\.tga",\s*0,\s*false\)',
    238: r'ApplyVolcano\("effect\\\\ring_white\.tga",\s*0,\s*false\)',
    261: r'ApplyVolcano\("effect\\\\ring_yellow\.tga",\s*0,\s*true\)',
    263: r'ApplyVolcano\("effect\\\\ring_yellow\.tga",\s*1,\s*false\)',
}
for effect, call in local_maps.items():
    if not re.search(rf"case\s+{effect}:\s*{call};\s*return\s+true;", DISPATCH):
        raise SystemExit(f"local generated route for VOLCANO effect{effect} changed")

local_helper = body(SRC, "void CRagEffect::ApplyVolcano(const char* textureName, short variant, bool fastSpin)")
require_order(local_helper, "local ApplyVolcano", "m_stateCnt != 0", "m_renderFlag = 5", "m_duration = m_duration", "m_size = 0.0f", "m_accel = fastSpin ? 99.0f : 0.0f", "const bool compact = variant == 2", "gi.maxHeight = compact ? 15.0f : 25.0f", "gi.alphaT = variant")

fixture_start = FIXTURE.index("struct VolcanoCase")
fixture_end = FIXTURE.index("rngValues.clear(); rngCursor=0;", fixture_start)
fixture = FIXTURE[fixture_start:fixture_end]
for effect in (223, 225, 236, 237, 238, 261, 263):
    if not re.search(rf"\{{{effect},", fixture):
        raise SystemExit(f"VOLCANO lifecycle fixture does not instantiate effect{effect}")
for phrase in (
    "VOLCANO source profile preserves F1 variant bands texture position and silent RNG behavior",
    "VOLCANO emits only at state zero with no later texture RNG or audio work",
):
    if phrase not in fixture:
        raise SystemExit(f"VOLCANO lifecycle assertions missing {phrase!r}")

print("VOLCANO/VOLCANO2 PE callers, secondary Ref helper profiles, local routing and lifecycle coverage passed")
