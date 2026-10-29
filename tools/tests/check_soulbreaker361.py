#!/usr/bin/env python3
"""Compare the PE/Ref SoulBreaker effect361 caller/helper and local port."""
from __future__ import annotations
import re, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/"tools"))
from ro_exe_resolve import ExeImage

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
ref2=(ROOT/"Ref/RagEffect2.cpp").read_text(encoding="utf-8",errors="replace")
src=(ROOT/"src/world/RagEffect.cpp").read_text(encoding="utf-8",errors="replace")
dispatch=(ROOT/"src/world/RagEffectOnProcess2Cases.generated.inc").read_text(encoding="utf-8")
generator=(ROOT/"tools/generate_effect_dispatch.py").read_text(encoding="utf-8")
pe2=function(pe,"unsigned __int8 __thiscall CRagEffect::OnProcess2(CRagEffect *this)")
ref_on=function(ref,"unsigned __int8 __thiscall CRagEffect::OnProcess2(CRagEffect *this)")
branches=[]
for body,label in ((pe2,"PE"),(ref_on,"Ref")):
    a=body.index("case 0x169:"); b=body.index("case 0x16A:",a); branch=compact(body[a:b])
    require("m_stateCnt==20" in branch and "this->m_pos=this->m_master->m_pos" in branch
        and "PlayWave(aEffect_93,xd,0.0,zd,250,40,1.0)" in branch
        and "m_stateCnt<=0xAu" in branch and "m_BodyLight&1" in branch
        and "SetArgb(this->m_master,-1,255,0,255)" in branch
        and "CRagEffect::SoulBreaker(this);return1" in branch,
        f"{label} effect361 branch missing source behavior")
    branches.append(branch)
require(branches[0]==branches[1],"PE and Ref effect361 branches differ")
pehelper=function(pe,"void __thiscall CRagEffect::SoulBreaker(CRagEffect *this)")
refhelper=function(ref2,"void __thiscall CRagEffect::SoulBreaker(CRagEffect *this)")
require(compact(pehelper)==compact(refhelper),"PE and Ref SoulBreaker helpers differ")
for token in ("m_stateCnt","PP_SOULBREAKER","aEffectPurplesl","m_renderFlag=5","m_duration",
              "m_deltaPos","m_size=0.0","rand()%5","m_pos.y-10.0"):
    require(token in compact(pehelper),f"SoulBreaker source helper missing {token}")
exe=ExeImage()
require(exe.read_cstring_at(0x6F1FF0).raw==b"effect\\", "effect361 wave prefix bytes mismatch")
require(exe.read_cstring_at(0x71CA60).raw==b"effect\\purpleslash.tga", "SoulBreaker texture bytes mismatch")
require("case 361: return ProcessSoulBreaker361Effect();" in dispatch,"effect361 route missing")
require('361: "return ProcessSoulBreaker361Effect();"' in generator and "FORCE_ROUTE.add(361)" in generator,
        "effect361 generator route missing")
spawn=function(src,"void CRagEffect::SpawnSoulBreaker361()")
caller=function(src,"bool CRagEffect::ProcessSoulBreaker361Effect()")
for token in ("PP_SOULBREAKER","g_texMgr.GetTexture(\"effect\\\\purpleslash.tga\"", "m_renderFlag=5",
              "gi.process=static_cast<int>(lane)-28", "rand()%5", "gi.sparkY=m_effectPos.y-10.0f",
              "gi.height[1]=0.80000001f", "gi.flag[0]=0"):
    require(token in compact(spawn),f"local SoulBreaker emitter missing {token}")
for token in ('m_stateCnt==20','PlayWave("effect\\\\"','m_stateCnt<=10','SetArgb(-1,255,0,255)',
              'SpawnSoulBreaker361();returntrue'):
    require(token in compact(caller),f"local effect361 caller missing {token}")
print("effect361 PE/Ref case/helper, blocked wave prefix, texture, route, and local field mapping passed")
