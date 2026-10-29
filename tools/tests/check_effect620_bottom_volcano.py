#!/usr/bin/env python3
"""Compare effect620 Bottom_Volcano helper, route, emitter, and lifecycle behavior."""
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
ROUTE = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")
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


caller_signature = "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)"
for label, source in (("PE", PE), ("primary Ref", REF)):
    caller = function_body(source, caller_signature)
    if not re.search(r"case 0x26C:\s*CRagEffect::Bottom_Volcano\(this, aEffectRingBlue, 14, 0\.0, 0\.0\);\s*return 1;", caller):
        raise AssertionError(f"{label} effect620 Bottom_Volcano caller changed")

helper_signature = "void __thiscall CRagEffect::Bottom_Volcano(CRagEffect *this, char *tName, char PW, float x, float z)"
pe_helper = function_body(PE, helper_signature)
ref_helper = function_body(REF2, helper_signature)
if compact(pe_helper) != compact(ref_helper):
    raise AssertionError("PE and primary-Ref Bottom_Volcano helper bodies differ")
for token in (
    "this->m_pos = this->m_master->m_pos", "if (!this->m_stateCnt)",
    "LaunchEffectPrim(this, PP_GI_4", "m_renderFlag = 5", "m_duration = this->m_duration",
    "m_size = (float)PW", "m_GI[0].life = 1", "m_GI[0].process = 0",
    "m_GI[0].full_display_angle = 360", "m_GI[0].max_height = 15.0",
    "rand() % 360", "rand() % 100", "distance = 2.5999999", "rise_angle = 80",
):
    if compact(token) not in compact(pe_helper):
        raise AssertionError(f"PE Bottom_Volcano helper missing {token}")

if 'case 620: ApplyBottomVolcano("effect\\\\ring_blue.tga", 14.0f); return true;' not in ROUTE:
    raise AssertionError("local effect620 generated route changed")
local = function_body(LOCAL, "void CRagEffect::ApplyBottomVolcano(const char* textureName, float size)")
ordered = (
    "if (textureName == nullptr)", "ResolveCurrentMaster()", "m_effectPos = origin",
    "m_cachedPos = origin", "if (m_stateCnt != 0)", "LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeGi4)",
    "prim->m_renderFlag = 5", "prim->m_duration = m_duration", "prim->m_size = size",
    "gi.life = 1", "gi.process = 0", "gi.fullDisplayAngle = 360", "gi.maxHeight = 15.0f",
    "rand() % 360", "rand() % 100", "gi.distance = 2.5999999f", "gi.riseAngle = 80",
)
cursor = -1
for token in ordered:
    cursor = local.find(token, cursor + 1)
    if cursor < 0:
        raise AssertionError(f"local Bottom_Volcano operation missing or reordered: {token}")

image = ExeImage(ROOT / "HighPriest.exe")
resource = image.read_cstring_at(0x71943C)
if resource is None or resource.raw != b"effect\\ring_blue.tga":
    raise AssertionError("effect620 PE ring-blue bytes changed")

fixture = re.search(r"effect\.m_type=620;(.*?)(?=\n    \})", LIFECYCLE, re.S)
if fixture is None:
    raise AssertionError("effect620 lifecycle fixture missing")
for token in ("ApplyBottomVolcano", "beforeFrames+2", "effect.m_effectPos.x==22",
              "rngDraws==beforeFrames+2", "g_texMgr.requests.size()==1"):
    if token not in fixture.group(1):
        raise AssertionError(f"effect620 lifecycle fixture missing {token}")

print("effect620 Bottom_Volcano parity passed: PE/Ref callers and helper, refreshed local route, exact resource and lifecycle profile")
