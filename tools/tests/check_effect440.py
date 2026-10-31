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
assumptio_wave=ExeImage().read_cstring_at(0x71A3D4)
require(assumptio_wave is not None
    and assumptio_wave.raw==b"effect\\"+bytes.fromhex("BE C6 BC FB C7 C1 C6 BC BF C0")+b".wav",
    "canonical PE Assumptio wave bytes at 0x71A3D4 changed")
callsites=(ROOT/"docs/skill/generated/playwave_callsites.tsv").read_text(encoding="utf-8")
require("0x5ECE6A\tCRagEffect::OnProcess3\t0x71A3D4\teffect\\아숨프티오.wav\tok" in callsites,
    "canonical PE Assumptio filename/callsite crosswalk changed")
require('case 440: return ProcessEffect440();' in dispatch,"effect440 route missing")
require('440: "return ProcessEffect440();"' in generator and "FORCE_ROUTE.add(440)" in generator,
    "effect440 generator route missing")
local=compact(function(src,"bool CRagEffect::ProcessEffect440()"))
ordered=("if(m_stateCnt==0&&master!=nullptr)","m_effectPos=master->m_pos",
    r'PlayWave("effect\\\xBE\xC6\xBC\xFB\xC7\xC1\xC6\xBC\xBF\xC0.wav"',
    'm_effectPos.x-player->m_pos.x','m_effectPos.z-player->m_pos.z',
    "ProcessEZ2STR(0.0f,-80.0f,0.0f);returntrue")
positions=[local.find(token) for token in ordered]
require(all(p>=0 for p in positions) and positions==sorted(positions),"local effect440 branch order/STR mismatch")
strrow=next(line for line in (ROOT/"src/world/RagEffectStrNameTable.generated.inc").read_text(encoding="utf-8").splitlines() if "{ 440," in line)
require('"asum.str"' in strrow,"local Ref STR mapping mismatch")
packet=(ROOT/"src/gamemode/GameModePacket.cpp").read_text(encoding="utf-8")
resolver=function(packet,"CGameActor* ResolveSkillPacketActor(CGameMode& mode, u32 aid)")
require("aid==g_session.m_gid" in compact(resolver)
    and "aid==player->m_gid" in compact(resolver),
    "skill packet actor resolver must map both local actor ids")
use_skill=(ROOT/"src/gamemode/UseSkillEffect.generated.inc").read_text(encoding="utf-8")
require("case 0x169:" in use_skill and "LaunchOn(target, 375);" in use_skill
    and "LaunchOn(target, 440);" in use_skill,
    "Assumptio 0x169 effect route is missing effect375/effect440")
print("effect440 PE Assumptio wave bytes/callsite, STR route, local actor resolution, and Assumptio skill effects passed")
