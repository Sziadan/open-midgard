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
):
    case_num = effect_id
    pe_branch = branch(pe_process, case_num, case_num + 1)
    ref_branch = branch(ref_process, case_num, case_num + 1)
    if compact(pe_branch) != compact(ref_branch):
        raise AssertionError(f"PE/primary-Ref effect{effect_id} caller differs")
    offset = 0
    for symbol, variant in expected:
        token = f"ThrowItem(this, &{symbol}, {variant})"
        found = pe_branch.find(token, offset)
        if found < 0:
            raise AssertionError(f"effect{effect_id} ordered caller missing {token}")
        offset = found + len(token)
    route = "case 539: return ProcessEffectFrameLegacy();" if effect_id == 539 else "case 541: return ProcessThrowItemEffect();"
    if route not in ROUTES:
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

local_helper = function(LOCAL, "void CRagEffect::ApplyThrowItem(const char* textureName, int variant)")
for token in ("variant == 10", "launchFrame = 5", "variant == 11", "launchFrame = 10",
              "m_stateCnt != launchFrame", "kPrimTypeThrowItem", "variant == 1",
              "gi.flag[2] = 9", "gi.flag[3] = 4"):
    if token not in local_helper:
        raise AssertionError(f"local ThrowItem lifecycle/payload missing {token}")

image = ExeImage(ROOT / "HighPriest.exe")
expected_resources = {
    0x71992C: b"\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\item\\\xC8\xAD\xBF\xB0\xBA\xB4.bmp",
    0x71990C: b"\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\item\\\xBF\xB0\xBB\xEA\xBA\xB4.bmp",
    0x7198E8: b"\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\item\\\xB9\xF6\xBC\xAD\xC5\xA9\xC6\xF7\xBC\xC7.bmp",
}
for va, expected in expected_resources.items():
    actual = image.read_cstring_at(va)
    if actual is None or actual.raw != expected:
        raise AssertionError(f"ThrowItem resource differs at PE VA 0x{va:X}")

print("effects539/541 ThrowItem parity passed: PE/Ref caller order, local variants and launch frames, exact textures")
