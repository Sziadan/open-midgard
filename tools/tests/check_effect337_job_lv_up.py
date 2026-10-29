#!/usr/bin/env python3
"""Compare effect337 JobLvUp50 helper, caller order, and resource bytes."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage  # noqa: E402

pe = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
ref = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="latin-1")
local = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
route = (ROOT / "src/world/RagEffectOnProcess2Cases.generated.inc").read_text(encoding="utf-8")


def function_body(source, signature):
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 0
    for position in range(opening, len(source)):
        if source[position] == "{":
            depth += 1
        elif source[position] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:position]
    raise AssertionError(f"unterminated function {signature}")


def compact(source):
    return re.sub(r"\s+", "", source)


signature = "unsigned __int8 __thiscall CRagEffect::JobLvUp50(CRagEffect *this, int time)"
pe_helper = function_body(pe, signature)
ref_helper = function_body(ref, signature)
if compact(pe_helper) != compact(ref_helper):
    raise AssertionError("PE and primary-Ref JobLvUp50 helper bodies differ")
if "CRagEffect::JobLvUp50(this, 0);" not in pe or "CRagEffect::JobLvUp50(this, 45);" not in pe:
    raise AssertionError("canonical effect337 helper call sequence changed")
if "case 337: ApplyJobLvUp50(0); ApplyJobLvUp50(45); return true;" not in route:
    raise AssertionError("local effect337 route or two-call ordering changed")

local_helper = function_body(local, "bool CRagEffect::ApplyJobLvUp50(int time)")
for token in (
    "m_stateCnt == 5", "LaunchEffectPrim(PP_SLASH1, vector3d{})", "prim->m_renderFlag = 5",
    "prim->m_duration = m_duration", r'GetTexture("effect\\magic_green.tga", false)',
    "prim->m_size = 9.0f", "band < 4", "gi.life = 1", "gi.maxHeight = 2.0f",
    "gi.rotStart = static_cast<short>(time + band * 90)", "gi.distance = 2.0f",
    "gi.height[0] = 1.0f", "gi.flag[0] = 0", "ProcessEZ2STR(0.0f, 0.0f, 0.0f)",
):
    if token not in local_helper:
        raise AssertionError(f"local JobLvUp50 payload missing {token}")

resource = ExeImage(ROOT / "HighPriest.exe").read_cstring_at(0x71A730)
if resource is None or resource.raw != b"effect\\magic_green.tga":
    raise AssertionError("JobLvUp50 texture bytes differ at PE VA 0x71A730")

print("effect337 JobLvUp50 parity passed: PE/Ref helper, local caller order/payload and exact PE texture bytes")
