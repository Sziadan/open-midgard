#!/usr/bin/env python3
"""Compare effect599 inline state, wave, tint, and BodyLight branches."""
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
src = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
dispatch = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")
generator = (ROOT / "tools/generate_effect_dispatch.py").read_text(encoding="utf-8")

pe_on = function(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
ref_on = function(ref, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
branches = []
for body, label in ((pe_on, "PE"), (ref_on, "Ref")):
    start = body.index("case 0x257:")
    end = body.index("case 0x258:", start)
    branch = compact(body[start:end])
    require("this->m_stateCnt==5" in branch and "PlayWave(aEffectEfBegins,xx,0.0,zx,250,40,1.0)" in branch
            and "v73<5||v73>=26" in branch and "v74=(this->m_stateCnt&1)==0" in branch
            and "++v71->m_BodyLight" in branch and "SetArgb(this->m_master,-1,250,250,100)" in branch
            and "gotoLABEL_206" in branch and "return1;" in branch,
            f"{label} effect599 inline branch missing a source behavior")
    branches.append(branch)
require(branches[0] == branches[1], "PE and Ref effect599 inline branches differ")
require("LABEL_206:" in compact(pe_on)
        and "SetArgb(this->m_master,-1,255,255,255)" in compact(pe_on),
        "effect599 shared odd-state white tint label missing")

resolved = SymbolTable(ExeImage()).resolve_name("aEffectEfBegins")
require(resolved is not None and resolved.text == "effect\\EF_BeginSpell.wav",
        "effect599 exact wave bytes mismatch")
require("case 599: return ProcessEffect599();" in dispatch, "effect599 generated route missing")
require('599: "return ProcessEffect599();"' in generator, "effect599 generator route missing")

local = compact(function(src, "bool CRagEffect::ProcessEffect599()"))
ordered = ("if(m_stateCnt==5)", 'PlayWave("effect\\\\EF_BeginSpell.wav"',
           "m_effectPos.x-listener.x", "m_effectPos.z-listener.z", "if(m_stateCnt<5||m_stateCnt>=26||master==nullptr)returntrue",
           "(m_stateCnt&1)==0", "master->m_BodyLight&1", "++master->m_BodyLight",
           "SetArgb(-1,250,250,100)", "SetArgb(-1,255,255,255)", "returntrue")
cursor = 0
for token in ordered:
    pos = local.find(token, cursor)
    require(pos >= 0, f"local effect599 branch or operation order missing: {token}")
    cursor = pos + len(token)
print("effect599 complete PE/Ref branch, exact wave, route, BodyLight, and tint checks passed")
