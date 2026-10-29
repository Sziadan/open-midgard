"""Compare effect253 HealBlue caller, variant-two ring, sound bytes and route."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage, SymbolTable  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
REF_HELPERS = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="latin-1")
SRC = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
FIXTURE = (ROOT / "tools/tests/EffectLifecycleTests.cpp").read_text(encoding="utf-8")


def body(text: str, signature: str, start: int = 0) -> str:
    begin = text.index(signature, start)
    opening = text.index("{", begin)
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[begin : pos + 1]
    raise AssertionError(f"unterminated function: {signature}")


def named_body(text: str, method: str) -> str:
    matches = list(re.finditer(rf"void\s+(?:__thiscall\s+)?CRagEffect\s*::\s*{method}\s*\(", text))
    if not matches:
        raise SystemExit(f"{method} helper is missing")
    return body(text, matches[-1].group(0), matches[-1].start())


pe_process = body(
    PE,
    "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)",
    PE.index("unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this); // idb") + 1,
)
case_start = pe_process.index("case 253:")
caller = pe_process[case_start : pe_process.index("case 254:", case_start)]
if not re.search(r"if\s*\(\s*!this->m_stateCnt\s*\).*?PlayWave\(aEffect_28, x, -80\.0, z, 250, 40, 1\.0\).*?CRagEffect::Heal2\(this, aEffectRingBlue, 2\);\s*break;", caller, re.S):
    raise SystemExit("PE effect253 frame-zero wave then Heal2(blue,2) route changed")

image = ExeImage()
symbols = SymbolTable(image)
for symbol, expected, va in (("aEffect_28", "effect\\", 0x6F1FF0),
                             ("aEffectRingBlue", "effect\\ring_blue.tga", None)):
    resolved = symbols.resolve_name(symbol)
    if resolved is None or resolved.text != expected or (va is not None and resolved.va != va):
        raise SystemExit(f"PE bytes changed for {symbol}; expected {expected!r}")

pe_heal2 = named_body(PE, "Heal2")
ref_heal2 = named_body(REF_HELPERS, "Heal2")
if re.sub(r"\s+", "", pe_heal2) != re.sub(r"\s+", "", ref_heal2):
    raise SystemExit("PE and primary Ref Heal2 helpers differ after whitespace normalization")
for token in ("F1 < 2", "m_stateCnt <= 50", "m_stateCnt % 6", "PP_PARTICLE_UP",
              "F1 == 2", "PP_HEAL", "m_size = 4.0", "max_height = 60.0",
              "distance = 4.5999999", "RotStart = 180", "max_height = 18.0",
              "RotStart = rand() % 360", "distance = 5.1999998"):
    if token not in pe_heal2:
        raise SystemExit(f"PE Heal2 variant/state branch missing {token!r}")

ref_process = body(REF, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
ref_group = ref_process[ref_process.index("case 0x251:") : ref_process.index("case 0x257:")]
if not all(f"case 0x{effect:X}:" in ref_group for effect in range(0x251, 0x257)):
    raise SystemExit("primary Ref grouped 251-256 caller structure changed")
if "CRagEffect::Heal2" in ref_group or "PlayWave(aEffect_28" in ref_process:
    raise SystemExit("primary Ref unexpectedly gained the PE HealBlue caller")
if "PlayWave(aHealEffectWav, xbk, 0.0, zbk, 250, 40, 1.0)" not in ref_process:
    raise SystemExit("primary Ref generic grouped heal wave changed")
if "ProcessEZ2STR(this, 0.0, -80.0, 0.0)" not in ref_process:
    raise SystemExit("primary Ref generic grouped heal STR tail changed")

local_process = body(SRC, "bool CRagEffect::ProcessHealBlue()")
local_heal2 = body(SRC, "void CRagEffect::ApplyHeal2(const char* textureName, int variant)")
if 'case 253: return ProcessHealBlue();' not in DISPATCH:
    raise SystemExit("generated local effect253 route changed")
for token in ('if (m_stateCnt == 0)', 'wavePos.y = mode->m_world->m_player->m_pos.y - 80.0f;',
              'TryPlayEffectWaveAt(wavePos, "effect\\\\");',
              'ApplyHeal2("effect\\\\ring_blue.tga", 2);', 'return true;'):
    if token not in local_process:
        raise SystemExit(f"local ProcessHealBlue route missing {token!r}")
for token in ('if (variant < 2)', 'variant == 2 ? 4.0f : 9.0f',
              'ConfigureHealBand(prim->m_gi[0], 60.0f, 0, BitsToFloat(0x40933333u), 90, alphaPeak)',
              'ConfigureHealBand(prim->m_gi[1], 60.0f, 180, 5.0f, 90, alphaPeak)',
              'ConfigureHealBand(prim->m_gi[2], upperHeight',
              'ConfigureHealBand(prim->m_gi[3], upperHeight',
              'else if (m_stateCnt <= 50 && (m_stateCnt % 6) == 0 && variant < 2)'):
    if token not in local_heal2:
        raise SystemExit(f"local Heal2 variant-two profile changed at {token!r}")

for phrase in (
    "HealBlue effect253 plays its exact executable wave and emits the source variant-two blue heal ring",
    "HealBlue nonzero divisible-by-six frame advances variant two without repeating wave, ring, or particle",
):
    if phrase not in FIXTURE:
        raise SystemExit(f"HealBlue lifecycle assertion missing {phrase!r}")

print("effect253 PE HealBlue/Heal2, executable string bytes, primary-Ref divergence, local route and lifecycle evidence passed")
