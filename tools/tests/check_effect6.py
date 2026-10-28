#!/usr/bin/env python3
"""Compare Effect6 Entry caller, exact ring asset and cylinder payload."""
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
require(re.search(r"case\s+6\s*:\s*CRagEffect::Entry\(this\);\s*break\s*;",pe) is not None,
        "PE OnProcess case6 Entry caller missing")
pe_helper=function(pe,"void __thiscall CRagEffect::Entry(CRagEffect *this)")
ref_helper=function(ref,"void __thiscall CRagEffect::Entry(CRagEffect *this)")
require(compact(pe_helper)==compact(ref_helper),"PE and Ref Entry helper bodies differ")
for token in ("if(!this->m_stateCnt)","PP_3DCYLINDER","m_duration=55","m_accel=-0.000070000002",
              "m_longSpeed=-10.0","m_outerSize=6.0","m_outerSpeed=0.079999998","m_innerSize=5.0",
              "m_heightSize=6.5","m_heightSpeed=-0.1","m_maxAlpha=245.0","m_alphaSpeed=10.0",
              "m_fadeOutCnt=g_prim->m_duration-g_prim->m_duration/5","aEffectRingBlue","m_arcAngle=45.0",
              "m_longSpeed=10.0","m_outerSize=4.5","m_innerSize=4.5","m_heightSpeed=2.5",
              "m_heightAccel=g_prim->m_heightSpeed/(double)g_prim->m_duration*-2.0","m_fadeOutCnt=g_prim->m_duration-g_prim->m_duration/3"):
    require(token in compact(pe_helper),f"Entry helper missing {token}")
im=ExeImage(ROOT/"HighPriest.exe")
asset=im.read_cstring_at(0x71943C)
require(asset is not None and asset.raw==b"effect\\ring_blue.tga","effect6 ring texture bytes differ at 0x71943C")
spawn=function(local,"void CRagEffect::SpawnEntry6()")
for token in ("if(m_stateCnt!=0)return;","LaunchEffectPrim(PP_3DCYLINDER,vector3d{})","MatrixIdentity(first->m_matrix)",
              "MatrixAppendXRotation(first->m_matrix,GetRadian(static_cast<int>(first->m_latitude)))",
              "MatrixAppendYRotation(first->m_matrix,GetRadian(static_cast<int>(first->m_longitude)))",
              "first->m_accel=-0.000070000002f","first->m_outerSpeed=0.079999998f",
              'GetTexture("effect\\\\ring_blue.tga",false)','second->m_heightAccel=second->m_heightSpeed/static_cast<double>(second->m_duration)*-2.0'):
    require(token in compact(spawn),f"local Entry mapping missing {token}")
dispatch=(ROOT/"src/world/RagEffectOnProcessCases.generated.inc").read_text()
require("case 6: return ProcessEntry6();" in dispatch,"effect6 route missing")
gen=(ROOT/"tools/generate_effect_dispatch.py").read_text()
require("FORCE_ROUTE.add(6)" in gen and '6: "return ProcessEntry6();"' in gen,"effect6 generator route missing")
print("effect6 PE/Ref helper, exact texture, local cylinder payload and route passed")
