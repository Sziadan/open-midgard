#!/usr/bin/env python3
"""Compare effect673 POISON variant2 caller and primitive payload."""
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


def caller(source: str) -> str:
    body = function(source, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
    start = body.index("case 0x2A1:")
    return body[start:body.index("case 0x2A2:", start)]


pe_caller, ref_caller = caller(PE), caller(REF)
if compact(pe_caller) != compact(ref_caller):
    raise AssertionError("PE and primary-Ref effect673 callers differ")
if not re.search(r"POISON\(this, 2\);\s*return 1;", pe_caller):
    raise AssertionError("effect673 POISON variant2 caller changed")

pe_helper = function(PE, "void __thiscall CRagEffect::POISON(CRagEffect *this, char F1)")
ref_helper = function(REF_HELPER, "void __thiscall CRagEffect::POISON(CRagEffect *this, char F1)")
if compact(pe_helper) != compact(ref_helper):
    raise AssertionError("PE and primary-Ref POISON helper bodies differ")
if "case 673: ApplyPoison(2); return true;" not in ROUTES:
    raise AssertionError("local effect673 generated route changed")

local = function(LOCAL, "void CRagEffect::ApplyPoison(int variant)")
for token in ("m_stateCnt == 99999998", "m_stateCnt = 0", "m_duration = 99999999",
              "m_stateCnt == 0", "kPrimTypePoison", "origin.y -= 9.0f",
              "prim->m_renderFlag |= 0x200u", "prim->m_duration = m_duration",
              'variant != 0 ? "effect\\\\white02.bmp" : "effect\\\\poison_f.bmp"',
              "gi.life = 1", "gi.process = 0", "gi.distance = 30.0f", "gi.maxHeight = 0.0f",
              "gi.alphaB = 0", "gi.flag[10] = static_cast<u8>(variant)", "prim->m_pos = origin"):
    if token not in local:
        raise AssertionError(f"local POISON variant behavior missing {token}")

image = ExeImage(ROOT / "HighPriest.exe")
if not any(item.raw == b"effect\\white02.bmp" for item in image.iter_cstrings()):
    raise AssertionError("PE effect673 selector resource bytes missing")

if not re.search(r"CRagEffect poison;[\s\S]*?poison\.ApplyPoison\(2\);", LIFECYCLE):
    raise AssertionError("effect673 variant2 lifecycle fixture does not invoke the production helper")
for token in (
    "effect673 sentinel state resets duration",
    "effect673 variant2 launches one white poison primitive",
    "effect673 variant2 initializes exact poison GI payload",
    "effect673 later state follows the master without relaunching",
):
    if token not in LIFECYCLE:
        raise AssertionError(f"effect673 lifecycle assertion missing: {token}")

print("effect673 POISON parity passed: PE/Ref caller/helper, local variant2 state/resource/payload, runtime lifecycle fixture")
