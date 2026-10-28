#!/usr/bin/env python3
"""Compare Effect13 GlassWall PE/Ref helper, caller, resources and local route."""
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
dhxj = (ROOT / "Ref/dhxj/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
local = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
require(re.search(r"case\s+13\s*:\s*result\s*=\s*CRagEffect::GlassWall\(this\);\s*goto\s+LABEL_425\s*;", pe) is not None,
        "PE OnProcess case13 GlassWall caller missing")
pe_helper = function(pe, "bool __thiscall CRagEffect::GlassWall(CRagEffect *this)")
ref_helper = function(ref, "bool __thiscall CRagEffect::GlassWall(CRagEffect *this)")
require(compact(pe_helper) == compact(ref_helper), "PE and primary Ref GlassWall helper bodies differ")
body = compact(pe_helper)
for token in ("if(!this->m_stateCnt)", "aEffectEfGlassw", "180.0-this->m_master->m_roty",
              "if((int)v3<0)", "v3+360", "MakeYRotation(&tmpMat,Radian)", "v5=1", "i=1",
              "LaunchEffectPrim(this,PP_3DTEXTURE", "v7*2.5999999", "m_pattern=40", "m_any=50",
              "m_longitude=v29", "m_widthSize=3.0", "m_heightSize=40.0", "m_heightSpeed=0.25",
              "m_alpha=0.0", "m_maxAlpha=180.0", "m_maxAlpha*0.16666667", "m_duration-6",
              "aEffectRingBlue", "*(float*)&angle*3.0", "m_longitude=v28", "m_widthSize=2.5999999",
              "v5-=2", "while(v5>=-1)", "ProcessEZ2STR(this,0.0,-80.0,0.0)",
              "returnthis->m_stateCnt!=this->m_duration"):
    require(token in body, f"GlassWall helper missing {token}")

image = ExeImage(ROOT / "HighPriest.exe")
for va, expected in ((0x71B26C, b"effect\\EF_GlassWall.wav"),
                     (0x71943C, b"effect\\ring_blue.tga")):
    actual = image.read_cstring_at(va)
    require(actual is not None and actual.raw == expected, f"GlassWall resource bytes differ at 0x{va:X}")

primary_onprocess = function(ref, "unsigned __int8 __thiscall CRagEffect::OnProcess2(CRagEffect *this)")
require(re.search(r"case\s+13\s*:\s*(?:result\s*=\s*)?CRagEffect::GlassWall", primary_onprocess) is None,
        "primary Ref unexpectedly contains the omitted GlassWall caller branch")
require("EF_GLASSWALL" in dhxj and "GlassWall()" in dhxj,
        "secondary Ref does not corroborate the GlassWall caller")

local_helper = function(local, "bool CRagEffect::ProcessGlassWall13()")
local_body = compact(local_helper)
ordered = ("if(m_stateCnt==0)", 'PlayWave("effect\\\\EF_GlassWall.wav"',
           "static_cast<int64_t>(180.0-master->m_roty)", "if(angle<0)angle+=360",
           "MatrixAppendYRotation(rotation,GetRadian(angle))", "for(intlane=1;lane>=-1;lane-=2)",
           "LaunchEffectPrim(PP_3DTEXTURE,vector3d{})", "first->m_pattern=40", "first->m_any=50",
           "rotation.m[2][0]*firstDistance", "first->m_longitude=static_cast<float>(angle+90)",
           "first->m_widthSize=3.0", 'GetTexture("effect\\\\ring_blue.tga",false)',
           "rotation.m[0][0]*secondDistance", "second->m_longitude=static_cast<float>(angle)",
           "second->m_widthSize=static_cast<float>(2.5999999)", "(void)ProcessEZ2STR(0.0f,-80.0f,0.0f)",
           "returnm_stateCnt!=m_duration")
cursor = 0
positions = []
for token in ordered:
    position = local_body.find(token, cursor)
    positions.append(position)
    if position >= 0:
        cursor = position + len(token)
require(all(pos >= 0 for pos in positions),
        f"local GlassWall handler misses source operations: {[token for token, pos in zip(ordered, positions) if pos < 0]}")
require(positions == sorted(positions), "local GlassWall state, lane, resource or completion order differs")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text()
require("case 13: return ProcessGlassWall13();" in dispatch, "effect13 route missing")
generator = (ROOT / "tools/generate_effect_dispatch.py").read_text()
require("FORCE_ROUTE.add(13)" in generator and '13: "return ProcessGlassWall13();"' in generator,
        "effect13 generator route missing")
print("effect13 PE/primary-Ref helper, caller discrepancy, resources and route passed")
