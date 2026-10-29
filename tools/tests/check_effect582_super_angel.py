#!/usr/bin/env python3
"""Compare effect582's ordered SuperAngel variant calls and resources."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
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


def caller(source: str) -> str:
    body = function(source, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
    start = body.index("case 0x246:")
    return body[start:body.index("case 0x247:", start)]


pe_caller, ref_caller = caller(PE), caller(REF)
if compact(pe_caller) != compact(ref_caller):
    raise AssertionError("PE and primary-Ref effect582 caller branches differ")
if not re.search(r"SuperAngel\(this, 11, 0\);\s*CRagEffect::SuperAngel\(this, 10, 0\);", pe_caller):
    raise AssertionError("effect582 variant11 then variant10 caller order changed")

pe_helper = function(PE, "void __thiscall CRagEffect::SuperAngel(CRagEffect *this, char F1, char time)")
ref_helper = function(REF_HELPER, "void __thiscall CRagEffect::SuperAngel(CRagEffect *this, char F1, char time)")
if compact(pe_helper) != compact(ref_helper):
    raise AssertionError("PE and primary-Ref SuperAngel helper bodies differ")

if "case 582: ProcessSuperAngelEffect(); return true;" not in ROUTES:
    raise AssertionError("local effect582 route changed")
local_process = function(LOCAL, "void CRagEffect::ProcessSuperAngelEffect()")
local_branch = local_process[local_process.index("} else if (m_type == 582)"):]
if not re.search(r"ApplySuperAngel\(11, 0\);\s*ApplySuperAngel\(10, 0\);", local_branch):
    raise AssertionError("local effect582 variant order changed")
if "PlayWave" in local_branch:
    raise AssertionError("local effect582 unexpectedly adds a wave")

emitter = function(LOCAL, "void CRagEffect::ApplySuperAngel(signed char variant, signed char)")
for token in ("variant == 1 || variant == 11", "m_stateCnt % 3 == 0", "variant == 10",
              "variant == 11", "SetParticleResources(asset.spr, asset.act)",
              "gi.flag[0] = static_cast<u8>(variant)", "gi.sparkY = m_effectPos.y - 30.0f"):
    if token not in emitter:
        raise AssertionError(f"local SuperAngel variant payload missing {token}")
if "{ 11, \"\\xC0\\xCC\\xC6\\xD1\\xC6\\xAE\\\\\\xC7\\xD1\\xBA\\xB9\\xC3\\xB5\\xBB\\xE7(\\xB3\\xAF\\xB0\\xB3).spr\"" not in emitter:
    raise AssertionError("local effect582 variant11 sprite mapping changed")

image = ExeImage(ROOT / "HighPriest.exe")
for va, expected in (
    (0x71C048, b"\xC0\xCC\xC6\xD1\xC6\xAE\\\xC7\xD1\xBA\xB9\xC3\xB5\xBB\xE7(\xB3\xAF\xB0\xB3).spr"),
    (0x71C02C, b"\xC0\xCC\xC6\xD1\xC6\xAE\\\xC7\xD1\xBA\xB9\xC3\xB5\xBB\xE7(\xB3\xAF\xB0\xB3).act"),
    (0x71C078, b"\xC0\xCC\xC6\xD1\xC6\xAE\\\xC7\xD1\xBA\xB9\xC3\xB5\xBB\xE7.spr"),
    (0x71C064, b"\xC0\xCC\xC6\xD1\xC6\xAE\\\xC7\xD1\xBA\xB9\xC3\xB5\xBB\xE7.act"),
):
    actual = image.read_cstring_at(va)
    if actual is None or actual.raw != expected:
        raise AssertionError(f"SuperAngel variant resource differs at VA 0x{va:X}")

print("effect582 SuperAngel parity passed: PE/Ref caller/helper, local variant11/10 order and exact assets")
