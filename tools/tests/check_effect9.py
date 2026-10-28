#!/usr/bin/env python3
"""Compare Effect9 Enhance PE/Ref helper, resources, cadence and route."""
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
require(re.search(r"case\s+9\s*:\s*CRagEffect::Enhance\(this\);\s*break\s*;",pe) is not None,"PE OnProcess case9 Enhance caller missing")
pe_helper=function(pe,"void __thiscall CRagEffect::Enhance(CRagEffect *this)")
ref_helper=function(ref,"void __thiscall CRagEffect::Enhance(CRagEffect *this)")
require(compact(pe_helper)==compact(ref_helper),"PE and Ref Enhance helper bodies differ")
for token in ("if(!this->m_stateCnt)","PP_3DCIRCLE","m_arcAngle=22.5","m_size=7.0","m_latitude=90.0",
              "m_texture=CTexMgr::GetTexture(&g_texMgr,aEffectRingYell,0)","PP_3DCYLINDER","m_outerSize=6.0",
              "m_innerSize=6.0","m_heightSpeed=2.0","m_heightAccel=g_prim->m_heightSpeed/(double)g_prim->m_duration*-0.66666669",
              "m_maxHeightSize=50.0","aEffectAlphaDow","m_stateCnt%15","m_stateCnt%5","PP_3DCROSSTEXTURE",
              "v2%360","rand()%5+2","m_speed=2.0","m_widthSize=7.0","m_heightSize=0.25","m_GI[0].life=0"):
    require(token in compact(pe_helper),f"Enhance helper missing {token}")
im=ExeImage(ROOT/"HighPriest.exe")
for va,expected in ((0x7193E8,b"effect\\ring_yellow.tga"),(0x719498,b"effect\\alpha_down.tga")):
    actual=im.read_cstring_at(va)
    require(actual is not None and actual.raw==expected,f"Enhance texture bytes differ at 0x{va:X}")
spawn=function(local,"void CRagEffect::SpawnEnhance9()")
compact_spawn=compact(spawn)
ordered=("if(m_stateCnt==0)","LaunchEffectPrim(PP_3DCIRCLE,vector3d{})","m_arcAngle=22.5f","m_size=7.0f",
         'GetTexture("effect\\\\ring_yellow.tga",false)',"LaunchEffectPrim(PP_3DCYLINDER,vector3d{})",
         "m_heightAccel=cylinder->m_heightSpeed/static_cast<double>(cylinder->m_duration)*-0.66666669",
         'GetTexture("effect\\\\alpha_down.tga",false)',"if(g_session.m_isMinEffect)","m_stateCnt%15",
         "m_stateCnt%5","LaunchEffectPrim(PP_3DCROSSTEXTURE,vector3d{})","rotation=rand()%360","radius=rand()%5+2",
         'cross->m_texture.push_back(g_texMgr.GetTexture("effect\\\\alpha_down.tga",false))')
positions=[compact_spawn.find(t) for t in ordered]
require(all(x>=0 for x in positions),"local Enhance emitter is missing a source cadence/payload operation")
require(positions==sorted(positions),"local Enhance cadence, RNG or resource order differs")
require("case 9: return ProcessEnhance9();" in (ROOT/"src/world/RagEffectOnProcessCases.generated.inc").read_text(),"effect9 route missing")
gen=(ROOT/"tools/generate_effect_dispatch.py").read_text()
require("FORCE_ROUTE.add(9)" in gen and '9: "return ProcessEnhance9();"' in gen,"effect9 generator route missing")
print("effect9 PE/Ref helper, exact textures, cadence/RNG payload and route passed")
