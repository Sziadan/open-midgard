#!/usr/bin/env python3
"""Compare Effect365, Pressure, and GroundShake callers, helpers, resources, and local payloads."""
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
ref2=(ROOT/"Ref/RagEffect2.cpp").read_text(encoding="utf-8",errors="replace")
src=(ROOT/"src/world/RagEffect.cpp").read_text(encoding="utf-8",errors="replace")
dispatch=(ROOT/"src/world/RagEffectOnProcess2Cases.generated.inc").read_text(encoding="utf-8")
generator=(ROOT/"tools/generate_effect_dispatch.py").read_text(encoding="utf-8")
pe2=function(pe,"unsigned __int8 __thiscall CRagEffect::OnProcess2(CRagEffect *this)")
refon=function(ref,"unsigned __int8 __thiscall CRagEffect::OnProcess2(CRagEffect *this)")
branches=[]
for body,label in ((pe2,"PE"),(refon,"Ref")):
    a=body.index("case 0x16D:"); b=body.index("case 0x16E:",a); branch=compact(body[a:b])
    require("m_stateCnt==3" in branch and "this->m_pos=this->m_master->m_pos" in branch
        and "PlayWave(aEffect_86,xe,0.0,ze,250,40,1.0)" in branch
        and "PRESSURE(this,aEffectCrossOld,0)" in branch
        and "GroundShake(this,aEffectExplosiv,-28,0)" in branch and "return1" in branch,
        f"{label} effect365 branch mismatch")
    branches.append(branch)
require(branches[0]==branches[1],"PE and Ref effect365 case bodies differ")
for signature in ("void __thiscall CRagEffect::PRESSURE(CRagEffect *this, char *tName, char F1)",
                  "void __thiscall CRagEffect::GroundShake(CRagEffect *this, char *tName, int minus, char F1)"):
    require(compact(function(pe,signature))==compact(function(ref2,signature)),
        f"PE and Ref helper bodies differ: {signature}")
assets=SymbolTable(ExeImage())
for symbol,expected in (("aEffect_86",b"effect\\"),("aEffectCrossOld",b"effect\\cross_old.bmp"),
                       ("aEffectExplosiv",b"effect\\explosive_1_128.bmp")):
    got=assets.resolve_name(symbol)
    require(got is not None and got.raw==expected,f"resource bytes differ for {symbol}")
require('case 365: return ProcessEffect365();' in dispatch,"effect365 dispatch route missing")
require('365: "return ProcessEffect365();"' in generator and "FORCE_ROUTE.add(365)" in generator,
        "effect365 generator route missing")
local=function(src,"bool CRagEffect::ProcessEffect365()")
require("ProcessEffect365" in src and "LaunchOn(target, 365)" in (ROOT/"src/world/SkillSpecialEffect.cpp").read_text(encoding="utf-8"),
        "skill367 launch or local handler missing")
for sig,checks in (("void CRagEffect::SpawnPressure365()",("PP_PRESSURE",'"effect\\\\cross_old.bmp"',
        "first.distance=12.0f","first.arcBaselineY=-115.0f","rand()%360","first.flag[2]=7")),
       ("void CRagEffect::SpawnGroundShake365()",("PP_GROUNDSHAKE",'"effect\\\\explosive_1_128.bmp"',
        "first.process=-28","first.distance=15.0f","first.height[0]=-5.0f","unused0=0"))):
    body=compact(function(src,sig))
    for token in checks: require(token in body,f"local helper field mapping missing: {sig}: {token}")
handler=compact(function(src,"bool CRagEffect::ProcessEffect365()"))
for token in ('m_stateCnt==3','PlayWave("effect\\\\"','SpawnPressure365();SpawnGroundShake365();returntrue'):
    require(token in handler,f"local effect365 operation missing: {token}")
print("effect365 PE/Ref caller and helper bodies, exact textures, partial cue, routes, and local mappings passed")
