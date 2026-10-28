"""Compare BloodDrain's canonical PE and primary Ref helpers with the routed implementation."""
from __future__ import annotations
import ast, re, struct
from pathlib import Path
root = Path(__file__).resolve().parents[2]
ref = (root / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
exe = (root / "HighPriest.exe.c").read_text(encoding="latin-1")
impl = (root / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
header = (root / "src/world/RagEffect.h").read_text(encoding="utf-8")
dispatch = (root / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
def fn(text: str, signature: str) -> str:
    start = text.rindex(signature); opening = text.index("{", start); depth = 0
    for i in range(opening, len(text)):
        if text[i] == "{": depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0: return text[start:i+1]
    raise AssertionError(signature)
def compact(text: str) -> str: return re.sub(r"\s+", "", text)
ref_fn = fn(ref, "void __thiscall CRagEffect::BloodDrain(CRagEffect *this)")
exe_fn = fn(exe, "void __thiscall CRagEffect::BloodDrain(CRagEffect *this)")
if compact(ref_fn) != compact(exe_fn): raise SystemExit("FAIL: primary Ref/PE BloodDrain helper bodies differ")
main = fn(exe, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
case = main[main.index("case 216:"):main.index("case 217:")]
if "CRagEffect::BloodDrain(this)" not in case: raise SystemExit("FAIL: canonical PE case216 no longer calls BloodDrain")
if "BloodDrain(this" in ref: raise SystemExit("FAIL: Ref caller omission changed; reevaluate parity evidence")
if "if (m_type == 216) {\n        SpawnBloodDrain();\n        return true;\n    }" not in impl: raise SystemExit("FAIL: current effect216 route missing")
if "void SpawnBloodDrain();" not in header or "216: \"SpawnBloodDrain(); return true;\"" not in (root / "tools/generate_effect_dispatch.py").read_text(encoding="utf-8"):
    raise SystemExit("FAIL: BloodDrain declaration/dispatcher route missing")
cur_fn = fn(impl, "void CRagEffect::SpawnBloodDrain()")
for label, body, tokens in (
    ("Ref", ref_fn, ["if(!this->m_stateCnt)", "v2=-90", "PP_3DPARTICLESPLINE", "m_duration=60", "m_radius=CRenderObject::CalcDist", "CRenderObject::CalcDir", "m_longitude=-g_prim->m_roty", "m_roll=(float)v8", "m_red=250", "m_green=100", "m_blue=100", "m_size=1.7", "m_numSegment=7", "InitSegment(g_prim)"]),
    ("local", cur_fn, ["if(m_stateCnt!=0)return", "index<3", "PP_3DPARTICLESPLINE", "m_duration=60", "std::sqrt(dx*dx+dz*dz)", "std::atan2(dx,-dz)", "m_longitude=-prim->m_roty", "m_roll=static_cast<float>(-90+index*90)", "m_red=250", "m_green=100", "m_blue=100", "m_size=1.7f", "m_numSegments=7", "InitReferenceSegments(prim)"]),
):
    body = compact(body)
    pos = 0
    for token in tokens:
        token = compact(token)
        pos = body.find(token, pos)
        if pos < 0: raise SystemExit(f"FAIL: {label} ordered payload missing {token}")
        pos += len(token)
if re.search(r"\b(?:rand|random)\s*\(", ref_fn + exe_fn + cur_fn): raise SystemExit("FAIL: unexpected RNG in BloodDrain")
# Resolve sprite/action pointers from the PE data section and compare to emitted C++ literals.
bin_data=(root/"HighPriest.exe").read_bytes(); pe=struct.unpack_from("<I",bin_data,0x3c)[0]; coff=pe+4
n=struct.unpack_from("<H",bin_data,coff+2)[0]; opts=struct.unpack_from("<H",bin_data,coff+16)[0]; opt=coff+20
base=struct.unpack_from("<I",bin_data,opt+28)[0]; sec=opt+opts
def cstring(va):
    rva=va-base
    for i in range(n):
        o=sec+i*40; vs,sv,rs,rp=struct.unpack_from("<IIII",bin_data,o+8)
        if sv<=rva<sv+max(vs,rs):
            off=rp+rva-sv; return bin_data[off:bin_data.index(b"\0",off)]
    raise SystemExit(f"FAIL: PE pointer {va:#x} outside sections")
req=re.findall(r'prim->SetParticleResources\(("(?:\\.|[^"\\])*"),\s*("(?:\\.|[^"\\])*")\)',cur_fn)
if len(req)!=1 or ast.literal_eval(req[0][0]).encode("latin-1")!=cstring(0x71B200) or ast.literal_eval(req[0][1]).encode("latin-1")!=cstring(0x71B1E8):
    raise SystemExit("FAIL: BloodDrain resources differ from canonical PE bytes")
print("PASS: BloodDrain primary Ref/PE helpers match; PE caller, Ref omission, effect216 route, payload, cadence, no-RNG and assets verified")
