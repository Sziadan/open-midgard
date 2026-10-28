#!/usr/bin/env python3
"""Compare Effect74 IceWall PE/Ref helper, exact resources, payload and route."""
from __future__ import annotations
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage


def require(ok, message):
    if not ok:
        raise SystemExit(message)


def function(source, signature):
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[start:pos + 1]
    raise AssertionError(signature)


def compact(source):
    return re.sub(r"\s+", "", source)


pe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
ref = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
local = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
require(re.search(r"case\s+74\s*:\s*CRagEffect::IceWall\(this\);\s*break\s*;", pe) is not None,
        "PE OnProcess case74 IceWall caller missing")
pe_helper = function(pe, "void __thiscall CRagEffect::IceWall(CRagEffect *this)")
ref_helper = function(ref, "void __thiscall CRagEffect::IceWall(CRagEffect *this)")
require(compact(pe_helper) == compact(ref_helper), "PE and Ref IceWall helper bodies differ")
body = compact(pe_helper)
for token in ("if(!this->m_stateCnt)", "aEffectWizardIc", "this->m_duration=9999", "v10=-2",
              "CRagEffect::LaunchEffectPrim(this,PP_3DQUADHORN,angle[0])", "m_duration=this->m_duration",
              "m_pattern|=1u", "rand()%360", "GetSin(v3)", "GetCos(v3)", "v10*Cos",
              "rand()%40/100", "rand()%100+300", "rand()%6+87", "m_count=20", "m_alpha=200.0",
              "MakeXRotation(&g_prim->m_matrix", "AppendYRotation(&g_prim->m_matrix",
              "m_speed=3.0", "m_speed*0.025*-2.0", "m_duration-10", "aEffectIceTga", "v10+=2",
              "while(v10<4)"):
    require(token in body, f"IceWall helper missing {token}")

image = ExeImage(ROOT / "HighPriest.exe")
for va, expected in ((0x71B83C, b"effect\\wizard_icewall.wav"),
                     (0x71B5FC, b"effect\\ice.tga")):
    actual = image.read_cstring_at(va)
    require(actual is not None and actual.raw == expected, f"IceWall resource bytes differ at 0x{va:X}")

local_helper = function(local, "bool CRagEffect::ProcessIceWall74()")
local_body = compact(local_helper)
ordered = ("if(m_stateCnt!=0)returntrue;", 'PlayWave("effect\\\\wizard_icewall.wav"',
           "m_duration=9999", "for(intoffset=-2;offset<4;offset+=2)",
           "LaunchEffectPrim(PP_3DQUADHORN,vector3d{})", "m_pattern|=1u", "rand()%360",
           "GetSin(angle)", "GetCos(angle)", "rand()%40/100", "rand()%100+300", "rand()%360",
           "rand()%6+87", "m_count=20", "m_alpha=200.0f", "MatrixAppendXRotation",
           "MatrixAppendYRotation", "m_speed=3.0f", "m_speed*0.025*-2.0", "m_duration-10",
           'GetTexture("effect\\\\ice.tga",false)')
positions = []
cursor = 0
for token in ordered:
    position = local_body.find(token, cursor)
    positions.append(position)
    if position >= 0:
        cursor = position + len(token)
require(all(pos >= 0 for pos in positions), "local IceWall handler misses a source payload/resource operation")
require(positions == sorted(positions), "local IceWall resource/RNG/payload order differs")
require(local_body.endswith("returntrue;}"), "local IceWall handler no longer completes after state0 setup")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text()
require("case 74: return ProcessIceWall74();" in dispatch, "effect74 route missing")
generator = (ROOT / "tools/generate_effect_dispatch.py").read_text()
require("FORCE_ROUTE.add(74)" in generator and '74: "return ProcessIceWall74();"' in generator,
        "effect74 generator route missing")
print("effect74 PE/Ref helper, executable resources, RNG payload and route passed")
