#!/usr/bin/env python3
"""Compare Effect302 Demonstration, exact sprite resources, and skill177 launch."""
from __future__ import annotations
import re
import sys
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
refskill=(ROOT/"Ref/Skill.cpp").read_text(encoding="utf-8",errors="replace")
local=(ROOT/"src/world/RagEffect.cpp").read_text(encoding="utf-8",errors="replace")
local_skill=(ROOT/"src/world/SkillUnit.cpp").read_text(encoding="utf-8",errors="replace")
dispatch=(ROOT/"src/world/RagEffectOnProcess2Cases.generated.inc").read_text(encoding="utf-8")
generator=(ROOT/"tools/generate_effect_dispatch.py").read_text(encoding="utf-8")
pe2=function(pe,"unsigned __int8 __thiscall CRagEffect::OnProcess2(CRagEffect *this)")
refon=function(ref,"unsigned __int8 __thiscall CRagEffect::OnProcess2(CRagEffect *this)")
branches=[]
for body,label in ((pe2,"PE"),(refon,"Ref")):
    a=body.index("case 0x12E:"); b=body.index("case 0x12F:",a); branch=compact(body[a:b])
    require("CRagEffect::Demonstration(this);return1" in branch,f"{label} effect302 caller branch mismatch")
    branches.append(branch)
require(branches[0]==branches[1],"PE and Ref effect302 callers differ")
pe_helper=function(pe,"void __thiscall CRagEffect::Demonstration(CRagEffect *this)")
ref_helper=function(ref2,"void __thiscall CRagEffect::Demonstration(CRagEffect *this)")
require(compact(pe_helper)==compact(ref_helper),"PE and Ref Demonstration helper bodies differ")
for token in ("m_stateCnt","m_pos=this->m_master->m_pos","PP_BOTTOMSPR","m_duration",
              "life=1","process=0","max_height=0.0","m_size=1.2","m_alpha=200",
              "m_animSpeed=4","SetAction(g_prim,0,16,0)","InitSegment(g_prim)","i=180;i<720","i+=180","m_latiSpeed+i"):
    require(token in compact(pe_helper),f"Demonstration source helper missing {token}")
exe=ExeImage()
for va,raw in ((0x71CBD8,b"\xC0\xCC\xC6\xD1\xC6\xAE\\\xB5\xA5\xB8\xF3\xBD\xBA\xC6\xAE\xB7\xB9\xC0\xCC\xBC\xC7.spr"),
               (0x71CBBC,b"\xC0\xCC\xC6\xD1\xC6\xAE\\\xB5\xA5\xB8\xF3\xBD\xBA\xC6\xAE\xB7\xB9\xC0\xCC\xBC\xC7.act")):
    got=exe.read_cstring_at(va)
    require(got is not None and got.raw==raw,f"effect302 asset bytes differ at 0x{va:X}")
require("case 177" in refskill and "302" in refskill[refskill.index("case 177"):refskill.index("case 178")],
        "Ref skill177 loop effect302 launch missing")
require("case 177" in local_skill and "LaunchLoop(302)" in local_skill[local_skill.index("case 177"):local_skill.index("case 178")],
        "local skill177 loop effect302 launch missing")
require("case 302: return ProcessDemonstration302();" in dispatch,"effect302 route missing")
require('302: "return ProcessDemonstration302();"' in generator and "FORCE_ROUTE.add(302)" in generator,
        "effect302 generator route missing")
spawn=function(local,"void CRagEffect::SpawnDemonstration302()")
for token in ("PP_BOTTOMSPR","prim->m_duration=m_duration","prim->m_size=1.2f","gi.life=1","gi.process=0",
              "gi.maxHeight=0.0f","gi.vecBPreX=m_effectPos.x","gi.arcBaselineY=m_effectPos.y-1.0f",
              "gi.vecBPreZ=m_effectPos.z","gi.flag[4]=0","prim->m_alpha=200.0f","SetParticleResources(",
              "prim->m_animSpeed=4","prim->m_spriteAction=0","prim->m_spriteMotionBase=16","InitReferenceSegments(prim)","lane<4"):
    require(token in compact(spawn),f"local Demonstration emitter mapping missing: {token}")
print("effect302 PE/Ref branch/helper, skill177 selector, executable assets, route, and local payload passed")

