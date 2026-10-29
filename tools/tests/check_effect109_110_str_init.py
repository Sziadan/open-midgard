#!/usr/bin/env python3
"""Compare effects109/110 STR initialization, variants and PE resource bytes."""
from __future__ import annotations
import re
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/"tools"))
from ro_exe_resolve import ExeImage  # noqa: E402
PE=(ROOT/"HighPriest.exe.c").read_text(encoding="latin-1")
REF=(ROOT/"Ref/RagEffect.cpp").read_text(encoding="latin-1")
LOCAL=(ROOT/"src/world/RagEffect.cpp").read_text(encoding="utf-8")
TABLE=(ROOT/"src/world/RagEffectStrNameTable.generated.inc").read_text(encoding="utf-8")
DISPATCH=(ROOT/"src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
LIFECYCLE=(ROOT/"tools/tests/EffectLifecycleTests.cpp").read_text(encoding="utf-8")

def body(source,marker):
    start=source.rindex(marker); opening=source.index("{",start); depth=0
    for pos in range(opening,len(source)):
        if source[pos]=="{": depth+=1
        elif source[pos]=="}":
            depth-=1
            if depth==0:return source[start:pos+1]
    raise AssertionError(f"unterminated {marker}")
def compact(s):return re.sub(r"\s+","",s)
marker="void __thiscall CRagEffect::Init(CRagEffect *this, CRenderObject *master, int effectId, vector3d fDeltaPos)"
init_pe=body(PE,marker); init_ref=body(REF,marker)
def case_block(init,effect_id):
    match=re.search(rf"case\s+{effect_id}\s*:[\s\S]*?\bbreak\s*;",init)
    if not match:raise AssertionError(f"Init case {effect_id} missing")
    return match.group(0)
for effect_id in (109,110):
    pe_case=case_block(init_pe,effect_id); ref_case=case_block(init_ref,effect_id)
    if compact(pe_case)!=compact(ref_case):raise AssertionError(f"PE/Ref Init case{effect_id} differs")
bubble=compact(case_block(init_pe,109))
if "rand()%4" not in bubble or 'sprintf(StrName,"Bubble%d.str",v8+1)' not in bubble:
    raise AssertionError("effect109 no longer selects Bubble1.str through Bubble4.str with one rand()%4 draw")
gas=compact(case_block(init_pe,110))
if "sprintf(StrName,aGaspushStr)" not in gas:raise AssertionError("effect110 GasPush STR selection changed")
if '{ 109, nullptr, nullptr, "Bubble", 1, 4 }' not in TABLE or '{ 110, "GasPush.str", nullptr, nullptr, 0, 0 }' not in TABLE:
    raise AssertionError("local generated STR rows for effects109/110 changed")
str_only=body(LOCAL,"std::optional<bool> CRagEffect::ProcessReferenceStrOnly()")
if not re.search(r"case\s+109:\s*case\s+110:",str_only):raise AssertionError("local shared STR route missing 109/110")
if "case 109" not in DISPATCH and "case 109:" not in DISPATCH:raise AssertionError("local dispatcher lacks effect109 route")
if "case 110" not in DISPATCH and "case 110:" not in DISPATCH:raise AssertionError("local dispatcher lacks effect110 route")
if "109,110,126,135,139" not in re.sub(r"\s+","",LIFECYCLE):raise AssertionError("existing STR lifecycle matrix no longer covers effects109/110")
image=ExeImage(ROOT/"HighPriest.exe")
gas_bytes=image.read_cstring_at(0x71AF24)
if gas_bytes is None or gas_bytes.raw!=b"GasPush.str":raise AssertionError("GasPush PE bytes differ at VA 0x71AF24")
print("effects109/110 STR parity passed: PE/Ref Init, four Bubble variants, GasPush bytes, local routes and lifecycle coverage")
