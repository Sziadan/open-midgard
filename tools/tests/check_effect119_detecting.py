#!/usr/bin/env python3
"""Compare effect119 Detecting helper, PE-only caller and local lifecycle route."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage, SymbolTable  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
TEST = (ROOT / "tools/tests/EffectLifecycleTests.cpp").read_text(encoding="utf-8", errors="replace")


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


sig = "void __thiscall CRagEffect::Detecting(CRagEffect *this)"
pe_helper, ref_helper = function(PE, sig), function(REF, sig)
require(re.sub(r"\s+", "", pe_helper) == re.sub(r"\s+", "", ref_helper),
        "PE/primary-Ref Detecting helper bodies diverged")
for token in ("if ( !this->m_stateCnt )", "PP_3DTEXTURE", "m_duration = 57",
              "m_widthSpeed = 1.5", "m_heightSpeed = 1.5", "m_widthAccel", "m_heightAccel",
              "m_deltaPos2.y = -5.0", "aEffectFashasha"):
    require(token in pe_helper, f"canonical Detecting helper missing {token}")
require("case 119:" in PE and "CRagEffect::Detecting(this);" in PE,
        "PE effect119 case caller disappeared")
require("CRagEffect::Detecting(this);" not in REF and "case 119:" not in REF,
        "primary Ref should remain documented as lacking the effect119 caller")
require("case 119: SpawnDetecting(); return true;" in DISPATCH,
        "local effect119 dispatcher route changed")
local = function(LOCAL, "void CRagEffect::SpawnDetecting()")
for token in ('if (m_stateCnt != 0)', 'PlayWave("effect\\\\hunter_detecting.wav"',
              "PP_3DTEXTURE", "prim->m_duration = 57", '"effect\\\\fashasha.tga"'):
    require(token in local, f"local Detecting emitter missing {token}")
symbols = SymbolTable(ExeImage())
for name, expected in (("aEffectFashasha", b"effect\\fashasha.tga"),):
    value = symbols.resolve_name(name)
    require(value is not None and value.raw == expected, f"PE resource bytes changed: {name}")
wave = symbols.resolve_name("aEffectHunterDe")
require(wave is not None and wave.raw == b"effect\\hunter_detecting.wav", "PE Detecting wave bytes changed")
fixture = re.search(r"effect\.SpawnDetecting\(\);(.*?)(?=\n    \})", TEST, re.S)
require(fixture is not None and all(token in fixture.group(1) for token in (
    "hunter_detecting.wav", "fashasha.tga", "PP_3DTEXTURE", "m_stateCnt=1")),
    "effect119 lifecycle fixture assertions disappeared")
print("effect119 Detecting parity passed: PE/Ref helper, PE-only caller, local route and lifecycle resources")
