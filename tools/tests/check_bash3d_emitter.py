#!/usr/bin/env python3
"""Compare readable Ref, canonical EXE and current BASH3D emitter branches."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
REF = (ROOT / "Ref/dhxj/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
CANON = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
IMPL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")


def body(text: str, marker: str) -> str:
    start = text.index(marker)
    opening = text.index("{", start)
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{": depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[opening + 1:pos]
    raise AssertionError(f"unclosed body: {marker}")


ref = body(REF, "void\tCRagEffect :: BASH3D(char *tName,char F1,char F2)")
canonical = body(CANON, "void __thiscall CRagEffect::BASH3D(CRagEffect *this, char *tName, char F1, char F2)\n{")
impl = body(IMPL, "void CRagEffect::SpawnBash3D(")

checks = {
    "parent position refresh and frame-zero-only primitive launch": (
        re.search(r"m_pos = m_master->m_pos[\s\S]*?if \(m_stateCnt == 0\)[\s\S]*?LaunchEffectPrim\(PP_BASH3D\)", ref),
        re.search(r"this->m_pos = this->m_master->m_pos[\s\S]*?if \( !this->m_stateCnt \)[\s\S]*?LaunchEffectPrim\(this, PP_BASH3D", canonical),
        re.search(r"m_effectPos = master->m_pos[\s\S]*?if \(m_stateCnt != 0\) return[\s\S]*?LaunchEffectPrim\(PP_BASH3D", impl)),
    "primitive RF_EFFECT, parent duration and caller texture": (
        all(token in ref for token in ("m_renderFlag = RF_EFFECT", "m_duration", "GetTexture(tName)")),
        all(token in canonical for token in ("m_renderFlag = 5", "m_duration", "GetTexture(&g_texMgr, tName, 0)")),
        all(token in impl for token in ("prim->m_renderFlag = 5", "prim->m_duration = m_duration", "GetTexture(textureName, false)"))),
    "four lanes retain life, mode, process, distance, rise and alpha initialization": (
        all(token in ref for token in ("ec<4", "life=1", "height[4]=F2", "distance=2.0f", "alphaB=0", "height[0]=-12.0f")),
        all(token in canonical for token in ("v7 < 4", "height[4] = F2a", "distance = 2.0", "rand() % 360", "= -12.0")),
        all(token in impl for token in ("index < 4", "gi.life = 1", "gi.height[4]", "gi.distance = 2.0f", "gi.alphaB = 0", "gi.height[0] = -12.0f"))),
    "mode-specific 7/22 degree rise offsets and four angle RNG draws": (
        "F2 == 3" in ref and "F1*7" in ref and "F1*22" in ref and "random(360)" in ref,
        "F2 == 3" in canonical and "rand() % 360" in canonical,
        "f2 == 3 ? 7 : 22" in impl and "rand() % 360" in impl),
}

for label, (source_match, canonical_match, implementation_match) in checks.items():
    assert source_match, f"readable Ref comparison failed: {label}"
    assert canonical_match, f"canonical EXE comparison failed: {label}"
    assert implementation_match, f"implementation comparison failed: {label}"
    print(f"PASS {label}")
print(f"{len(checks)} mechanical BASH3D emitter comparisons passed")
