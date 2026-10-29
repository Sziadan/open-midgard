#!/usr/bin/env python3
"""Compare the effect552 SMA2 caller/helper against PE, Ref, and local route."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage, SymbolTable  # noqa: E402


def require(ok: bool, message: str) -> None:
    if not ok:
        raise SystemExit(message)


def function(source: str, signature: str) -> str:
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{": depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[start:pos + 1]
    raise AssertionError(signature)


def compact(source: str) -> str:
    return re.sub(r"\s+", "", source)


pe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
ref = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
ref2 = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
src = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
dispatch = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")
generator = (ROOT / "tools/generate_effect_dispatch.py").read_text(encoding="utf-8")

pe_on = function(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
ref_on = function(ref, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
for source, label in ((pe_on, "PE"), (ref_on, "Ref")):
    start = source.index("case 0x228:")
    end = source.index("case 0x229:", start)
    branch = compact(source[start:end])
    require("CRagEffect::SMA2(this,aEffectRingBlue)" in branch and "return1;" in branch,
            f"{label} effect552 OnProcess3 caller mismatch")
require(compact(pe_on[pe_on.index("case 0x228:"):pe_on.index("case 0x229:", pe_on.index("case 0x228:"))])
        == compact(ref_on[ref_on.index("case 0x228:"):ref_on.index("case 0x229:", ref_on.index("case 0x228:"))]),
        "PE and Ref effect552 inline caller branches differ")

pe_helper = function(pe, "void __thiscall CRagEffect::SMA2(CRagEffect *this, char *tName)")
ref_helper = function(ref2, "void __thiscall CRagEffect::SMA2(CRagEffect *this, char *tName)")
require(compact(pe_helper) == compact(ref_helper), "PE and Ref SMA2 helper bodies differ")
for token in ("PP_SMA2", "m_renderFlag=5", "m_size=4.0", "m_deltaPos=this->m_deltaPos",
              "m_deltaPos2=this->m_deltaPos2", "m_GI[0].max_height=13.0",
              "m_GI[1].max_height=11.0", "m_GI[2].max_height=9.0",
              "m_GI[3].life=0", "v4<21"):
    require(token in compact(pe_helper), f"SMA2 PE/Ref helper behavior missing: {token}")

symbol = SymbolTable(ExeImage()).resolve_name("aEffectRingBlue")
require(symbol is not None and symbol.text == "effect\\ring_blue.tga", "PE exact ring-blue resource mismatch")
require("case 552: return ProcessSma2Effect552();" in dispatch, "effect552 generated route missing")
require('552: "return ProcessSma2Effect552();"' in generator, "effect552 generator route missing")

local = compact(function(src, "void CRagEffect::SpawnSma2()"))
for token in ('LaunchEffectPrim(PP_SMA2,vector3d{})', '"effect\\\\ring_blue.tga"',
              "prim->m_renderFlag=5", "prim->m_duration=m_duration", "prim->m_size=4.0f",
              "prim->m_deltaPos=m_deltaPos", "prim->m_deltaPos2=m_deltaPos2",
              "m_stateCnt!=0", "zeroDeltaY?3.4000001f:2.4000001f",
              "zeroDeltaY?3.5999999f:2.5999999f", "zeroDeltaY?3.8f:2.8f",
              "prim->m_gi[3].life=0"):
    require(token in local, f"local SMA2 emitter missing behavior: {token}")
handler = compact(function(src, "bool CRagEffect::ProcessSma2Effect552()"))
require(handler == "boolCRagEffect::ProcessSma2Effect552(){SpawnSma2();returntrue;}",
        "local effect552 handler is not a direct SMA2 wrapper")
print("effect552 PE/Ref caller and helper, exact resource, local route, and SMA2 emitter passed")
