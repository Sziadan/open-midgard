#!/usr/bin/env python3
"""Compare effect601 Sight2's original helper and local source/lifecycle route."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
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

print("effect601 Sight2 parity passed: complete PE/Ref helper, caller, local shared emitter, exact resources and lifecycle binding")
