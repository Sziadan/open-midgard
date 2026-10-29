#!/usr/bin/env python3
"""Compare PE-only effect405 Bottom_Magnus variant2 with shared helper/local route."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
REF2 = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="latin-1")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
ROUTE = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
LIFECYCLE = (ROOT / "tools/tests/EffectLifecycleTests.cpp").read_text(encoding="utf-8")


def function_body(source, signature):
    for match in reversed(list(re.finditer(re.escape(signature), source))):
        start = match.start()
        opening = source.find("{", start + len(signature))
        if opening < 0 or source.find(";", start + len(signature), opening) >= 0:
            continue
        depth = 0
        for pos in range(opening, len(source)):
            if source[pos] == "{":
                depth += 1
            elif source[pos] == "}":
                depth -= 1
                if depth == 0:
                    return source[opening + 1:pos]
    raise AssertionError(f"function body not found: {signature}")


def compact(value):
    return re.sub(r"\s+", "", value)


caller = function_body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
if not re.search(r"case 405:\s*CRagEffect::Bottom_Magnus\(this, aEffectRingWhit, 2\);\s*break;", caller):
    raise AssertionError("PE effect405 Bottom_Magnus caller changed")
if re.search(r"case (?:405|0x195):", REF):
    raise AssertionError("primary Ref added a numeric effect405 caller; update provenance")

signature = "void __thiscall CRagEffect::Bottom_Magnus(CRagEffect *this, char *tName, char F1)"
pe_helper = function_body(PE, signature)
ref_helper = function_body(REF2, signature)
if compact(pe_helper) != compact(ref_helper):
    raise AssertionError("PE and secondary-Ref Bottom_Magnus helper bodies differ")
for label, body in (("PE", pe_helper), ("secondary Ref", ref_helper)):
    for token in ("PP_RECT_UP", "m_renderFlag", "m_duration", "m_size = 1.0",
                  "m_GI[0].full_display_angle = rand() % 360", "F1 == 2",
                  "m_GI[0].max_height = 32.0", "m_GI[0].alphaB = 30",
                  "m_GI[0].alphaT = 29999", "m_GI[0].height[0] = v5",
                  "m_GI[0].height[1] = v5", "m_GI[0].height[2] = 0.0",
                  "m_GI[0].height[3] = F1a", "m_GI[1].life = 0"):
        if token not in body:
            raise AssertionError(f"{label} Bottom_Magnus helper missing {token}")

if 'case 405: ApplyBottomMagnus("effect\\\\ring_white.tga", 2); return true;' not in ROUTE:
    raise AssertionError("local effect405 generated route changed")
local = function_body(LOCAL, "void CRagEffect::ApplyBottomMagnus(const char* textureName, int variant)")
for token in (
    "m_stateCnt != 0", "LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeRectUp)",
    "prim->m_renderFlag = 5", "prim->m_duration = m_duration", "prim->m_size = 1.0f",
    "gi.fullDisplayAngle = static_cast<short>(rand() % 360)", "variant == 2",
    "gi.maxHeight = 32.0f", "gi.alphaB = 30", "gi.alphaT = 29999",
    "gi.height[0] = 2.5f", "gi.height[1] = 2.5f", "gi.height[2] = 0.0f",
    "gi.height[3] = static_cast<float>(variant)", "prim->m_gi[1].life = 0",
):
    if token not in local:
        raise AssertionError(f"local Bottom_Magnus variant2 profile missing {token}")

image = ExeImage(ROOT / "HighPriest.exe")
resource = image.read_cstring_at(0x719480)
if resource is None or resource.raw != b"effect\\ring_white.tga":
    raise AssertionError("PE effect405 ring-white resource bytes changed")

if 'effect.ApplyBottomMagnus("effect\\\\ring_white.tga",2)' not in LIFECYCLE:
    raise AssertionError("effect405 variant2 lifecycle fixture does not invoke the production helper")
for token in (
    "effect405 variant2 emits one ring-white RectUp at master origin",
    "effect405 variant2 preserves the PE RectUp GI profile and disabled bands",
    "effect405 null-texture and nonzero-state gates suppress launch",
):
    if token not in LIFECYCLE:
        raise AssertionError(f"effect405 lifecycle assertion missing: {token}")

print("effect405 parity passed: PE-only caller provenance, PE/Ref helper, local variant2 route/profile and exact resource")
