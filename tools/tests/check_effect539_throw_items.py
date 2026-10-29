#!/usr/bin/env python3
"""Compare effect539 dual and effect541 single ThrowItem callers."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF_CALLER = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
REF_HELPER = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="latin-1")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
ROUTES = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")
LIFECYCLE = (ROOT / "tools/tests/EffectLifecycleTests.cpp").read_text(encoding="utf-8")


def function(source: str, signature: str) -> str:
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:pos]
    raise AssertionError(f"unterminated function: {signature}")


def compact(text: str) -> str:
    return re.sub(r"\s+", "", text)


def branch(body: str, case: int, next_case: int) -> str:
    start = body.index(f"case 0x{case:X}:")
    return body[start:body.index(f"case 0x{next_case:X}:", start)]


pe_process = function(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
ref_process = function(REF_CALLER, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
for effect_id, expected in (
    (539, (("byte_71992C", 10), ("byte_71990C", 11))),
    (541, (("byte_7198E8", 1),)),
    (613, (("byte_7199F8", 3),)),
    (614, (("byte_7199D4", 3),)),
    (615, (("byte_7199B0", 3),)),
    (616, (("aEffectCoinABmp", 3),)),
):
    case_num = effect_id
    pe_branch = branch(pe_process, case_num, case_num + 1)
    ref_branch = branch(ref_process, case_num, case_num + 1)
    if compact(pe_branch) != compact(ref_branch):
        raise AssertionError(f"PE/primary-Ref effect{effect_id} caller differs")
    offset = 0
    for symbol, variant in expected:
        address = "" if symbol == "aEffectCoinABmp" else "&"
        token = f"ThrowItem(this, {address}{symbol}, {variant})"
        found = pe_branch.find(token, offset)
        if found < 0:
            raise AssertionError(f"effect{effect_id} ordered caller missing {token}")
        offset = found + len(token)
    route = "case 539: return ProcessEffectFrameLegacy();" if effect_id == 539 else f"case {effect_id}: return ProcessThrowItemEffect();"
    if route not in ROUTES:
        raise AssertionError(f"local effect{effect_id} generated route changed")

pe_600 = branch(pe_process, 0x258, 0x259)
ref_600 = branch(ref_process, 0x258, 0x259)
if compact(pe_600) != compact(ref_600):
    raise AssertionError("PE and primary-Ref effect600 ThrowItem callers differ")
if "ThrowItem(this, &byte_719A18, 3)" not in pe_600:
    raise AssertionError("effect600 ThrowItem variant3 or resource symbol changed")
if "case 600: return ProcessThrowItemEffect();" not in ROUTES:
    raise AssertionError("local effect600 generated route changed")
for effect_id in (613, 614, 615, 616):
    if f"case {effect_id}: return ProcessThrowItemEffect();" not in ROUTES:
        raise AssertionError(f"local effect{effect_id} generated route changed")

pe_helper = function(PE, "void __thiscall CRagEffect::ThrowItem(CRagEffect *this, char *tName, char F1)")
ref_helper = function(REF_HELPER, "void __thiscall CRagEffect::ThrowItem(CRagEffect *this, char *tName, char F1)")
if compact(pe_helper) != compact(ref_helper):
    raise AssertionError("PE and primary-Ref ThrowItem helpers differ")

legacy = function(LOCAL, "bool CRagEffect::ProcessEffectFrameLegacy()")
local539 = re.search(r"else if \(dispatchHandler && m_type == 539\)\s*\{([\s\S]*?)\n\s*\} else if", legacy)
local541 = re.search(r"else if \(dispatchHandler && \(m_type == 299 \|\| m_type == 541\)\)\s*\{([\s\S]*?)\n\s*\} else if", legacy)
if local539 is None or local541 is None:
    raise AssertionError("local dual/single ThrowItem caller branches missing")
if not re.search(r"ApplyThrowItem\([^\n]*\\xC8\\xAD\\xBF\\xB0\\xBA\\xB4\.bmp\", 10\);\s*ApplyThrowItem\([^\n]*\\xBF\\xB0\\xBB\\xEA\\xBA\\xB4\.bmp\", 11\);", local539.group(1)):
    raise AssertionError("local effect539 ordered resource/variant pairing changed")
if not re.search(r"m_type == 299\s*\?[^:]*\\xBA\\xF3\\xC6\\xF7\\xBC\\xC7\\xBA\\xB4\.bmp[^:]*:\s*\"[^\"]*\\xB9\\xF6\\xBC\\xAD\\xC5\\xA9\\xC6\\xF7\\xBC\\xC7\.bmp\",\s*1\);", local541.group(1)):
    raise AssertionError("local effect541 resource or selector1 mapping changed")
local_process = function(LOCAL, "bool CRagEffect::ProcessThrowItemEffect()")
if not re.search(r'case 600:\s*ApplyThrowItem\("[^\"]*\\xBA\\xA3\\xB3\\xD1\\xB3\\xAA\\xC0\\xCC\\xC7\\xC1\.bmp", 3\);\s*return true;', local_process):
    raise AssertionError("local effect600 variant3 resource mapping changed")
for effect_id, escaped in (
    (613, r"\\xBC\\xF6\\xB8\\xAE\\xB0\\xCB\.bmp"),
    (614, r"\\xC4\\xED\\xB3\\xAA\\xC0\\xCC_\\xB5\\xB6\.bmp"),
    (615, r"\\xC7\\xB3\\xB8\\xB6_\\xBF\\xAD\\xC8\\xAD\.bmp"),
):
    if not re.search(rf'case {effect_id}:\s*ApplyThrowItem\("[^"\n]*{escaped}", 3\);\s*return true;', local_process):
        raise AssertionError(f"local effect{effect_id} variant3 resource mapping changed")
if 'case 616:\n        ApplyThrowItem("effect\\\\coin_a.bmp", 3);\n        return true;' not in local_process:
    raise AssertionError("local effect616 variant3 coin resource mapping changed")

local_helper = function(LOCAL, "void CRagEffect::ApplyThrowItem(const char* textureName, int variant)")
for token in ("variant == 10", "launchFrame = 5", "variant == 11", "launchFrame = 10",
              "m_stateCnt != launchFrame", "kPrimTypeThrowItem", "variant == 1",
              "variant == 3", "gi.distance = variant == 2 ? 2.0f : variant == 3 ? 4.0f : 3.0f",
              "gi.height[1] = variant == 2 ? 2.0f : variant == 3 ? 3.0f",
              "gi.fullDisplayAngle = variant == 3 ? 330 : 0", "gi.flag[2] = 15",
              "gi.flag[2] = 9", "gi.flag[3] = 4"):
    if token not in local_helper:
        raise AssertionError(f"local ThrowItem lifecycle/payload missing {token}")

image = ExeImage(ROOT / "HighPriest.exe")
expected_resources = {
    0x71992C: b"\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\item\\\xC8\xAD\xBF\xB0\xBA\xB4.bmp",
    0x71990C: b"\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\item\\\xBF\xB0\xBB\xEA\xBA\xB4.bmp",
    0x7198E8: b"\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\item\\\xB9\xF6\xBC\xAD\xC5\xA9\xC6\xF7\xBC\xC7.bmp",
    0x719A18: b"\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\item\\\xBA\xA3\xB3\xD1\xB3\xAA\xC0\xCC\xC7\xC1.bmp",
    0x7199F8: b"\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\item\\\xBC\xF6\xB8\xAE\xB0\xCB.bmp",
    0x7199D4: b"\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\item\\\xC4\xED\xB3\xAA\xC0\xCC_\xB5\xB6.bmp",
    0x7199B0: b"\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\item\\\xC7\xB3\xB8\xB6_\xBF\xAD\xC8\xAD.bmp",
    0x71999C: b"effect\\coin_a.bmp",
}
for va, expected in expected_resources.items():
    actual = image.read_cstring_at(va)
    if actual is None or actual.raw != expected:
        raise AssertionError(f"ThrowItem resource differs at PE VA 0x{va:X}")

if not re.search(r"CRagEffect effect;effect\.m_type=600;[\s\S]*?effect\.ProcessThrowItemEffect\(\);", LIFECYCLE):
    raise AssertionError("effect600 variant3 lifecycle fixture does not invoke the production dispatcher")
for token in (
    "effect600 launches one variant3 ThrowItem with the exact PE texture",
    "effect600 variant3 preserves exact trajectory, angle and selector flags",
    "effect600 later state repositions existing ThrowItem without duplicate launch",
):
    if token not in LIFECYCLE:
        raise AssertionError(f"effect600 lifecycle assertion missing: {token}")

if not re.search(r"CRagEffect effect;effect\.m_type=616;[\s\S]*?effect\.ProcessThrowItemEffect\(\);", LIFECYCLE):
    raise AssertionError("effect616 coin lifecycle fixture does not invoke the production dispatcher")
for token in (
    "effect616 selects exact ASCII effect\\\\coin_a.bmp",
    "effect616 applies the exact variant3 trajectory and selector fields",
    "effect616 later state repositions the coin without relaunch or resource reload",
):
    if token not in LIFECYCLE:
        raise AssertionError(f"effect616 lifecycle assertion missing: {token}")

throw_cases = re.search(r"const ThrowItemCase items\[\]=\{([\s\S]*?)\};", LIFECYCLE)
if (throw_cases is None or "for(const ThrowItemCase& item:items)" not in LIFECYCLE
        or "effect.ProcessThrowItemEffect()" not in LIFECYCLE
        or "g_texMgr.requests==std::vector<std::string>({item.texture})" not in LIFECYCLE):
    raise AssertionError("effects613-615 lifecycle fixture does not invoke the production dispatcher loop")
for effect_id in (613, 614, 615):
    if not re.search(rf"\{{{effect_id},", throw_cases.group(1)):
        raise AssertionError(f"effect{effect_id} runtime texture case missing from ThrowItem fixture")
for token in (
    "effects613-615 launch variant3 with each exact CP949 texture",
    "effects613-615 share the exact variant3 trajectory and selector fields",
    "effects613-615 follow master without duplicate launch or resource request",
):
    if token not in LIFECYCLE:
        raise AssertionError(f"effects613-615 lifecycle assertion missing: {token}")

effect539_fixture = re.search(r"CRagEffect effect;effect\.m_type=539;[\s\S]*?effect\.ClearPrims\(\);g_world\.UpdateGameObjects\(\);", LIFECYCLE)
if effect539_fixture is None or "effect.ProcessThrowItemEffect()" not in effect539_fixture.group(0):
    raise AssertionError("effect539 delayed-pair lifecycle fixture does not invoke the production dispatcher")
for state in ("m_stateCnt=0", "m_stateCnt=5", "m_stateCnt=10", "m_stateCnt=11"):
    if state not in effect539_fixture.group(0):
        raise AssertionError(f"effect539 lifecycle fixture is missing checkpoint {state}")
for token in (
    "effect539 state5 launches only the first PE texture",
    "effect539 state10 appends the second PE texture in order",
    "effect539 state11 repositions both items without duplicate launches",
):
    if token not in effect539_fixture.group(0):
        raise AssertionError(f"effect539 lifecycle assertion missing: {token}")

effect541_fixture = re.search(r"CRagEffect effect;effect\.m_type=541;[\s\S]*?effect\.ClearPrims\(\);g_world\.UpdateGameObjects\(\);", LIFECYCLE)
if effect541_fixture is None or "effect.ProcessThrowItemEffect()" not in effect541_fixture.group(0):
    raise AssertionError("effect541 selector1 lifecycle fixture does not invoke the production dispatcher")
for token in (
    "effect541 launches selector1 with exact CP949 texture, delta and master origin",
    "effect541 selector1 preserves trajectory and source count flags",
    "effect541 later state repositions existing item without duplicate launch or resource request",
):
    if token not in effect541_fixture.group(0):
        raise AssertionError(f"effect541 lifecycle assertion missing: {token}")

print("effects539/541/600/613/614/615/616 ThrowItem parity passed: PE/Ref callers, local variants and launch frames, exact textures")
