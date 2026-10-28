"""Compare BeginAsura/BeginAsura2 helpers, direct callers, assets and routes."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import ro_exe_resolve as resolver


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


pe = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
ref = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="latin-1")
ref_callers = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
local = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
onprocess = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
onprocess2 = (ROOT / "src/world/RagEffectOnProcess2Cases.generated.inc").read_text(encoding="utf-8")
onprocess3 = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")

signatures = (
    "void __thiscall CRagEffect::BeginAsura(CRagEffect *this, char F1)",
    "void __thiscall CRagEffect::BeginAsura2(CRagEffect *this, char F1)",
    "void __thiscall CRagEffect::SoulLinkLight(CRagEffect *this, char *tName)",
)
for signature in signatures:
    if compact(function(pe, signature)) != compact(function(ref, signature)):
        raise SystemExit(f"Ref/PE helper mismatch: {signature}")

pe_process2 = function(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess2(CRagEffect *this)")
branch_328 = pe_process2[pe_process2.index("case 0x148:"):pe_process2.index("case 0x149:")]
if "BeginAsura(this, 0)" not in branch_328:
    raise SystemExit("canonical 328 caller must use BeginAsura variant0")
pe_process3 = function(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
branch_510 = pe_process3[pe_process3.index("case 0x1FE:"):pe_process3.index("case 0x200:")]
if "BeginAsura(this, 1)" not in branch_510:
    raise SystemExit("canonical 510 caller must use BeginAsura variant1")
branch_503 = pe_process3[pe_process3.index("case 0x1F7:"):pe_process3.index("case 0x1F8:")]
for token in ("PlayWave(aEffectT_11", "this->m_stateCnt == 145", "PlayWave(aEffectT_15",
              "SoulLinkLight(this, aEffectWhitelig)", "BeginAsura2(this, 0)"):
    if token not in branch_503:
        raise SystemExit(f"canonical 503 branch missing {token}")
ref_branch_503 = ref_callers[ref_callers.index("case 0x1F7:"):ref_callers.index("case 0x1F8:")]
if compact(branch_503) != compact(ref_branch_503):
    raise SystemExit("Ref/PE 503 caller branch mismatch")
ref_branch_510 = ref_callers[ref_callers.index("case 0x1FE:"):ref_callers.index("case 0x200:")]
if compact(branch_510) != compact(ref_branch_510):
    raise SystemExit("Ref/PE 510 caller branch mismatch")

if "case 328: return ProcessBeginAsuraEffect();" not in onprocess2:
    raise SystemExit("effect328 route missing")
if "case 510: return ProcessBeginAsuraEffect();" not in onprocess3:
    raise SystemExit("effect510 route missing")
if "case 503: return ProcessBeginAsura2Effect();" not in onprocess3:
    raise SystemExit("effect503 route missing")
for declaration in ("bool ProcessBeginAsuraEffect();", "bool ProcessBeginAsura2Effect();",
                    "void SpawnBeginAsura(int variant, bool secondBurst);",
                    "void SpawnSoulLinkLight(const char* textureName);", "void SpawnBeginAsura2(bool secondBurst);"):
    if declaration not in header:
        raise SystemExit(f"local declaration missing: {declaration}")

image = resolver.ExeImage()
raw_at = {
    call_va: resolver._nearest_name_push(image, image.offset_of(call_va)).raw
    for call_va in (0x5E9B6F, 0x5E9BAC)
}
expected = {
    0x5E9B6F: b"effect\\T_\xC4\xB3\xBD\xBA\xC6\xC3.wav",
    0x5E9BAC: b"effect\\T_\xBF\xB5\xC8\xA5.wav",
}
if raw_at != expected:
    raise SystemExit(f"effect503 PlayWave argument bytes changed: {raw_at!r}")

primary = function(local, "void CRagEffect::SpawnBeginAsura(")
for token in ("PP_ASURACASTING", "firstTextures", "secondTextures", "m_renderFlag |= 1u",
              "m_renderFlag |= 5u", "lane < 3", "-40 + 20 * lane", "-40 - 20 * lane",
              "gi.flag[0]", "gi.flag[4]", "24.0f", "18.0f"):
    if token not in primary: raise SystemExit(f"BeginAsura emitter anchor missing: {token}")
soul_link = function(local, "void CRagEffect::SpawnSoulLinkLight(")
for token in ("PP_SOULLINK", "gi.height[10] = m_deltaPos.y",
              "gi.height[13]", "gi.height[14]", "gi.height[15]", "rand() % 360",
              "gi.vecBPreX = -30.0f * 0.0f", "gi.arcBaselineY = -40.0f"):
    if token not in soul_link: raise SystemExit(f"SoulLinkLight emitter anchor missing: {token}")
if soul_link.count("rand() % 360") != 4:
    raise SystemExit("SoulLinkLight must consume four ordered angle draws")
asura2 = function(local, "void CRagEffect::SpawnBeginAsura2(")
for token in ("PP_ASURACASTING", "firstTextures", "secondTextures", "laneCount = secondBurst ? 5 : 3",
              "-60 + 20 * lane", "-120 + 20 * lane", "22 - 6 * lane", "-4 - 6 * lane",
              "gi.distance = 5.0f", "gi.flag[4] = 2"):
    if token not in asura2: raise SystemExit(f"BeginAsura2 emitter anchor missing: {token}")
if "UpdateReferenceAsuraCasting(this)" not in local:
    raise SystemExit("existing PP_ASURACASTING updater missing")
if "ApplySaintCasting(45, \"effect\\\\ring_white.tga\", 2)" not in function(local, "bool CRagEffect::ProcessBeginAsuraEffect()"):
    raise SystemExit("effect328/510 source saint-casting calls missing")
if 'SpawnSoulLinkLight("effect\\\\whitelight.tga")' not in function(local, "bool CRagEffect::ProcessBeginAsura2Effect()"):
    raise SystemExit("effect503 SoulLinkLight resource call missing")
print("BeginAsura parity: Ref/PE helpers and callers, 328/503/510 routes, PE sound bytes, texture variants and GI payload anchors passed")
