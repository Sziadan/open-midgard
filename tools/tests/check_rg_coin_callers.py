"""Source parity checks for RG_COIN and RG_COIN2 callers and GI setup."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def require(ok: bool, message: str) -> None:
    if not ok:
        raise SystemExit(message)


def function(source: str, signature: str) -> str:
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{": depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0: return source[start:pos + 1]
    raise AssertionError(signature)


def compact(text: str) -> str:
    return re.sub(r"\s+", "", text)


ref = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
ref_calls = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
pe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
src = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
head = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8", errors="replace")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
dispatch3 = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")

for sig in (
    "void __thiscall CRagEffect::RG_COIN(CRagEffect *this, char *tName)",
    "void __thiscall CRagEffect::RG_COIN2(CRagEffect *this, char *tName, float size)",
):
    require(compact(function(ref, sig)) == compact(function(pe, sig)), f"Ref/PE helper mismatch: {sig}")

require("bool CRagEffect::ProcessRgCoinEffects()" in src and "SpawnRgCoin(const char*" in head,
        "RG_COIN implementation/declaration missing")
require("case 274: return ProcessRgCoinEffects();" in dispatch, "ID 274 route missing")
require("case 495: return ProcessRgCoinEffects();" in dispatch3, "ID 495 route missing")
require("CRagEffect::RG_COIN(this, aEffectCoinABmp);" in pe
        and "v162 = 30;" in pe and "PlayWave(aEffectRogSteal, xde, 0.0, zde, 250, 40, 1.0);" in pe,
        "canonical ID 274 caller branch mismatch")
require("CRagEffect::RG_COIN2(this, aEffectShieldBm, 5.0);" in pe
        and "CRagEffect::RG_COIN2(this, aEffectSwordBmp, 9.0);" in pe
        and "aEffect_70[] = \"effect\\\\\";" in pe,
        "canonical ID 495 caller branch or exact sound prefix mismatch")
body = function(src, "bool CRagEffect::ProcessRgCoinEffects()")
for token in (
    "call < 30", '"effect\\\\coin_a.bmp", 3.5f, 1',
    '"effect\\\\shield.bmp", 5.0f, 2', '"effect\\\\sword.bmp", 9.0f, 2',
    '"effect\\\\", m_effectPos.x - listener.x',
    "m_stateCnt == 1 || m_stateCnt == 11 || m_stateCnt == 21 || m_stateCnt == 31",
    "repeat < 4", "return true;",
): require(token in body, f"caller behavior anchor missing: {token}")

spawn = function(src, "void CRagEffect::SpawnRgCoin(const char* textureName, float size, u8 colorMode)")
for token in (
    "PP_INTIMIDATE", "m_effectPos.y -= 12.0f", "lane < prim->m_gi.size()",
    "gi.process = -18 - rand() % 30", "gi.riseAngle = static_cast<short>(rand() % 360)",
    "gi.fullDisplayAngle = static_cast<short>(rand() % 360)",
    "gi.rotStart = static_cast<short>(rand() % 360)", "const float radius = static_cast<float>(rand() % 360)",
    "lane + 1 < prim->m_gi.size()", "shifted.height[1] = radius", "shifted.height[0] = size",
    "shifted.fullDisplayAngle = 0", "shifted.flag[0] = colorMode",
): require(token in spawn, f"emitter behavior anchor missing: {token}")

for address, expected in (
    (0x714280, b"effect\\rog_steal coin.wav"),
    (0x71999C, b"effect\\coin_a.bmp"),
    (0x719688, b"effect\\shield.bmp"),
    (0x719674, b"effect\\sword.bmp"),
):
    from sys import path
    path.insert(0, str(ROOT / "tools"))
    from ro_exe_resolve import ExeImage
    value = ExeImage().read_cstring_at(address)
    require(value is not None and value.raw == expected, f"PE asset mismatch at {address:#x}")

for effect, table in ((274, dispatch), (495, dispatch3)):
    require(re.search(rf"case {effect}: return ProcessRgCoinEffects\(\);", table), f"ordered route {effect} missing")

print("RG_COIN caller parity: helper bodies, 274/495 routes, assets, five-draw lane order, bounded shifted stores passed")
