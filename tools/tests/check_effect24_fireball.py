#!/usr/bin/env python3
"""Compare effect24 FireBall's PE/Ref emitter and local implementation."""
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


sig = "void __thiscall CRagEffect::FireBall(CRagEffect *this)"
pe_helper, ref_helper = function(PE, sig), function(REF, sig)
require(re.sub(r"\s+", "", pe_helper) == re.sub(r"\s+", "", ref_helper),
        "PE/primary-Ref FireBall helper bodies diverged")
for token in (
    "m_stateCnt == 20 || m_stateCnt == 24 || m_stateCnt == 28 || m_stateCnt == 32",
    "PlayWave(aEffectEfFireba, x, 0.0, z, 250, 40, 1.0)",
    "PP_3DPARTICLE", "g_prim->m_duration = 20", "g_prim->m_pattern |= 0x120u",
    "v4->m_deltaPos2.y = -15.0", "CalcDir(g_prim, this->m_deltaPos.x, this->m_deltaPos.z)",
    "CalcDist(v5, this->m_deltaPos.x, this->m_deltaPos.z)",
    "g_prim->m_red = 246", "g_prim->m_green = 199", "g_prim->m_blue = 76",
    "&byte_71B5A0", "&byte_71B58C",
):
    require(token in pe_helper, f"canonical FireBall helper missing {token}")

symbols = SymbolTable(ExeImage())
for name, value in (("aEffectEfFireba", b"effect\\EF_FireBall.wav"),):
    resolved = symbols.resolve_name(name)
    require(resolved is not None and resolved.raw == value, f"PE wave bytes changed: {name}")
image = ExeImage()
for address, expected in ((0x71B5A0, b"\xC0\xCC\xC6\xD1\xC6\xAE\\fireball.spr"),
                          (0x71B58C, b"\xC0\xCC\xC6\xD1\xC6\xAE\\fireball.act")):
    resolved = image.read_cstring_at(address)
    require(resolved is not None and resolved.raw == expected, f"PE FireBall resource bytes changed at {address:#x}")

require(re.search(r"case 24:\s*SpawnFireBall\(\);\s*return true;", DISPATCH),
        "effect24 local dispatcher route changed")
local = function(LOCAL, "void CRagEffect::SpawnFireBall()")
for token in (
    "m_stateCnt != 20 && m_stateCnt != 24 && m_stateCnt != 28 && m_stateCnt != 32",
    'PlayWave("effect\\\\EF_FireBall.wav"', "m_effectPos.x - listener.x",
    'LaunchEffectPrim(PP_3DPARTICLE, vector3d{})', "prim->m_duration = 20",
    "prim->m_pattern |= 0x120", "prim->m_deltaPos2 = {0.0f,-15.0f,0.0f}",
    "static_cast<double>(m_deltaPos.x) - prim->m_pos.x",
    "static_cast<double>(m_deltaPos.z) - prim->m_pos.z",
    "prim->m_tintColor = RGB(246,199,76)",
    '"\\xC0\\xCC\\xC6\\xD1\\xC6\\xAE\\\\fireball.spr"',
    '"\\xC0\\xCC\\xC6\\xD1\\xC6\\xAE\\\\fireball.act"',
):
    require(token in local, f"local FireBall emitter missing {token}")
fixture = re.search(r"effect\.SpawnFireBall\(\);(.*?)(?=\n    \})", TEST, re.S)
require(fixture is not None and all(token in fixture.group(1) for token in (
    "FireBall emits nothing before frame twenty", "FireBall emits exactly four scheduled waves",
    "FireBall computes heading and distance", "FireBall cleanup releases sprite/action locks")),
    "effect24 lifecycle fixture assertions disappeared")
print("effect24 FireBall parity passed: PE/Ref helper, schedule, wave and assets, local route and lifecycle")
