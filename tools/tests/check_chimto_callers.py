#!/usr/bin/env python3
"""Compare CHIMTO helper and callers with the canonical executable sources."""
from __future__ import annotations

import re
from pathlib import Path

import sys
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage  # noqa: E402


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(message)


def function(source: str, signature: str) -> str:
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 0
    for cursor in range(opening, len(source)):
        if source[cursor] == "{":
            depth += 1
        elif source[cursor] == "}":
            depth -= 1
            if depth == 0:
                return source[start:cursor + 1]
    raise AssertionError(f"unterminated function {signature}")


def compact(source: str) -> str:
    return re.sub(r"\s+", "", source)


exe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
ref2 = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
ref = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
local = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8", errors="replace")
onprocess = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
onprocess3 = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")

signature = "void __thiscall CRagEffect::CHIMTO(CRagEffect *this, char F1)"
require(compact(function(exe, signature)) == compact(function(ref2, signature)),
        "Ref and canonical CHIMTO helper differ")
method = function(local, "bool CRagEffect::ProcessChimtoEffect(int variant)")
require("ProcessChimtoEffect" in header, "CHIMTO declaration missing")
require("case 267: return ProcessChimtoEffect(0);" in onprocess, "effect 267 route missing")
require("case 606: return ProcessChimtoEffect(2);" in onprocess3, "effect 606 route missing")
require("case 607: return ProcessChimtoEffect(1);" in onprocess3, "effect 607 route missing")

main = exe.index("unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)\n{")
case267 = exe[exe.index("          case 267:", main):exe.index("          case 269:", main)]
require("CRagEffect::CHIMTO(this, 0);" in case267, "effect 267 variant changed")
main3 = exe.index("unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)\n{")
case606 = exe[exe.index("    case 0x25E:", main3):exe.index("    case 0x260:", main3)]
require("CRagEffect::CHIMTO(this, 2);" in case606 and "CRagEffect::CHIMTO(this, 1);" in case606,
        "OnProcess3 Chimto variants changed")
require("PlayWave(aEffectMon_0, x, 0.0, z, 250, 40, 1.0)" in function(exe, signature),
        "variant-zero sound call mismatch")
helper = function(exe, signature)
require("for ( i = 180; i < 720; *((_BYTE *)&g_prim->m_latiSpeed + i) = 0 )" in helper,
        "canonical inactive alphaT byte stores changed")
require("PlayWave" not in function(exe, signature).split("if ( !F1 )", 1)[-1].split("memset", 1)[-1],
        "unexpected variant-specific sound")
gi_struct = function(header, "struct EffectGi")
require("short alphaT = 0;" in gi_struct and "std::array<EffectGi, 4> m_gi;" in header,
        "local GI alphaT initialization no longer makes the aliased zero stores idempotent")

expected_assets = {
    0x71CB90: b"\xC0\xCC\xC6\xD1\xC6\xAE\\\xC4\xA7\xC5\xF5\xB0\xE6_.spr",
    0x71CB7C: b"\xC0\xCC\xC6\xD1\xC6\xAE\\\xC4\xA7\xC5\xF5\xB0\xE6_.act",
    0x71CB6C: b"\xC0\xCC\xC6\xD1\xC6\xAE\\wink.spr",
    0x71CB5C: b"\xC0\xCC\xC6\xD1\xC6\xAE\\wink.act",
    0x71CB48: b"\xC0\xCC\xC6\xD1\xC6\xAE\\fvoice.spr",
    0x71CB34: b"\xC0\xCC\xC6\xD1\xC6\xAE\\fvoice.act",
    0x71CBA4: b"effect\\mon_\xC4\xA7\xC5\xF5\xB0\xE6.wav",
}
image = ExeImage()
for address, expected in expected_assets.items():
    value = image.read_cstring_at(address)
    require(value is not None and value.raw == expected, f"PE resource at {address:#x} mismatch")

tokens = (
    "if (m_stateCnt != 0) return true", "TryPlaySingleEffectWave(m_effectPos, \"effect\\\\mon_\\xC4\\xA7\\xC5\\xF5\\xB0\\xE6.wav\"",
    "LaunchEffectPrim(PP_CHIMTO", "prim->m_duration = m_duration", "prim->m_gi[0].life = 1",
    "prim->m_gi[0].maxHeight = 0.0f", "prim->m_gi[0].vecBPreX = m_deltaPos.x",
    "prim->m_gi[0].arcBaselineY = m_deltaPos.y", "prim->m_gi[0].vecBPreZ = m_deltaPos.z",
    "prim->m_alpha = 200.0f", "SetParticleResources(", "prim->m_animSpeed = 2",
    "prim->m_size = 2.5f", "prim->m_repeatAnim = false", "prim->m_pattern |= 0x200",
    "g_cosTable[angle]", "prim->m_renderFlag |= 0x200", "prim->m_spriteAction = 0",
    "InitReferenceSegments(prim)",
)
cursor = 0
for token in tokens:
    pos = method.find(token, cursor)
    require(pos >= 0, f"local CHIMTO operation missing or reordered: {token}")
    cursor = pos + len(token)
for expected in ("\\xC0\\xCC\\xC6\\xD1\\xC6\\xAE", "\\xC4\\xA7\\xC5\\xF5\\xB0\\xE6_.spr",
                 "\\wink.spr", "\\fvoice.spr", "prim->m_size = 1.0f"):
    require(expected in method, f"local CHIMTO asset/variant setup missing {expected}")
print("CHIMTO helper, caller, resources, variants and routes checks passed")
