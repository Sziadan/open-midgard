#!/usr/bin/env python3
"""Compare Sight effects22/601 original helpers and local source/lifecycle routes."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
REF_DHXJ = (ROOT / "Ref/dhxj/RagEffect.cpp").read_text(encoding="latin-1")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
ROUTE2 = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
ROUTE = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")
LIFECYCLE = (ROOT / "tools/tests/EffectLifecycleTests.cpp").read_text(encoding="utf-8")


def function_body(source, signature):
    starts = [match.start() for match in re.finditer(re.escape(signature), source)]
    for start in reversed(starts):
        opening = source.find("{", start + len(signature))
        if opening < 0 or source.find(";", start + len(signature), opening) >= 0:
            continue
        depth = 0
        for pos in range(opening, len(source)):
            if source[pos] == "{":
                depth += 1
            elif source[pos] == "}":
                depth -= 1
                if depth == 0:
                    return source[opening + 1:pos]
    raise AssertionError(f"function body not found: {signature}")


def compact(source):
    return re.sub(r"\s+", "", source)


signature = "void __thiscall CRagEffect::Sight2(CRagEffect *this)"
pe_helper = function_body(PE, signature)
ref_helper = function_body(REF, signature)
if compact(pe_helper) != compact(ref_helper):
    raise AssertionError("PE and primary-Ref Sight2 helper bodies differ")
for token in (
    "this->m_pos = this->m_master->m_pos", "ProjectVertex(g_renderer", "this->m_tlvertX",
    "this->m_tlvertY", "this->m_stateCnt", "GetSin(v7)", "GetCos(v7)", "GetRadian(v7)",
    "PP_3DPARTICLE", "m_duration = 20", "m_deltaPos2.y = -20.0", "sn * 15.0",
    "Cos * -15.0", "m_deltaPosAccel.y = 0.1", "m_animSpeed = 3", "m_size = 2.5",
    "m_sizeSpeed = -0.1", "m_alpha = 50.0", "m_alphaSpeed = -3.0",
    "SetSprName(g_prim, &byte_71B578)", "SetActName(g_prim, &byte_71B564)",
    "CEffectPrim::InitSegment(g_prim)",
):
    if token not in pe_helper:
        raise AssertionError(f"PE Sight2 helper missing {token}")
if "Shadow" in pe_helper or "rand(" in pe_helper or "PlayWave(" in pe_helper:
    raise AssertionError("Sight2 unexpectedly gained shadow, RNG, or sound behavior")

sight_signature = "void __thiscall CRagEffect::Sight(CRagEffect *this)"
pe_sight = function_body(PE, sight_signature)
ref_sight = function_body(REF, sight_signature)
if compact(pe_sight) != compact(ref_sight):
    raise AssertionError("PE and primary-Ref Sight helper bodies differ")
for token in (
    "this->m_pos = this->m_master->m_pos", "ProjectVertex(g_renderer", "this->m_tlvertX",
    "this->m_tlvertY", "this->m_stateCnt", "PP_3DPARTICLE", "aShadowSpr_0", "aShadowAct_0",
    "m_duration = 20", "m_alpha = 150.0", "m_alphaSpeed = -3.0", "m_alpha = 120.0",
):
    if token not in pe_sight:
        raise AssertionError(f"PE Sight helper missing {token}")

pe_onprocess = function_body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
if not re.search(r"case 22:\s*CRagEffect::Sight\(this\);\s*break;", pe_onprocess):
    raise AssertionError("canonical PE effect22 Sight caller changed")
if "CRagEffect::Sight(this)" in REF:
    raise AssertionError("primary Ref effect22 caller omission should remain documented")
if not re.search(r"case\s+EF_SIGHT\s*:.*?Sight\(\)\s*;\s*break\s*;", REF_DHXJ, re.S):
    raise AssertionError("secondary Ref EF_SIGHT caller changed")
if "case 22: SpawnSight(); return true;" not in ROUTE2:
    raise AssertionError("local generated effect22 route changed")
spawn_sight = function_body(LOCAL, "void CRagEffect::SpawnSight()")
if "ApplySight(150.0f, true);" not in spawn_sight:
    raise AssertionError("local effect22 must use the Sight profile with Shadow")

for source_name, source in (("PE", PE), ("primary Ref", REF)):
    body = function_body(source, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
    if not re.search(r"case 0x259:\s*CRagEffect::Sight2\(this\);\s*return 1;", body):
        raise AssertionError(f"{source_name} effect601 OnProcess3 caller changed")

if "case 601: SpawnSight2(); return true;" not in ROUTE:
    raise AssertionError("local generated effect601 route changed")
spawn = function_body(LOCAL, "void CRagEffect::SpawnSight2()")
if "ApplySight(50.0f, false);" not in spawn:
    raise AssertionError("local Sight2 mapping changed")
sight = function_body(LOCAL, "void CRagEffect::ApplySight(float initialAlpha, bool includeShadow)")
for token in (
    "m_effectPos = master->m_pos", "ProjectReferenceEffectVertex(master->m_pos",
    "m_tlvertX = projected.x", "m_tlvertY = projected.y", "if (m_stateCnt % 2 != 0) return",
    "GetSin(angle)", "GetCos(angle)", "PP_3DPARTICLE", "prim->m_duration = 20",
    "prim->m_deltaPos2 = {offsetX,-20.0f,offsetZ}", "prim->m_deltaPosAccel.y = 0.1f",
    "prim->m_animSpeed = 3", "prim->m_size = 2.5f", "prim->m_alpha = initialAlpha",
    "prim->m_alphaSpeed = -3.0f", '"\\xC0\\xCC\\xC6\\xD1\\xC6\\xAE\\\\Sight.spr"',
    '"\\xC0\\xCC\\xC6\\xD1\\xC6\\xAE\\\\Sight.act"', "if (includeShadow)",
):
    if token not in sight:
        raise AssertionError(f"local Sight/Sight2 implementation missing {token}")
if "ApplySight(50.0f, false)" not in spawn or "ApplySight(150.0f, true)" not in LOCAL:
    raise AssertionError("Sight2 must share the emitter with Sight and suppress only its shadow")

image = ExeImage(ROOT / "HighPriest.exe")
for va, expected, label in (
    (0x71B578, b"\xC0\xCC\xC6\xD1\xC6\xAE\\Sight.spr", "Sight SPR"),
    (0x71B564, b"\xC0\xCC\xC6\xD1\xC6\xAE\\Sight.act", "Sight ACT"),
    (0x71B558, b"Shadow.spr", "Shadow SPR"),
    (0x71B54C, b"Shadow.act", "Shadow ACT"),
):
    resource = image.read_cstring_at(va)
    if resource is None or resource.raw != expected:
        raise AssertionError(f"{label} resource differs at PE VA 0x{va:06X}")

fixture = re.search(r"CRagEffect effect;effect\.m_master=&actor;effect\.m_type=601;(.*?)(?=\n    \})", LIFECYCLE, re.S)
if fixture is None:
    raise AssertionError("effect601 lifecycle fixture disappeared")
for token in (
    "SpawnSight2()", "m_primList.size()==1", "m_alpha==50", "m_alphaSpeed==-3",
    "m_deltaPosAccel.y==0.1f", "m_animSpeed==3", "m_repeatAnim", "m_stateCnt=1",
    "m_stateCnt=9998", "m_stateCnt=9999", "m_primList.size()==2",
):
    if token not in fixture.group(1):
        raise AssertionError(f"effect601 lifecycle coverage missing {token}")

sight_fixture = re.search(r"CRagEffect effect;effect\.m_master=&actor;effect\.m_type=22;(.*?)(?=\n    \})", LIFECYCLE, re.S)
if sight_fixture is None:
    raise AssertionError("effect22 Sight lifecycle fixture disappeared")
for token in (
    "SpawnSight()", "effect.m_stateCnt=1", "effect.m_stateCnt=2", "effect.m_primList.size()==2",
    "Shadow.spr", "Shadow.act", "m_alpha==150", "m_alpha==120", "m_alphaSpeed==-1",
    "effect.m_stateCnt=3", "m_pos.y==70",
):
    if token not in sight_fixture.group(1):
        raise AssertionError(f"effect22 Sight lifecycle coverage missing {token}")

print("effects22/601 Sight/Sight2 parity passed: PE/Ref helpers, callers, local routes, exact resources and lifecycle bindings")
