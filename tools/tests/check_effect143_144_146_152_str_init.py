#!/usr/bin/env python3
"""Compare STR Init cases143/144/146-152 and exact PE resource bytes."""
from __future__ import annotations
import re
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/"tools"))
from ro_exe_resolve import ExeImage  # noqa: E402
PE=(ROOT/"HighPriest.exe.c").read_text(encoding="latin-1")
REF=(ROOT/"Ref/RagEffect.cpp").read_text(encoding="latin-1")
TABLE=(ROOT/"src/world/RagEffectStrNameTable.generated.inc").read_text(encoding="utf-8")
DISPATCH=(ROOT/"src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
LOCAL=(ROOT/"src/world/RagEffect.cpp").read_text(encoding="utf-8")

def body(source,marker):
    start=source.rindex(marker); opening=source.index("{",start); depth=0
    for pos in range(opening,len(source)):
        if source[pos]=="{": depth+=1
        elif source[pos]=="}":
            depth-=1
            if depth==0:return source[start:pos+1]
    raise AssertionError(f"unterminated {marker}")
def compact(s):return re.sub(r"\s+","",s)
def case(init,effect_id):
    matches=list(re.finditer(rf"case\s+{effect_id}\s*:[\s\S]*?\bbreak\s*;",init))
    if not matches:raise AssertionError(f"Init case{effect_id} missing")
    return matches[0].group(0)
marker="void __thiscall CRagEffect::Init(CRagEffect *this, CRenderObject *master, int effectId, vector3d fDeltaPos)"
pe_init=body(PE,marker); ref_init=body(REF,marker)
resources={143:("SonicBlow.str",0x71AE50),144:("Brandish2.str",0x71AE30),146:("ShockWaveHit.str",0x71ADFC),
           147:("EarthHit.str",0x71ADEC),148:("Pierce.str",0x71ADE0),149:("Bowling.str",0x71ADD4),
           150:("SpearStab.str",0x71ADC4),151:("SpearBoomerang.str",0x71ADB0),152:("HolyHit.str",0x71ADA4)}
image=ExeImage(ROOT/"HighPriest.exe")
for effect_id,(filename,va) in resources.items():
    pe_case=case(pe_init,effect_id); ref_case=case(ref_init,effect_id)
    if compact(pe_case)!=compact(ref_case):raise AssertionError(f"PE/Ref Init case{effect_id} differs")
    if filename not in TABLE:raise AssertionError(f"local STR catalog missing effect{effect_id} {filename}")
    if f"case {effect_id}: return ProcessEZ2STR();" not in DISPATCH:raise AssertionError(f"local effect{effect_id} shared STR route missing")
    actual=image.read_cstring_at(va)
    if actual is None or actual.raw!=filename.encode("ascii"):raise AssertionError(f"PE bytes for {filename} differ at VA 0x{va:X}")
str_only=body(LOCAL,"std::optional<bool> CRagEffect::ProcessReferenceStrOnly()")
group=compact(str_only)
for effect_id in resources:
    if f"case{effect_id}:" not in group:raise AssertionError(f"effect{effect_id} omitted from source shared STR-only group")
print("effects143/144/146-152 STR parity passed: PE/Ref Init cases, local table/routes and nine exact PE strings")
