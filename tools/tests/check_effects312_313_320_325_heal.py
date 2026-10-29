#!/usr/bin/env python3
"""Compare Heal/Heal2 callers and four variant mappings across PE, Ref and local code."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage, SymbolTable  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
REF2 = (ROOT / "Ref/dhxj/RagEffect2.cpp").read_text(encoding="latin-1")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
DISPATCH2 = (ROOT / "src/world/RagEffectOnProcess2Cases.generated.inc").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(message)


def function(source: str, signature: str) -> str:
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{": depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[start:pos + 1]
    raise AssertionError(signature)


def process_case(source: str, signature: str, case_label: str) -> str:
    body = function(source, signature)
    match = re.search(rf"case {case_label}:(.*?)(?=\n\s*case |\n\s*default:|\Z)", body, re.S)
    require(match is not None, f"OnProcess case {case_label} missing")
    return match.group(1)


for label, expected in (
    ("0x138", "CRagEffect::Heal(this, aEffectAlphaDow, 0);"),
    ("0x139", "CRagEffect::Heal2(this, aEffectRingWhit, 0);"),
    ("0x140", "CRagEffect::Heal(this, aEffectAlphaDow, 1);"),
    ("0x145", "CRagEffect::Heal2(this, aEffectRingWhit, 1);"),
):
    pe_case = process_case(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess2(CRagEffect *this)", label)
    ref_case = process_case(REF, "unsigned __int8 __thiscall CRagEffect::OnProcess2(CRagEffect *this)", label)
    require(re.sub(r"\s+", "", pe_case) == re.sub(r"\s+", "", ref_case),
            f"PE/primary-Ref case {label} differs")
    require(expected in pe_case, f"PE case {label} helper/variant mapping changed")

pe_heal = function(PE, "void __thiscall CRagEffect::Heal(CRagEffect *this, char *tName, char F1)")
ref_heal = function(REF2, "void\tCRagEffect :: Heal(char *tName, char F1)")
for label, body in (("PE Heal", pe_heal), ("secondary-Ref Heal", ref_heal)):
    for token in ("PP_HEAL", "m_duration", "full_display_angle", "RotStart",
                  "alphaT", "max_height", "distance", "rise_angle"):
        require(token in body, f"{label} helper missing {token}")
require("if ( m_stateCnt )" in pe_heal and "else" in pe_heal
        and "m_stateCnt == 0" in ref_heal, "Heal state-zero and follow-up branches changed")
require("m_stateCnt <= (F1 != 0 ? 25 : 50)" in pe_heal
        and "rand() % 40" in pe_heal and "0.40000001" in pe_heal and "0.30000001" in pe_heal,
        "PE Heal follow-up window or variant particle-up speeds changed")
require("F1 == 0" in ref_heal and "F1==1" in ref_heal and "max_height = 40.0f + random(20)" in ref_heal,
        "secondary-Ref Heal size/heal variant mapping changed")

pe_heal2 = function(PE, "void __thiscall CRagEffect::Heal2(CRagEffect *this, char *tName, char F1)")
ref_heal2 = function(REF2, "void\tCRagEffect :: Heal2(char *tName , char F1)")
for label, body in (("PE Heal2", pe_heal2), ("secondary-Ref Heal2", ref_heal2)):
    for token in ("PP_HEAL", "full_display_angle", "max_height", "RotStart",
                  "alphaB", "distance", "rise_angle"):
        require(token in body, f"{label} helper missing {token}")
require("if ( m_stateCnt )" in pe_heal2 and "else" in pe_heal2
        and "m_stateCnt == 0" in ref_heal2, "Heal2 state-zero and follow-up branches changed")
require("m_stateCnt <= 50" in pe_heal2 and "m_stateCnt % 6" in pe_heal2
        and "this->m_stateCnt == 1" in pe_heal2 and "v24 % 9 == 2" in pe_heal2,
        "PE Heal2 follow-up schedules changed")
require("F1 == 2" in ref_heal2 and "m_size = 4" in ref_heal2 and "F1 == 0" in ref_heal2,
        "secondary-Ref Heal2 texture/size/alpha variants changed")

symbols = SymbolTable(ExeImage())
for name, expected in (("aEffectAlphaDow", b"effect\\alpha_down.tga"),
                       ("aEffectRingWhit", b"effect\\ring_white.tga"),
                       ("aEffectPok1Tga", b"effect\\pok1.tga"),
                       ("aEffectPok3Tga", b"effect\\pok3.tga"),
                       ("aHealEffectWav", b"_heal_effect.wav")):
    value = symbols.resolve_name(name)
    require(value is not None and value.raw == expected, f"PE resource bytes changed: {name}")

for effect, route in (
    (312, 'case 312: ApplyHeal("effect\\\\alpha_down.tga", 0); return true;'),
    (313, 'case 313: ApplyHeal2("effect\\\\ring_white.tga", 0); return true;'),
    (320, 'case 320: ApplyHeal("effect\\\\alpha_down.tga", 1); return true;'),
    (325, 'case 325: ApplyHeal2("effect\\\\ring_white.tga", 1); return true;'),
):
    require(route in DISPATCH2, f"local effect{effect} variant route changed")

heal = function(LOCAL, "void CRagEffect::ApplyHeal(const char* textureName, int variant)")
for token in ("m_stateCnt != 0", "const int limit = variant == 1 ? 25 : 50", "m_stateCnt % 6",
              "LaunchParticleUp(origin", "_heal_effect.wav", "kPrimTypeHeal",
              "variant == 1 ? 5.0f : 9.0f", "variant == 1 ? 30 : 60"):
    require(token in heal, f"local Heal variant implementation missing {token}")
heal2 = function(LOCAL, "void CRagEffect::ApplyHeal2(const char* textureName, int variant)")
for token in ("variant < 2", "m_stateCnt == 0", "variant != 0 ? 40 : 60", "m_stateCnt <= 50",
              "m_stateCnt % 6", "m_stateCnt == 1", "m_stateCnt % 9", "LaunchParticleUp(origin"):
    require(token in heal2, f"local Heal2 variant implementation missing {token}")
print("effects312/313/320/325 Heal parity passed: PE/Ref callers/helpers, exact known assets, local variant routes and schedules")
