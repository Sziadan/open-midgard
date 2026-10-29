#!/usr/bin/env python3
"""Compare PE/Ref/local Tarot effect texture routes and selector-1 payload."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
ROUTES = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")


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
                return source[opening + 1:pos]
    raise AssertionError(f"unterminated function: {signature}")


def check_ref_routes(source: str, *, pe: bool) -> None:
    body = (function(source, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
            if pe else source)
    for effect_id in range(523, 537):
        n = effect_id - 522
        case_id = effect_id
        texture = f"Tarot{n:02d}"
        pattern = rf"case 0x{case_id:X}:\s*CRagEffect::EffectTextureSet\(this, aEffect{texture}T, 1\);\s*return 1;"
        if not re.search(pattern, body):
            raise AssertionError(f"effect{effect_id} {texture} selector-1 source route missing")


check_ref_routes(PE, pe=True)
check_ref_routes(REF, pe=False)
for effect_id in range(523, 537):
    texture = f"Tarot{effect_id - 522:02d}"
    pattern = rf"case {effect_id}: ApplyEffectTexture\(\"effect\\\\{texture}\.tga\", 1\); return true;"
    if not re.search(pattern, ROUTES):
        raise AssertionError(f"local effect{effect_id} {texture} route missing")

pe_helper = function(PE, "void __thiscall CRagEffect::EffectTextureSet(CRagEffect *this, char *tName, char F1)")
if not re.search(r"if\s*\(\s*this->m_stateCnt\s*\)\s*return;", pe_helper):
    raise AssertionError("PE EffectTextureSet later-state no-relaunch gate changed")
if not re.search(r"if\s*\(\s*F1\s*!=\s*1\s*\).*?distance\s*=\s*8\.0.*?height\[10\]\s*=\s*1\.0.*?full_display_angle\s*=\s*120", pe_helper, re.S):
    raise AssertionError("PE EffectTextureSet selector-1 default payload changed")

local_helper = function(LOCAL, "void CRagEffect::ApplyEffectTexture(const char* textureName, int variant)")
for token in (
    "m_stateCnt != 0", "kPrimTypeEffectTexture", "prim->m_renderFlag = 5",
    "prim->m_duration = m_duration", "prim->m_size = 0.0f", "prim->m_pos = origin",
    "prim->m_texture.push_back(g_texMgr.GetTexture(textureName, false))",
    "gi.life = 1", "gi.process = 0", "gi.distance = 8.0f",
    "gi.arcBaselineY = origin.y - 30.0f", "gi.height[10] = 1.0f",
    "gi.fullDisplayAngle = 120", "gi.height[8] = 0.0f", "gi.alphaB = 0",
    "gi.sparkX = origin.x", "gi.sparkZ = origin.z", "rand() % 360",
    "gi.flag[0] = 0", "gi.flag[4] = static_cast<u8>(variant == 4 ? 1 : variant)",
):
    if token not in local_helper:
        raise AssertionError(f"local selector-1 texture payload missing {token}")

print("effects523-536 Tarot parity passed: PE/Ref/local 14-way routes, selector-1 payload and spawn gate")
