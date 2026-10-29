#!/usr/bin/env python3
"""Compare effect488 FlowerCast PE/secondary-Ref behavior with local routing."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
PRIMARY = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
SECONDARY = (ROOT / "Ref/dhxj/RagEffect.cpp").read_text(encoding="latin-1")
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


def compact(source):
    return re.sub(r"\s+", "", source)


pe_process = function_body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
branch = re.search(r"case 488:[\s\S]*?CRagEffect::FlowerCasting\(this, aEffectRingBlue, 4, 1\);\s*break;", pe_process)
if branch is None:
    raise AssertionError("PE effect488 frame/sound/body-light/two-cast branch missing")
for token in (
    "this->m_stateCnt == 5", "PlayWave(aEffectT_13, xn, 0.0, zn, 250, 40, 1.0)",
    "m_BodyLight & 0x800", "m_BodyLight += 2048", "v32 >= 200", "v32 < 400",
    "m_BodySin5 = -112 - LOBYTE(this->m_stateCnt)",
    "m_BodySin5 = this->m_stateCnt", "FlowerCasting(this, aEffectRingBlue, 4, 0)",
):
    if token not in branch.group(0):
        raise AssertionError(f"PE effect488 branch missing {token}")

secondary = re.search(r"case\s+EF_FLOWERCAST\s*:[\s\S]*?FlowerCasting\(\"effect\\\\ring_blue.tga\",4,1\);", SECONDARY)
if secondary is None:
    raise AssertionError("secondary Ref FlowerCast branch missing")
for token in ("m_stateCnt == 5", "FlowerCasting(\"effect\\\\ring_blue.tga\",4,0)", "FlowerCasting(\"effect\\\\ring_blue.tga\",4,1)"):
    if token not in secondary.group(0):
        raise AssertionError(f"secondary Ref FlowerCast branch missing {token}")
if "case 488" in PRIMARY or "case 0x1E8" in PRIMARY:
    raise AssertionError("primary Ref gained effect488 case; update source-version provenance")

if "case 488: return ProcessEffectFrameLegacy();" not in ROUTE:
    raise AssertionError("local effect488 dispatcher route missing")
local_process = function_body(LOCAL, "bool CRagEffect::ProcessEffectFrameLegacy()")
if "const bool flowerCast = dispatchHandler && m_type == 488;" not in local_process:
    raise AssertionError("local effect488 flower-cast selector missing")
local_branch = re.search(r"\} else if \(flowerCast\) \{([\s\S]*?)\n    \} else if \(defender\)", local_process)
if local_branch is None:
    raise AssertionError("local effect488 inline handler branch missing")
for token in (
    "m_stateCnt == 5", 'TryPlayEffectWaveAt(wavePos, "effect\\\\T_\\xBE\\xC8\\xB6\\xF4\\xC7\\xD1\\xB8\\xB6\\xB9\\xFD.wav")',
    "master->m_BodyLight & 0x800", "master->m_BodyLight += 2048", "m_stateCnt >= 200",
    "m_stateCnt < 400", "-112 - (m_stateCnt & 0xFF)",
    'ApplyFlowerCasting("effect\\\\ring_blue.tga", 4.0f, 0)',
    'ApplyFlowerCasting("effect\\\\ring_blue.tga", 4.0f, 1)',
):
    if token not in local_branch.group(1):
        raise AssertionError(f"local effect488 handler missing {token}")

image = ExeImage(ROOT / "HighPriest.exe")
texture = image.read_cstring_at(0x71943C)
if texture is None or texture.raw != b"effect\\ring_blue.tga":
    raise AssertionError("FlowerCast ring-blue texture bytes differ at PE VA 0x71943C")

# PE's IDA symbol is only the effect\\T_ prefix; the executable's resolved suffix remains ambiguous.
if 'char aEffectT_13[] = "effect\\\\T_";' not in PE:
    raise AssertionError("PE FlowerCast wave symbol prefix changed")
print("effect488 FlowerCast parity passed; PE/secondary Ref call order, local cue/light/casts and exact texture; wave suffix remains unresolved")
