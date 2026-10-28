#!/usr/bin/env python3
"""Compare Effect8 Warp caller/helper, exact assets and cadence payload."""
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
local=(ROOT/"src/world/RagEffect.cpp").read_text(encoding="utf-8",errors="replace")
require(re.search(r"case\s+8\s*:\s*CRagEffect::Warp\(this\);\s*break\s*;",pe) is not None,"PE OnProcess case8 Warp caller missing")
pe_helper=function(pe,"void __thiscall CRagEffect::Warp(CRagEffect *this)")
ref_helper=function(ref,"void __thiscall CRagEffect::Warp(CRagEffect *this)")
require(compact(pe_helper)==compact(ref_helper),"PE and Ref Warp helper bodies differ")
for token in ("m_stateCnt%20","PP_3DCIRCLE","m_renderFlag","m_radius=2.0","m_arcAngle=22.5","m_innerSize=10.0","m_latitude=90.0",
              "m_radiusSpeed=1.25","m_radiusAccel=g_prim->m_radiusSpeed/(double)g_prim->m_duration*-0.5","m_fadeOutCnt=g_prim->m_duration-g_prim->m_duration/2",
              "aEffectRingYell_0","m_stateCnt%2","PP_3DPARTICLEORBIT","rand()%200/100","rand()%360","rand()%240",
              "m_radius=v3*0.69999999+1.0","m_duration=40","m_longSpeed=rndNum*3.0+3.0","m_size=0.2",
              "byte_71B200","byte_71B1E8"):
    require(token in compact(pe_helper),f"Warp helper missing {token}")
im=ExeImage(ROOT/"HighPriest.exe")
for va,expected in ((0x71B254,b"EFFECT\\RING_YELLOW.TGA"),
                    (0x71B200,bytes.fromhex("c0ccc6d1c6ae5c7061727469636c65312e737072")),
                    (0x71B1E8,bytes.fromhex("c0ccc6d1c6ae5c7061727469636c65312e616374"))):
    actual=im.read_cstring_at(va)
    require(actual is not None and actual.raw==expected,f"Warp resource bytes differ at 0x{va:X}")
spawn=function(local,"void CRagEffect::SpawnWarp8()")
compact_spawn=compact(spawn)
ordered=("if((m_stateCnt%20)==0)","LaunchEffectPrim(PP_3DCIRCLE,vector3d{})","circle->m_radius=2.0f",
         "circle->m_radiusSpeed=1.25f","circle->m_fadeOutCnt=circle->m_duration-circle->m_duration/2",
         'GetTexture("EFFECT\\\\RING_YELLOW.TGA",false)',"if((m_stateCnt%2)==0)","LaunchEffectPrim(PP_3DPARTICLEORBIT,vector3d{})",
         "randomRadius=rand()%200/100","rand()%360","rand()%240","orbit->m_size=0.2f",
         '"\\xC0\\xCC\\xC6\\xD1\\xC6\\xAE\\\\particle1.spr"','"\\xC0\\xCC\\xC6\\xD1\\xC6\\xAE\\\\particle1.act"')
positions=[compact_spawn.find(t) for t in ordered]
require(all(x>=0 for x in positions),"local Warp emitter missing cadence, RNG or asset operations")
require(positions==sorted(positions),"local Warp operation and RNG order differs")
require("case 8: return ProcessWarp8();" in (ROOT/"src/world/RagEffectOnProcessCases.generated.inc").read_text(),"effect8 route missing")
gen=(ROOT/"tools/generate_effect_dispatch.py").read_text()
require("FORCE_ROUTE.add(8)" in gen and '8: "return ProcessWarp8();"' in gen,"effect8 generator route missing")
print("effect8 PE/Ref helper, exact executable assets, cadence/RNG payload and route passed")
