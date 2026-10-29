#!/usr/bin/env python3
"""Compare effect610/611 cue-plus-texture caller sequences and local profiles."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage, SymbolTable  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
REF_DHXJ = (ROOT / "Ref/dhxj/RagEffect2.cpp").read_text(encoding="latin-1")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
ROUTE = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")


def function_body(source, signature):
    for match in reversed(list(re.finditer(re.escape(signature), source))):
        start = match.start()
        opening = source.find("{", start + len(signature))
        if opening < 0 or source.find(";", start + len(signature), opening) >= 0:
            continue
        depth = 0
        for pos in range(opening, len(source)):
            if source[pos] == "{":
                depth += 1
            elif source[pos] == "}":
                depth -= 1
                if depth == 0:
                    return source[opening + 1:pos]
    raise AssertionError(f"function body not found: {signature}")


def compact(value):
    return re.sub(r"\s+", "", value)


signature = "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)"
pe_body = function_body(PE, signature)
ref_body = function_body(REF, signature)
for label, body in (("PE", pe_body), ("primary Ref", ref_body)):
    for effect_id, wave, texture in (
        (610, "aHealEffectWav", "aEffectSuccessB"),
        (611, "aCaramelDieWav", "aEffectFailedBm"),
    ):
        case = re.search(rf"case 0x{effect_id:X}:(.*?)(?=\n\s*case |\Z)", body, re.S)
        if case is None:
            raise AssertionError(f"{label} effect{effect_id} case missing")
        expected = (rf"if\s*\(\s*!\s*this->m_stateCnt\s*\).*?PlayWave\(\s*{wave},.*?250,\s*40,\s*1\.0\).*?"
                    rf"EffectTextureSet\(this,\s*{texture},\s*3\);\s*return 1;")
        if not re.search(expected, case.group(1), re.S):
            raise AssertionError(f"{label} effect{effect_id} state-zero cue/texture sequence changed")
        if f"case {effect_id}: return ProcessTarotEffect();" not in ROUTE:
            raise AssertionError(f"local generated effect{effect_id} route changed")

pe_helper = function_body(PE, "void __thiscall CRagEffect::EffectTextureSet(CRagEffect *this, char *tName, char F1)")
ref_helper = function_body(REF_DHXJ, "void CRagEffect :: EffectTextureSet(char *tName , char F1)")
for label, body, selector_token in (("PE", pe_helper, "case 3:"), ("secondary Ref", ref_helper, "F1 == 3")):
    for token in (selector_token, "distance = 8.0", "flag1[2] = 3", "height[10] = 2", "full_display_angle = 155"):
        if token not in body:
            raise AssertionError(f"{label} EffectTextureSet selector-3 profile missing {token}")

local = function_body(LOCAL, "bool CRagEffect::ProcessTarotEffect()")
case610 = re.search(r"case 610:\s*case 611:(.*?)(?=\n\s*case |\Z)", local, re.S)
if case610 is None:
    raise AssertionError("local effects610/611 grouped handler missing")
for token in (
    "if (m_stateCnt == 0)", 'm_type == 610 ? "_heal_effect.wav" : "caramel_die.wav"',
    'ApplyEffectTexture(m_type == 610 ? "effect\\\\success.bmp" : "effect\\\\failed.bmp", 3)',
    "wavePos.y = mode->m_world->m_player->m_pos.y", "TryPlayEffectWaveAt(wavePos",
):
    if token not in case610.group(1):
        raise AssertionError(f"local effects610/611 cue/texture mapping missing {token}")
if case610.group(1).find("TryPlayEffectWaveAt") > case610.group(1).find("ApplyEffectTexture"):
    raise AssertionError("local effects610/611 texture must follow its state-zero wave")
for token in (
    "if (variant == 3)", "gi.distance = 8.0f", "gi.arcBaselineY = origin.y - 30.0f",
    "gi.flag[2] = 3", "gi.height[10] = 2.0f", "gi.fullDisplayAngle = 155",
):
    if token not in function_body(LOCAL, "void CRagEffect::ApplyEffectTexture(const char* textureName, int variant)"):
        raise AssertionError(f"local EffectTextureSet variant-3 profile missing {token}")

symbols = SymbolTable(ExeImage())
for symbol, expected in (
    ("aHealEffectWav", b"_heal_effect.wav"), ("aCaramelDieWav", b"caramel_die.wav"),
    ("aEffectSuccessB", b"effect\\success.bmp"), ("aEffectFailedBm", b"effect\\failed.bmp"),
):
    actual = symbols.resolve_name(symbol)
    if actual is None or actual.raw != expected:
        raise AssertionError(f"PE resource bytes for {symbol} differ: {actual!r}")

print("effects610/611 parity passed: ordered PE/Ref callers, selector-3 helper profile, local state-zero cues and exact resources")
