#!/usr/bin/env python3
"""Compare effect590 JobLvUp50_2 caller and two ordered emitters."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
REF_HELPER = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="latin-1")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
ROUTES = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")


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
    start = body.index("case 0x24E:")
    return body[start:body.index("case 0x250:", start)]


pe_caller, ref_caller = caller(PE), caller(REF)
if compact(pe_caller) != compact(ref_caller):
    raise AssertionError("PE and primary-Ref effect590 callers differ")
pe_compact = compact(pe_caller)
if not all(token in pe_compact for token in ("m_stateCnt!=15", "return1;", "JobLvUp50_2(this,0);", "JobLvUp50_2(this,45);")):
    raise AssertionError("effect590 frame gate or 0/45 caller order changed")

pe_helper = function(PE, "unsigned __int8 __thiscall CRagEffect::JobLvUp50_2(CRagEffect *this, __int16 time)")
ref_helper = function(REF_HELPER, "unsigned __int8 __thiscall CRagEffect::JobLvUp50_2(CRagEffect *this, __int16 time)")
if compact(pe_helper) != compact(ref_helper):
    raise AssertionError("PE and primary-Ref JobLvUp50_2 helper bodies differ")

if "case 590: ProcessJobLvUp50_2(); return true;" not in ROUTES:
    raise AssertionError("local effect590 route changed")
local_process = function(LOCAL, "void CRagEffect::ProcessJobLvUp50_2()")
if not re.search(r"m_stateCnt == 15\).*?ApplyJobLvUp50_2\(0\);\s*ApplyJobLvUp50_2\(45\);", local_process, re.S):
    raise AssertionError("local effect590 frame gate or helper order changed")
local_emitter = function(LOCAL, "bool CRagEffect::ApplyJobLvUp50_2(short time)")
for token in ("PP_SLASH1", '"effect\\\\magic_blue.tga"', "prim->m_renderFlag = 5",
              "prim->m_duration = m_duration", "prim->m_size = 4.0f", "band < 4",
              "gi.maxHeight = 1.5f", "gi.rotStart = static_cast<short>(time + band * 90)"):
    if token not in local_emitter:
        raise AssertionError(f"local JobLvUp50_2 emitter profile missing {token}")

print("effect590 JobLvUp50_2 parity passed: PE/Ref gate and helper, local route, ordered variants and profile")
