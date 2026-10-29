"""Source parity checks for RG_COIN and RG_COIN2 callers and GI setup."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
import sys
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage


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
lifecycle = (ROOT / "tools/tests/EffectLifecycleTests.cpp").read_text(encoding="utf-8")

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

pe_onprocess3 = function(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
ref_onprocess3 = function(ref_calls, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
pe_627 = re.search(r"case 0x273:(.*?)(?=\n\s*case |\n\s*default:|\Z)", pe_onprocess3, re.S)
ref_627 = re.search(r"case 0x273:(.*?)(?=\n\s*case |\n\s*default:|\Z)", ref_onprocess3, re.S)
require(pe_627 is not None and ref_627 is not None and compact(pe_627.group(1)) == compact(ref_627.group(1)),
        "PE/primary-Ref effect627 caller branches diverged")
for token in (
    "this->m_stateCnt == 21", "this->m_pos = this->m_master->m_pos",
    "PlayWave(aEffect_38, xm, 0.0, zm, 250, 40, 1.0)",
    "RG_COIN2(this, &byte_719AF4, 5.0)", "RG_COIN2(this, &byte_719AD0, 5.0)",
    "RG_COIN2(this, &byte_719AAC, 5.0)", "RG_COIN2(this, &byte_719A8C, 5.0)",
): require(token in pe_627.group(1), f"effect627 caller branch missing {token}")
require("case 627: ApplyRgCoin3(); return true;" in dispatch3,
        "effect627 local OnProcess3 route missing")
local_627 = function(src, "void CRagEffect::ApplyRgCoin3()")
for token in (
    "if (m_stateCnt == 21)", "m_effectPos = master->m_pos",
    'PlayWave("effect\\\\disarm.wav"',
    'SpawnRgCoin2("UserInterface\\\\item\\\\gold_lux.bmp", 5.0f)',
    'SpawnRgCoin2("UserInterface\\\\item\\\\deogaeriseun.bmp", 5.0f)',
    'SpawnRgCoin2("UserInterface\\\\item\\\\double_shotgun.bmp", 5.0f)',
    'SpawnRgCoin2("UserInterface\\\\item\\\\deoseukeu.bmp", 5.0f)',
): require(token in local_627, f"local effect627 mapping missing {token}")

pe_image = ExeImage(ROOT / "HighPriest.exe")
for address, expected in (
    (0x719AF4, bytes.fromhex("C0 AF C0 FA C0 CE C5 CD C6 E4 C0 CC BD BA 5C 69 74 65 6D 5C B0 F1 B5 E5 B7 E8 BD BA 2E 62 6D 70")),
    (0x719AD0, bytes.fromhex("C0 AF C0 FA C0 CE C5 CD C6 E4 C0 CC BD BA 5C 69 74 65 6D 5C B4 F5 B0 B3 B8 AE BD BC 2E 62 6D 70")),
    (0x719AAC, bytes.fromhex("C0 AF C0 FA C0 CE C5 CD C6 E4 C0 CC BD BA 5C 69 74 65 6D 5C B4 F5 BA ED BC A6 B0 C7 2E 62 6D 70")),
    (0x719A8C, bytes.fromhex("C0 AF C0 FA C0 CE C5 CD C6 E4 C0 CC BD BA 5C 69 74 65 6D 5C B4 F5 BD BA C5 A9 2E 62 6D 70")),
):
    value = pe_image.read_cstring_at(address)
    require(value is not None and value.raw == expected, f"effect627 bitmap bytes changed at {address:#x}")
wave_prefix = pe_image.read_cstring_at(0x6F1FF0)
require(wave_prefix is not None and wave_prefix.raw == b"effect\\",
        "effect627 PE wave symbol prefix changed")

fixture = re.search(r"effect\.ApplyRgCoin3\(\);(.*?)(?=\n    \})", lifecycle, re.S)
require(fixture is not None and "gold_lux.bmp" in fixture.group(1)
        and "m_stateCnt=21" in fixture.group(1) and "effect\\\\disarm.wav" in fixture.group(1),
        "effect627 lifecycle assertions disappeared")
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
    "PP_INTIMIDATE", "m_effectPos.y -= 12.0f", "lane < 4",
    "gi.process = -18 - rand() % 30", "gi.riseAngle = static_cast<short>(rand() % 360)",
    "gi.fullDisplayAngle = static_cast<short>(rand() % 360)",
    "gi.rotStart = static_cast<short>(rand() % 360)", "const float radius = static_cast<float>(rand() % 360)",
    "lane + 1 < 4", "shifted.height[1] = radius", "shifted.height[0] = size",
    "shifted.fullDisplayAngle = 0", "shifted.flag[0] = colorMode",
): require(token in spawn, f"emitter behavior anchor missing: {token}")

for address, expected in (
    (0x714280, b"effect\\rog_steal coin.wav"),
    (0x71999C, b"effect\\coin_a.bmp"),
    (0x719688, b"effect\\shield.bmp"),
    (0x719674, b"effect\\sword.bmp"),
):
    value = ExeImage().read_cstring_at(address)
    require(value is not None and value.raw == expected, f"PE asset mismatch at {address:#x}")

for effect, table in ((274, dispatch), (495, dispatch3)):
    require(re.search(rf"case {effect}: return ProcessRgCoinEffects\(\);", table), f"ordered route {effect} missing")

print("RG_COIN caller parity: helper bodies, 274/495/627 routes, exact assets, state21 cue and lane order passed")
