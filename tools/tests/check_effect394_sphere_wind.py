#!/usr/bin/env python3
"""Compare effect394's five SphereWind calls with local route and profile."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
ROUTE = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")


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


pe_process = function_body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
if not re.search(r"case 394:\s*for \( k = 0; k < 45; k \+= 9 \)\s*"
                 r"CRagEffect::SphereWind\(this, aEffectFireIvyB, k, 1\);\s*break;", pe_process):
    raise AssertionError("PE effect394 loop bounds, stride, texture or variant changed")

if "case 394: for (short windTime = 0; windTime < 45; windTime = static_cast<short>(windTime + 9)) ApplySphereWind(\"effect\\\\fire_ivy.bmp\", windTime, 1); return true;" not in ROUTE:
    raise AssertionError("local effect394 generated caller route changed")
local_helper = function_body(LOCAL, "void CRagEffect::ApplySphereWind(const char* textureName, short time, int variant)")
for token in (
    "m_stateCnt != 0", "kPrimTypeSphereWind", "variant == 1 ? 10.0f : 4.0f",
    "GetTexture(textureName, false)", "time + 45 * band", "band < 4", "gi.riseAngle",
):
    if token not in local_helper:
        raise AssertionError(f"local SphereWind profile missing {token}")

if "case 394:" in REF or "case 0x18A:" in REF:
    raise AssertionError("primary Ref gained a numeric effect394 caller; update source provenance")

image = ExeImage(ROOT / "HighPriest.exe")
texture = image.read_cstring_at(0x71A7AC)
if texture is None or texture.raw != b"effect\\fire_ivy.bmp":
    raise AssertionError("effect394 fire-ivy texture bytes differ at PE VA 0x71A7AC")

print("effect394 SphereWind parity passed: PE loop, local route/profile, exact texture and Ref caller provenance")
