#!/usr/bin/env python3
"""Compare Level99 PE callers/helpers and local routes across OnProcess families."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF2 = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="latin-1")
REF_PRIMARY = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
ROUTE = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
ROUTE3 = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")


def function_body(source, signature):
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


def compact(source):
    return re.sub(r"\s+", "", source)


pe_process = function_body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
if not re.search(r"case 200:\s*case 397:\s*CRagEffect::Level99\(this, aEffectRingBlue, 0\);\s*break;", pe_process):
    raise AssertionError("PE effects200/397 Level99 caller family changed")
if not re.search(r"case 201:\s*case 398:\s*CRagEffect::Level99_2\(this, aEffectPikapika, 0\);\s*break;", pe_process):
    raise AssertionError("PE effects201/398 Level99_2 caller family changed")

for name, signature in (
    ("Level99", "void __thiscall CRagEffect::Level99(CRagEffect *this, char *tName, char F1)"),
    ("Level99_2", "void __thiscall CRagEffect::Level99_2(CRagEffect *this, char *tName, char F1)"),
    ("Level99_3", "void __thiscall CRagEffect::Level99_3(CRagEffect *this, char F1)"),
):
    pe_body = function_body(PE, signature)
    ref_body = function_body(REF2, signature)
    if compact(pe_body) != compact(ref_body):
        raise AssertionError(f"PE and primary-Ref {name} helper bodies differ")

for source_name, source in (("PE", PE), ("primary Ref", REF_PRIMARY)):
    process3 = function_body(source, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
    for effect_id, variant in ((678, 2), (679, 1), (680, 1), (692, 3)):
        expected = (rf"case 0x{effect_id:X}:\s*CRagEffect::Level99_3\(this, {variant}\);\s*return 1;"
                    if effect_id in (678, 692) else
                    rf"case 0x{effect_id:X}:\s*CRagEffect::Level99(?:_2)?\(this, [^;]*, {variant}\);\s*return 1;")
        if not re.search(expected, process3):
            raise AssertionError(f"{source_name} OnProcess3 effect{effect_id} caller changed")

for effect_id, route_call in (
    (200, "ProcessLevel99Effect"), (397, "ProcessLevel99Effect"),
    (201, "ProcessLevel99_2Effect"), (398, "ProcessLevel99_2Effect"),
):
    if f"case {effect_id}: return {route_call}();" not in ROUTE:
        raise AssertionError(f"local effect{effect_id} route changed")

level99 = function_body(LOCAL, "bool CRagEffect::ProcessLevel99Effect()")
for token in (
    "case 200:", "case 397:", 'textureName = "effect\\\\ring_blue.tga"',
    "m_stateCnt == 99999998", "m_stateCnt != 0", "PP_3DCASTING",
    "maxHeights{15.0f, 13.0f, 11.0f}", "distances{3.9000001f, 4.0999999f, 4.3000002f}",
):
    if token not in level99:
        raise AssertionError(f"local Level99 profile missing {token}")
level99_2 = function_body(LOCAL, "bool CRagEffect::ProcessLevel99_2Effect()")
for token in (
    "case 201:", "case 398:", 'textureName = "effect\\\\pikapika2.bmp"',
    "m_stateCnt == 99999998", "m_stateCnt != 0", "PP_3DAURA",
    "m_gi[0].distance = 15.0f", "m_gi[1].distance = 15.0f",
):
    if token not in level99_2:
        raise AssertionError(f"local Level99_2 profile missing {token}")

if "case 397" in REF_PRIMARY or "case 398" in REF_PRIMARY:
    raise AssertionError("primary Ref added numeric Level99 callers; update provenance")

image = ExeImage(ROOT / "HighPriest.exe")
for va, expected, label in (
    (0x71943C, b"effect\\ring_blue.tga", "Level99 ring-blue"),
    (0x719DD8, b"effect\\pikapika2.bmp", "Level99_2 pikapika2"),
    (0x71C3C0, b"effect\\freezing_circle.bmp", "Level99_3 freezing circle"),
    (0x719780, b"effect\\whitelight.tga", "Level99_3 white light"),
    (0x71C3DC, b"effect\\ghost.bmp", "Level99_3 ghost"),
):
    resource = image.read_cstring_at(va)
    if resource is None or resource.raw != expected:
        raise AssertionError(f"{label} bytes differ at PE VA 0x{va:06X}")

for effect_id, route_call in ((678, "ProcessLevel99_3Effect"), (692, "ProcessLevel99_3Effect"),
                              (679, "ProcessLevel99Effect"), (680, "ProcessLevel99_2Effect")):
    if f"case {effect_id}: return {route_call}();" not in ROUTE3:
        raise AssertionError(f"local effect{effect_id} OnProcess3 route changed")
level99_3 = function_body(LOCAL, "bool CRagEffect::ProcessLevel99_3Effect()")
for token in (
    "case 678: variant = 2;", "case 692: variant = 3;", '"effect\\\\ghost.bmp"',
    '"effect\\\\freezing_circle.bmp"', '"effect\\\\whitelight.tga"',
    "variant == 2 ? 7.0f : 0.0f", "variant != 3 || index < 1",
    "variant == 1 ? 1 : variant == 3 ? 11 : 0", "rand() % 15 - 8", "rand() % 360",
):
    if token not in level99_3:
        raise AssertionError(f"local Level99_3 profile missing {token}")

print("Level99 parity passed: effects397/398, OnProcess3 variants678/679/680/692, full PE/Ref helpers, local profiles and exact resources")
