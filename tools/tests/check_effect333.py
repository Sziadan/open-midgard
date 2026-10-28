#!/usr/bin/env python3
"""Compare Effect333 Maple callers, RNG/resource payload, and local route."""
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
local=(ROOT/"src/world/RagEffect.cpp").read_text(encoding="utf-8",errors="replace")
branch_pattern=r"case\s+0x14D\s*:\s*CRagEffect::Maple\(this\);\s*return\s+1\s*;"
branches=[re.search(branch_pattern,s).group(0) if re.search(branch_pattern,s) else "" for s in (pe,ref)]
require(all(branches),"Maple case 0x14D missing in PE/Ref")
require(compact(branches[0])==compact(branches[1]),"PE/Ref Maple callers differ")
pe_helper=function(pe,"void __thiscall CRagEffect::Maple(CRagEffect *this)")
ref_helper=function(ref2,"void __thiscall CRagEffect::Maple(CRagEffect *this)")
require(compact(pe_helper)==compact(ref_helper),"PE and Ref Maple helper bodies differ")
for token in ("m_stateCnt==99999998","m_duration=99999999","m_stateCnt=0","PP_SAKURA",
              "rand()%4","rand()%6","rand()%300","rand()%360","m_GI[0].height[3]",
              "flag1[0]=1","m_alpha=200.0","SetActName","SetSprName","SetAction(g_prim,0,36,0)"):
    require(token in compact(pe_helper),f"Maple helper missing {token}")
im=ExeImage(ROOT/"HighPriest.exe")
for va,raw in ((0x71BFDC,bytes.fromhex("c0ccc6d1c6ae5cb4dcc7b32e616374")),
               (0x71BFCC,bytes.fromhex("c0ccc6d1c6ae5cb4dcc7b32e737072"))):
    got=im.read_cstring_at(va)
    require(got is not None and got.raw==raw,f"Maple executable asset bytes differ at 0x{va:X}")
spawn=function(local,"void CRagEffect::SpawnMaple333()")
ordered=("m_stateCnt==99999998","m_duration=99999999","m_stateCnt=0","PP_SAKURA",
         "maxHeightRoll=rand()%4","sizeRoll=rand()%6","xRoll=rand()%300","height[0]=static_cast<float>(rand()%360)",
         "height[1]=static_cast<float>(rand()%360)","height[2]=static_cast<float>(rand()%360)","height[3]=static_cast<float>(rand()%360)",
         "zRoll=rand()%300","flag[0]=1","m_alpha=200.0f","SetParticleResources(","m_spriteAction=0","m_spriteMotionBase=36","InitReferenceSegments(prim)")
compact_spawn=compact(spawn)
positions=[compact_spawn.find(token) for token in ordered]
require(all(pos>=0 for pos in positions),"local Maple emitter missing a required payload operation")
require(positions==sorted(positions),"local Maple RNG/payload order differs from PE/Ref")
require("case 333: return ProcessMaple333();" in (ROOT/"src/world/RagEffectOnProcess2Cases.generated.inc").read_text(),"effect333 route missing")
gen=(ROOT/"tools/generate_effect_dispatch.py").read_text()
require("FORCE_ROUTE.add(333)" in gen and '333: "return ProcessMaple333();"' in gen,"effect333 generator route missing")
print("effect333 PE/Ref caller/helper, executable assets, ordered RNG payload, and route passed")
