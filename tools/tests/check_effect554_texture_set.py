#!/usr/bin/env python3
"""Compare effect554's selector-2 EffectTextureSet route and emitter profile."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage, SymbolTable  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
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


for label, source in (("PE", PE), ("primary Ref", REF)):
    body = function_body(source, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
    match = re.search(r"case 0x22A:(.*?)(?=\n\s*case |\n\s*default:|\Z)", body, re.S)
    if match is None or not re.search(
            r"EffectTextureSet\(this,\s*aEffectFreezeAB,\s*2\);\s*return 1;", match.group(1)):
        raise AssertionError(f"{label} effect554 caller changed")

if 'case 554: ApplyEffectTexture("effect\\\\freeze_a.bmp", 2); return true;' not in ROUTE:
    raise AssertionError("local effect554 generated route changed")
if 'm_type == 554) {\n        ApplyEffectTexture("effect\\\\freeze_a.bmp", 2);' not in LOCAL:
    raise AssertionError("local effect554 selector-2 mapping changed")

symbols = SymbolTable(ExeImage())
resource = symbols.resolve_name("aEffectFreezeAB")
if resource is None or resource.raw != b"effect\\freeze_a.bmp":
    raise AssertionError("PE freeze_a resource bytes changed")

texture = function_body(LOCAL, "void CRagEffect::ApplyEffectTexture(const char* textureName, int variant)")
for token in (
    "if (m_stateCnt != 0)", "LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeEffectTexture)",
    "prim->m_renderFlag = 5", "prim->m_duration = m_duration", "gi.life = 1", "gi.process = 0",
    "if (variant == 2)", "gi.distance = 10.0f", "gi.arcBaselineY = origin.y - 10.0f",
    "gi.flag[2] = 2", "gi.height[10] = 0.0f", "gi.fullDisplayAngle = 135",
    "gi.height[8] = 0.0f", "gi.alphaB = 0", "gi.sparkX = origin.x", "gi.sparkZ = origin.z",
    "gi.rotStart = static_cast<short>(rand() % 360)", "gi.flag[4] = static_cast<u8>(variant",
):
    if token not in texture:
        raise AssertionError(f"local selector-2 EffectTextureSet profile missing {token}")

print("effect554 parity passed: PE/Ref caller, local route, freeze_a bytes and selector-2 emitter profile")
