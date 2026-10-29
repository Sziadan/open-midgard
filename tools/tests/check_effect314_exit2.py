#!/usr/bin/env python3
"""Compare effect314 Exit2, its PE/Ref caller, and local origin lifecycle."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage, SymbolTable  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
REF2 = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="latin-1")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
DISPATCH2 = (ROOT / "src/world/RagEffectOnProcess2Cases.generated.inc").read_text(encoding="utf-8")
TEST = (ROOT / "tools/tests/EffectLifecycleTests.cpp").read_text(encoding="utf-8")


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


def case(source: str) -> str:
    body = function(source, "unsigned __int8 __thiscall CRagEffect::OnProcess2(CRagEffect *this)")
    match = re.search(r"case 0x13A:(.*?)(?=\n\s*case |\n\s*default:|\Z)", body, re.S)
    require(match is not None, "OnProcess2 effect314 caller case missing")
    return match.group(1)


pe_case, ref_case = case(PE), case(REF)
require(re.sub(r"\s+", "", pe_case) == re.sub(r"\s+", "", ref_case)
        and "CRagEffect::Exit2(this, aEffectRingPurp);" in pe_case,
        "PE/primary-Ref effect314 caller mapping changed")
pe_helper = function(PE, "void __thiscall CRagEffect::Exit2(CRagEffect *this, char *tName)")
ref_helper = function(REF2, "void __thiscall CRagEffect::Exit2(CRagEffect *this, char *tName)")
require(re.sub(r"\s+", "", pe_helper) == re.sub(r"\s+", "", ref_helper),
        "PE/secondary-Ref Exit2 helper bodies diverged")
for token in ("this->m_pos = this->m_master->m_pos", "if ( !this->m_stateCnt )",
              "PlayWave(aEffectEfTelepo, x, 0.0, z, 250, 40, 1.0)", "PP_HEAL",
              "m_GI[3].max_height = 20.0", "m_GI[2].max_height = 45.0",
              "m_GI[1].max_height = 70.0", "m_GI[0].max_height = 180.0"):
    require(token in pe_helper, f"canonical Exit2 helper missing {token}")

require("case 314: ApplyExit2(\"effect\\\\ring_purple.tga\"); return true;" in DISPATCH2,
        "local effect314 route changed")
local = function(LOCAL, "void CRagEffect::ApplyExit2(const char* textureName)")
require(local.index("m_effectPos = origin") < local.index("m_stateCnt != 0"),
        "local Exit2 origin refresh must precede state-zero gate")
for token in ("m_cachedPos = origin", "vector3d wavePos = origin", "wavePos.y = mode->m_world->m_player->m_pos.y",
              'TryPlayEffectWaveAt(wavePos, "effect\\\\EF_Teleportation.wav")',
              "kPrimTypeHeal", "prim->m_renderFlag = 5", "prim->m_duration = m_duration",
              "ConfigureHealBand(prim->m_gi[3], 20.0f", "ConfigureHealBand(prim->m_gi[0], 180.0f"):
    require(token in local, f"local Exit2 mapping missing {token}")

symbols = SymbolTable(ExeImage())
for name, expected in (("aEffectRingPurp", b"effect\\ring_purple.tga"),
                       ("aEffectEfTelepo", b"effect\\EF_Teleportation.wav")):
    resolved = symbols.resolve_name(name)
    require(resolved is not None and resolved.raw == expected, f"PE Exit2 resource bytes changed: {name}")
for phrase in ("effect314 Exit2 plays one state-zero teleportation wave", "effect314 Exit2 preserves the four source GI",
               "effect314 Exit2 refreshes moved master after spawn gate"):
    require(phrase in TEST, f"effect314 lifecycle coverage disappeared: {phrase}")
print("effect314 Exit2 parity passed: caller/helper bodies, exact resources, four GI bands and origin refresh")
