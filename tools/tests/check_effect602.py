#!/usr/bin/env python3
"""Compare effect602 inline state, resource, skill caller, and tint behavior."""
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
pe_packet = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
ref_packet = (ROOT / "Ref/GameModePacket.cpp").read_text(encoding="utf-8", errors="replace")
src = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
dispatch = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")
generator = (ROOT / "tools/generate_effect_dispatch.py").read_text(encoding="utf-8")

pe_on = function(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
ref_on = function(ref, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
branches = []
for body, label in ((pe_on, "PE"), (ref_on, "Ref")):
    start = body.index("case 0x25A:")
    end = body.index("case 0x25C:", start)
    branch = compact(body[start:end])
    require("v103=this->m_stateCnt" in branch and "v103==20" in branch
            and "this->m_pos=this->m_master->m_pos" in branch
            and "PlayWave(aEffectEfHit6Wa,xbe,0.0,zbe,250,40,1.0)" in branch
            and "v103<=20" in branch and "BYTE1(v107)|=0x20u" in branch
            and "this->m_master->m_BodyTime=this->m_stateCnt" in branch
            and "SetArgb(this->m_master,-1,255,255,255)" in branch
            and "SetArgb(this->m_master,-1,250,50,50)" in branch,
            f"{label} effect602 inline branch missing source behavior")
    branches.append(branch)
require(branches[0] == branches[1], "PE and Ref effect602 inline branches differ")

launch = "CAbleToMakeEffect::LaunchEffect(GameActorByAID, 602, z, 0.0)"
for body, label in ((pe_packet, "PE"), (ref_packet, "Ref")):
    require(launch in body and "v14 == 1005" in body,
            f"{label} skill selector 1005 does not launch effect602")
asset = SymbolTable(ExeImage()).resolve_name("aEffectEfHit6Wa")
require(asset is not None and asset.text == "effect\\ef_hit6.wav", "effect602 exact wave bytes mismatch")
require("case 602: return ProcessEffect602();" in dispatch, "effect602 generated route missing")
require('602: "return ProcessEffect602();"' in generator, "effect602 generator route missing")

local = compact(function(src, "bool CRagEffect::ProcessEffect602()"))
ordered = ("if(m_stateCnt==20&&master!=nullptr)", "m_effectPos=master->m_pos",
           'PlayWave("effect\\\\ef_hit6.wav"', "m_effectPos.x-listener.x", "m_effectPos.z-listener.z",
           "if(m_stateCnt<=20||master==nullptr)returntrue", "master->m_BodyLight|=0x2000",
           "master->m_BodyTime=static_cast<u16>(m_stateCnt)", "(m_stateCnt%2)!=0",
           "SetArgb(-1,255,255,255)", "SetArgb(-1,250,50,50)", "returntrue")
cursor = 0
for token in ordered:
    pos = local.find(token, cursor)
    require(pos >= 0, f"local effect602 operation or branch order missing: {token}")
    cursor = pos + len(token)
print("effect602 complete PE/Ref branch, skill launch, exact wave, route, BodyLight and BodyTime checks passed")
