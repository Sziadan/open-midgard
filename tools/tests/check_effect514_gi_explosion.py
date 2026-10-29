#!/usr/bin/env python3
"""Compare effect514 GI_EXPLOSION caller/helper and local four-band setup."""
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
ROUTE = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")


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


for source_name, source in (("PE", PE), ("primary Ref", REF_CALLER)):
    onprocess3 = function_body(source, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
    if not re.search(r"case 0x202:\s*CRagEffect::GI_EXPLOSION\(this, aEffectRingBlue\);\s*return 1;", onprocess3):
        raise AssertionError(f"{source_name} effect514 GI_EXPLOSION caller missing")

pe_helper = function_body(PE, "void __thiscall CRagEffect::GI_EXPLOSION(CRagEffect *this, char *tName)")
ref_helper = function_body(REF_HELPER, "void __thiscall CRagEffect::GI_EXPLOSION(CRagEffect *this, char *tName)")
if compact(pe_helper) != compact(ref_helper):
    raise AssertionError("PE and primary-Ref GI_EXPLOSION helper bodies differ")

if 'case 514: ApplyGiExplosion("effect\\\\ring_blue.tga"); return true;' not in ROUTE:
    raise AssertionError("local effect514 OnProcess3 route/texture changed")
local_helper = function_body(LOCAL, "void CRagEffect::ApplyGiExplosion(const char* textureName)")
for token in (
    "m_stateCnt != 0", "kPrimTypeGiExplosion", "prim->m_renderFlag = 5", "prim->m_duration = m_duration",
    "prim->m_size = 4.0f", "GetTexture(textureName, false)", "band < 4", "gi.life = 1",
    "gi.process = 0", "gi.fullDisplayAngle = 315", "gi.maxHeight = 15.0f",
    "rand() % 360", "gi.alphaB = 0", "20.0f - static_cast<float>(band) * 0.5f",
    "gi.riseAngle = 50", "gi.height.fill(0.0f)", "gi.flag.fill(0)",
):
    if token not in local_helper:
        raise AssertionError(f"local GI_EXPLOSION payload missing {token}")

image = ExeImage(ROOT / "HighPriest.exe")
texture = image.read_cstring_at(0x71943C)
if texture is None or texture.raw != b"effect\\ring_blue.tga":
    raise AssertionError("GI_EXPLOSION ring-blue texture differs at PE VA 0x71943C")

print("effect514 GI_EXPLOSION parity passed: PE/Ref callers and helper, local route/four-band payload, exact texture")
