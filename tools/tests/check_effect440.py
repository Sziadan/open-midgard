#!/usr/bin/env python3
"""Compare Effect440's PE frame-zero branch, partial wave, and Ref STR mapping."""
from __future__ import annotations
import re,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/"tools"))
from ro_exe_resolve import ExeImage,SymbolTable  # noqa: E402

def require(ok,msg):
    if not ok: raise SystemExit(msg)
def function(src,sig):
    start=src.rindex(sig); opening=src.index("{",start); depth=0
    for pos in range(opening,len(src)):
        if src[pos]=="{": depth+=1
        elif src[pos]=="}":
            depth-=1
            if depth==0:return src[start:pos+1]
    raise AssertionError(sig)
def compact(s): return re.sub(r"\s+","",s)
pe=(ROOT/"HighPriest.exe.c").read_text(encoding="utf-8",errors="replace")
ref=(ROOT/"Ref/RagEffect.cpp").read_text(encoding="utf-8",errors="replace")
src=(ROOT/"src/world/RagEffect.cpp").read_text(encoding="utf-8",errors="replace")
dispatch=(ROOT/"src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
generator=(ROOT/"tools/generate_effect_dispatch.py").read_text(encoding="utf-8")
on=function(pe,"unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
a=on.index("case 440:"); b=on.index("case 441:",a); branch=compact(on[a:b])
require("if(!this->m_stateCnt)" in branch and "this->m_pos=(*p_m_master)->m_pos" in branch
    and "PlayWave(aEffect_124,xbf,0.0,zbf,250,40,1.0)" in branch and "ProcessEZ2STR(this,0.0,-80.0,0.0)" in branch,
    "PE effect440 branch/common STR continuation mismatch")
assets=SymbolTable(ExeImage())
wave=assets.resolve_name("aEffect_124"); clip=assets.resolve_name("aAsumStr")
require(wave is not None and wave.raw==b"effect\\", "effect440 truncated wave prefix bytes mismatch")
require(clip is not None and clip.raw==b"asum.str", "Ref STR asset bytes mismatch")
require('case 440: return ProcessEffect440();' in dispatch,"effect440 route missing")
require('440: "return ProcessEffect440();"' in generator and "FORCE_ROUTE.add(440)" in generator,
    "effect440 generator route missing")
local=compact(function(src,"bool CRagEffect::ProcessEffect440()"))
ordered=("if(m_stateCnt==0&&master!=nullptr)","m_effectPos=master->m_pos",
    'PlayWave("effect\\\\"','m_effectPos.x-player->m_pos.x','m_effectPos.z-player->m_pos.z',
    "ProcessEZ2STR(0.0f,-80.0f,0.0f);returntrue")
positions=[local.find(token) for token in ordered]
require(all(p>=0 for p in positions) and positions==sorted(positions),"local effect440 branch order/STR mismatch")
strrow=next(line for line in (ROOT/"src/world/RagEffectStrNameTable.generated.inc").read_text(encoding="utf-8").splitlines() if "{ 440," in line)
require('"asum.str"' in strrow,"local Ref STR mapping mismatch")
print("effect440 PE state0 branch, truncated wave bytes, Ref asum.str, route and local operation order passed")
