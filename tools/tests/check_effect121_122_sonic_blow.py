#!/usr/bin/env python3
"""Compare effects121/122 SonicBlow helpers, callers, local routes and resource bytes."""
from __future__ import annotations
import re
import sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage  # noqa: E402
PE=(ROOT/"HighPriest.exe.c").read_text(encoding="latin-1")
REF=(ROOT/"Ref/RagEffect.cpp").read_text(encoding="latin-1")
LOCAL=(ROOT/"src/world/RagEffect.cpp").read_text(encoding="utf-8")
HEADER=(ROOT/"src/world/RagEffect.h").read_text(encoding="utf-8")
DISPATCH=(ROOT/"src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")

def body(source,signature):
    matches=list(re.finditer(signature,source))
    if not matches: raise AssertionError(f"missing {signature}")
    opening=source.find("{",matches[-1].end()); depth=0
    for i in range(opening,len(source)):
        if source[i]=="{": depth+=1
        elif source[i]=="}":
            depth-=1
            if depth==0:return source[opening+1:i]
    raise AssertionError(f"unterminated {signature}")
def compact(source): return re.sub(r"\s+","",source)

for name in ("SonicBlow","SonicBlowHit"):
    sig=rf"void __thiscall CRagEffect::{name}\(CRagEffect \*this\)"
    if compact(body(PE,sig))!=compact(body(REF,sig)):
        raise AssertionError(f"canonical PE/primary-Ref {name} helper bodies differ")
on=body(PE,r"unsigned __int8 __thiscall CRagEffect::OnProcess\(CRagEffect \*this\)")
if not re.search(r"case\s+121:\s*CRagEffect::SonicBlow\(this\);\s*break;",on): raise AssertionError("PE effect121 caller changed")
if not re.search(r"case\s+122:\s*CRagEffect::SonicBlowHit\(this\);\s*break;",on): raise AssertionError("PE effect122 caller changed")
legacy=body(LOCAL,r"bool CRagEffect::ProcessEffectFrameLegacy\(\)")
if not re.search(r"if\s*\(m_type\s*==\s*121\s*\|\|\s*m_type\s*==\s*122\)\s*\{\s*if\s*\(m_type\s*==\s*121\)\s*ApplySonicBlow\(\);\s*else\s*ApplySonicBlowHit\(\);\s*return true;\s*\}",legacy):
    raise AssertionError("local SonicBlow/Hit route or order changed")
for i,name in ((121,"SonicBlow"),(122,"SonicBlowHit")):
    if not re.search(rf"case\s+{i}:\s*return ProcessEffectFrameLegacy\(\);",DISPATCH): raise AssertionError(f"local dispatcher effect{i} route missing")
    if f"void Apply{name}();" not in HEADER: raise AssertionError(f"local {name} declaration missing")
for name,tokens in {
    "ApplySonicBlow":("ProjectReferenceEffectVertex","if (m_stateCnt != 0) return;",'"effect\\\\EF_StoneCurse.wav"',"PP_2DCIRCLE",'"effect\\\\alpha_center.tga"'),
    "ApplySonicBlowHit":("if (m_stateCnt != 0) return;",'"effect\\\\assasin_sonicblow.wav"',"rand() % 10","rand() % 20","PP_3DCYLINDER",'"effect\\\\magic_red.tga"')}.items():
    local=body(LOCAL,rf"void CRagEffect::{name}\(\)")
    for token in tokens:
        if token not in local: raise AssertionError(f"local {name} missing {token}")
image=ExeImage(ROOT/"HighPriest.exe")
for va,expected in ((0x71BB9C,b"effect\\EF_StoneCurse.wav"),(0x7192FC,b"effect\\alpha_center.tga"),
                    (0x71BBD0,b"effect\\assasin_sonicblow.wav"),(0x71BBB8,b"effect\\magic_red.tga")):
    actual=image.read_cstring_at(va)
    if actual is None or actual.raw!=expected: raise AssertionError(f"SonicBlow resource bytes differ at VA 0x{va:X}")
print("effects121/122 SonicBlow parity passed: PE/Ref helper bodies, dispatch/local routes and four exact resource strings")
