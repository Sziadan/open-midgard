#!/usr/bin/env python3
"""Compare effects123/132 GrimTooth helpers, PE caller routes, and local bindings."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")


def function_body(source: str, signature: str) -> str:
    match = list(re.finditer(signature, source))
    if not match:
        raise AssertionError(f"missing {signature}")
    opening = source.find("{", match[-1].end())
    depth = 0
    for offset in range(opening, len(source)):
        if source[offset] == "{":
            depth += 1
        elif source[offset] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:offset]
    raise AssertionError(f"unterminated {signature}")


def compact(text: str) -> str:
    return re.sub(r"\s+", "", text)


for name in ("GrimTooth", "GrimToothAtk"):
    signature = rf"void __thiscall CRagEffect::{name}\(CRagEffect \*this\)"
    if compact(function_body(PE, signature)) != compact(function_body(REF, signature)):
        raise AssertionError(f"canonical PE and primary Ref {name} helper bodies differ")

pe_onprocess = function_body(PE, r"unsigned __int8 __thiscall CRagEffect::OnProcess\(CRagEffect \*this\)")
for effect_id, method in ((123, "GrimTooth"), (132, "GrimToothAtk")):
    if not re.search(rf"case\s+{effect_id}:\s*CRagEffect::{method}\(this\);\s*break;", pe_onprocess):
        raise AssertionError(f"canonical PE effect{effect_id} caller changed")
    if not re.search(rf"case\s+{effect_id}:\s*return ProcessEffectFrameLegacy\(\);", DISPATCH):
        raise AssertionError(f"local effect{effect_id} route changed")
if re.search(r"CRagEffect::GrimTooth(?:Atk)?\(this\)", REF):
    raise AssertionError("primary Ref unexpectedly added a dispatcher caller; review the PE-only caller mapping")

if "void SpawnGrimTooth();" not in HEADER or "void SpawnGrimToothAtk();" not in HEADER:
    raise AssertionError("local GrimTooth emitter declarations are missing")
local_frame = function_body(LOCAL, r"bool CRagEffect::ProcessEffectFrameLegacy\(\)")
if not re.search(r"if\s*\(m_type\s*==\s*123\)\s*SpawnGrimTooth\(\);\s*else\s*SpawnGrimToothAtk\(\);", local_frame):
    raise AssertionError("local legacy frame dispatch no longer maps ids123/132 to their matching emitters")

quad_horn = function_body(LOCAL, r"bool CEffectPrim::UsesReferenceQuadHorn\(\) const")
for effect_id in (123, 132):
    if not re.search(rf"m_master->GetEffectType\(\)\s*==\s*{effect_id}", quad_horn):
        raise AssertionError(f"effect{effect_id} missing from the source-backed quad-horn family")

image = ExeImage(ROOT / "HighPriest.exe")
stone = image.read_cstring_at(0x71B858)
if stone is None or stone.raw != b"effect\\stone.bmp":
    raise AssertionError("canonical PE stone texture bytes changed at VA 0x71B858")
for method in ("SpawnGrimTooth", "SpawnGrimToothAtk"):
    body = function_body(LOCAL, rf"void CRagEffect::{method}\(\)")
    if 'GetTexture("effect\\\\stone.bmp",false)' not in body:
        raise AssertionError(f"local {method} no longer requests canonical stone.bmp")

print("GrimTooth123/132 parity passed: PE/Ref helpers, PE callers, local routes, quad-horn family and exact stone.bmp bytes")
