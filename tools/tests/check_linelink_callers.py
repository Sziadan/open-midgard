"""Verify LineLink launcher/caller behavior against Ref and canonical PE."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage  # noqa: E402


def require(ok: bool, message: str) -> None:
    if not ok: raise SystemExit(message)


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
pe_onprocess = function(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
src = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
head = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8", errors="replace")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")

signatures = (
    "void __thiscall CRagEffect::LINELINK(CRagEffect *this)",
    "void __thiscall CRagEffect::LINELINK2(CRagEffect *this)",
    "void __thiscall CRagEffect::LINELINK3(CRagEffect *this)",
)
for sig in signatures:
    require(compact(function(ref, sig)) == compact(function(pe, sig)), f"Ref/PE helper mismatch: {sig}")

for effect in (232, 384, 395):
    require(f"case {effect}: return ProcessLineLinkEffects();" in dispatch, f"route {effect} missing")
    start = pe_onprocess.index(f"case {effect}:")
    end = pe_onprocess.index(f"case {effect + 1}:", start)
    branch = pe_onprocess[start:end]
    call = {232: "CRagEffect::LINELINK(this);", 384: "CRagEffect::LINELINK2(this);",
            395: "CRagEffect::LINELINK3(this);"}[effect]
    require(call in branch, f"PE caller {effect} missing helper call")
    if effect in (384, 395):
        require('PlayWave(aEffect_10,' in branch and branch.index('PlayWave(aEffect_10,') < branch.index(call),
                f"PE caller {effect} wave/call order mismatch")
require("LINELINK(this" not in ref_calls, "Ref caller omissions changed; review the mismatch record")

require("bool CRagEffect::ProcessLineLinkEffects()" in src and "SpawnLineLink(int variant)" in head,
        "local implementation/declaration missing")
spawn = function(src, "void CRagEffect::SpawnLineLink(int variant)")
for token in (
    "PP_LINELINK", '"effect\\\\cloud11.tga"', '"effect\\\\alpha_center.tga"',
    "gi.life = 1", "gi.process = 0", "variant == 0 ? 45 : 0", "gi.distance = 0.0f",
    "variant == 0 ? 0.40000001f : (variant == 1 ? 0.2f : 0.69999999f)",
    "gi.height[4] = static_cast<float>(variant)", "gi.height[0] = m_deltaPos.y",
    "gi.vecTNowX = m_effectPos.x", "gi.vecTPreX = target->m_pos.x", "gi.life = 0",
): require(token in spawn, f"GI setup anchor missing: {token}")
proc = function(src, "bool CRagEffect::ProcessLineLinkEffects()")
for token in ('"effect\\\\"', "m_stateCnt == 0 && (m_type == 384 || m_type == 395)",
              "SpawnLineLink(m_type == 232 ? 0 : (m_type == 384 ? 1 : 2))"):
    require(token in proc, f"caller behavior anchor missing: {token}")
require("rand()" not in spawn, "LineLink launcher unexpectedly consumes RNG")

image = ExeImage()
assets = {
    0x7194E0: b"effect\\cloud11.tga",
    0x7192FC: b"effect\\alpha_center.tga",
}
for address, expected in assets.items():
    value = image.read_cstring_at(address)
    require(value is not None and value.raw == expected, f"PE asset mismatch at {address:#x}")
require('aEffect_10[] = "effect\\\\";' in pe, "canonical effect\\ audio literal mismatch")

lifecycle = (ROOT / "tools/tests/EffectLifecycleTests.cpp").read_text(encoding="utf-8", errors="replace")
devotion_fixture = re.search(r"CRenderObject master,target,oldPlayer;(.*?)(?=\n    })", lifecycle, re.S)
require(devotion_fixture is not None and "effect.m_type=232" in devotion_fixture.group(1)
        and "effect.ProcessLineLinkEffects()" in devotion_fixture.group(1),
        "direct effect232/Devotion LineLink production lifecycle fixture is missing")
for token in (
    'g_texMgr.requests==std::vector<std::string>{"effect\\\\cloud11.tga"}',
    "prim->m_type==PP_LINELINK", "prim->m_renderFlag==5", "prim->m_duration==91",
    "gi.maxHeight==0.40000001f", "gi.height[4]==0", "gi.height[0]==42",
    "gi.vecTNowX==300", "gi.vecTPreX==100", "prim->ProcessPrimitiveFrame()",
    "prim->m_gi[0].vecTNowY==412", "prim->m_gi[0].vecTPreY==212", "waveCalls.empty()", "rngDraws==draws",
):
    require(token in devotion_fixture.group(1), f"Devotion LineLink runtime fixture missing: {token}")

print("LineLink parity: PE/Ref helpers, 232/384/395 routes, exact paths/targets/GI setup and effect232 production lifecycle passed")
