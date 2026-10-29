#!/usr/bin/env python3
"""Check PE/Ref and local GumGang emitter setup for its six effect callers."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage  # noqa: E402

pe = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
ref = (ROOT / "Ref/dhxj/RagEffect2.cpp").read_text(encoding="latin-1")
local = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
LIFECYCLE = (ROOT / "tools/tests/EffectLifecycleTests.cpp").read_text(encoding="utf-8")


def function_body(source, signature):
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 0
    for position in range(opening, len(source)):
        if source[position] == "{":
            depth += 1
        elif source[position] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:position]
    raise AssertionError(f"unterminated function {signature}")


pe_functions = {
    "GumGang": function_body(pe, "void __thiscall CRagEffect::GumGang(CRagEffect *this, char F1, char F2)"),
    "DoubleGumGang": function_body(pe, "void __thiscall CRagEffect::DoubleGumGang(CRagEffect *this, char F1, char F2)"),
}
ref_functions = {
    "GumGang": function_body(ref, "void\tCRagEffect :: GumGang(char F1,char F2)"),
    "DoubleGumGang": function_body(ref, "void\tCRagEffect :: DoubleGumGang(char F1,char F2)"),
}

# The older dhxj helper consumes one additional random draw for height[15].
# Canonical PE leaves that slot untouched; the local implementation follows PE.
for name in ("GumGang", "DoubleGumGang"):
    source, reference = pe_functions[name], ref_functions[name]
    if source.count("rand()") != 12:
        raise AssertionError(f"PE {name} must consume 12 draws per GI")
    if len(re.findall(r"\brandom\s*\(", reference)) != 13:
        raise AssertionError(f"older Ref {name} should retain its documented height[15] draw")
    if "height[15]" in source:
        raise AssertionError(f"canonical PE {name} unexpectedly writes height[15]")
    source_profile = ("PP_GUMGANG", "3.5", "70.0", "130.0", "190.0", "250.0", "m_renderFlag |= 5u") if name == "GumGang" else (
        "PP_DOUBLEGUMGANG", "2.0", "40.0", "80.0", "120.0", "160.0", "m_renderFlag = 5")
    for token in ("this->m_pos = this->m_master->m_pos", "!this->m_stateCnt", source_profile[6],
                  "m_duration = this->m_duration", "m_totalTexture = 5", "aEffectSuper1Bm",
                  "aEffectSuper2Bm", "aEffectSuper3Bm", "aEffectSuper4Bm", "aEffectSuper5Bm",
                  f"LaunchEffectPrim(this, {source_profile[0]}", f"distance = {source_profile[1]}",
                  f"height[1] = {source_profile[2]}", f"height[5] = {source_profile[3]}",
                  f"height[9] = {source_profile[4]}", f"height[13] = {source_profile[5]}", "F1a = F1"):
        if token not in source:
            raise AssertionError(f"canonical PE {name} payload missing {token}")
    reference_compact = re.sub(r"\s+", "", reference)
    for token in ("m_pos=m_master->m_pos", "m_stateCnt==0", "m_duration", "m_totalTexture=5",
                  "super1.bmp", "super2.bmp", "super3.bmp", "super4.bmp", "super5.bmp",
                  "height[0]", "height[4]", "height[8]", "height[12]", "flag1[4]=F2"):
        if token not in reference_compact:
            raise AssertionError(f"Ref {name} setup missing {token}")

# Canonical effect callers are the authority for the six grouped configurations.
callers = {
    203: "CRagEffect::GumGang(this, 1, 0)",
    254: "CRagEffect::GumGang(this, 4, 0)",
    455: "CRagEffect::GumGang(this, 2, 1)",
    418: "CRagEffect::DoubleGumGang(this, 2, 2)",
    485: "CRagEffect::DoubleGumGang(this, 2, 1)",
    486: "CRagEffect::DoubleGumGang(this, 2, 0)",
}
for effect_id, call in callers.items():
    if call not in pe:
        raise AssertionError(f"PE caller for effect{effect_id} changed")
    if f"case {effect_id}: return ProcessGumGangEffect();" not in dispatch:
        raise AssertionError(f"local dispatch for effect{effect_id} missing")

local_helper = function_body(local, "bool CRagEffect::ProcessGumGangEffect()")
for token in (
    "case 203: giCount = 1", "case 254: giCount = 4", "case 455: giCount = 2; colorVariant = 1",
    "case 418: giCount = 2; colorVariant = 2; doubleGumGang = true",
    "case 485: giCount = 2; colorVariant = 1; doubleGumGang = true",
    "case 486: giCount = 2; colorVariant = 0; doubleGumGang = true",
    "if (m_stateCnt != 0)", "prim->m_renderFlag = 5", "prim->m_duration = m_duration",
    "prim->m_totalTexture = 5", "doubleGumGang ? PP_DOUBLEGUMGANG : PP_GUMGANG",
    "gi.distance = doubleGumGang ? 2.0f : 3.5f", "gi.flag[4] = static_cast<u8>(colorVariant)",
):
    if token not in local_helper:
        raise AssertionError(f"local GumGang emitter mapping missing {token}")
if local_helper.count("rand()") != 12:
    raise AssertionError("local GumGang helper must consume 12 draws per GI")
for token in (
    "gi.height[1] = doubleGumGang ? 40.0f : 70.0f",
    "gi.height[5] = doubleGumGang ? 80.0f : 130.0f",
    "gi.height[9] = doubleGumGang ? 120.0f : 190.0f",
    "gi.height[13] = doubleGumGang ? 160.0f : 250.0f",
):
    if token not in local_helper:
        raise AssertionError(f"local GumGang height profile missing {token}")
if "height[15]" in local_helper:
    raise AssertionError("local canonical GumGang path must leave height[15] untouched")

image = ExeImage(ROOT / "HighPriest.exe")
for va, expected in (
    (0x71C3AC, b"effect\\super1.bmp"),
    (0x71C398, b"effect\\super2.bmp"),
    (0x71C384, b"effect\\super3.bmp"),
    (0x71C370, b"effect\\super4.bmp"),
    (0x71C35C, b"effect\\super5.bmp"),
):
    resource = image.read_cstring_at(va)
    if resource is None or resource.raw != expected:
        raise AssertionError(f"GumGang texture bytes differ at PE VA 0x{va:06X}")

fixture = re.search(r"CRagEffect effect;effect\.m_type=203;[\s\S]*?effect\.ClearPrims\(\);g_world\.UpdateGameObjects\(\);", LIFECYCLE)
if fixture is None or "effect.ProcessGumGangEffect()" not in fixture.group(0):
    raise AssertionError("effect203 runtime fixture does not invoke the production GumGang handler")
for token in (
    "effect203 emits one ordered five-texture GumGang primitive",
    "effect203 GI0 preserves the exact PE GumGang profile",
    "effect203 activates only its single PE GumGang band",
    "effect203 later state refreshes owner origin without relaunch",
):
    if token not in fixture.group(0):
        raise AssertionError(f"effect203 lifecycle assertion missing: {token}")

effect455_fixture = re.search(r"CRagEffect effect;effect\.m_type=455;[\s\S]*?effect\.ClearPrims\(\);g_world\.UpdateGameObjects\(\);", LIFECYCLE)
if effect455_fixture is None or "effect.ProcessGumGangEffect()" not in effect455_fixture.group(0):
    raise AssertionError("effect455 two-band runtime fixture does not invoke the production GumGang handler")
for token in (
    "effect455 emits one ordered five-texture GumGang primitive with 24 controlled draws",
    "effect455 preserves both active GumGang GI profiles and selector flag4=1",
    "effect455 leaves the remaining two GumGang bands inactive",
    "effect455 later state refreshes origin without reemission, texture or RNG",
):
    if token not in effect455_fixture.group(0):
        raise AssertionError(f"effect455 lifecycle assertion missing: {token}")

effect418_fixture = re.search(r"CRagEffect effect;effect\.m_type=418;[\s\S]*?effect\.ClearPrims\(\);g_world\.UpdateGameObjects\(\);", LIFECYCLE)
if effect418_fixture is None or "effect.ProcessGumGangEffect()" not in effect418_fixture.group(0):
    raise AssertionError("effect418 double-band runtime fixture does not invoke the production GumGang handler")
for token in (
    "effect418 emits one size-zero DoubleGumGang with ordered assets",
    "effect418 preserves both DoubleGumGang GI profiles and selector flag4=2",
    "effect418 leaves the remaining two DoubleGumGang bands inactive",
    "effect418 later state refreshes origin without re-emission, texture or RNG",
):
    if token not in effect418_fixture.group(0):
        raise AssertionError(f"effect418 lifecycle assertion missing: {token}")

print("GumGang emitter parity passed: PE/Ref setup, six PE/local callers, RNG profile, height15 mismatch and five PE assets")
