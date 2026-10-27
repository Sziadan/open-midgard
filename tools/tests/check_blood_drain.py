from pathlib import Path
import ast,re,struct
root=Path(__file__).resolve().parents[2]
ref=(root/'Ref/dhxj/RagEffect.cpp').read_text(encoding='latin-1')
header=(root/'Ref/dhxj/RagEffect.h').read_text(encoding='latin-1')
exe=(root/'HighPriest.exe.c').read_text(encoding='latin-1')
impl=(root/'src/world/RagEffect.cpp').read_text(encoding='utf-8')
def body(text,start,end):
    b=text.index(end); return text[text.rindex(start,0,b):b]
def ordered(label,text,tokens):
    p=0
    for token in tokens:
        p=text.find(token,p)
        if p<0: raise SystemExit(f'FAIL: {label} missing/out of order: {token}')
        p+=len(token)
ids=re.findall(r'\bEF_[A-Z0-9_]+\b',header[header.index('EF_PONG'):header.index('EF_ENERGYDRAIN')])
if 199+ids.index('EF_BLOODDRAIN')!=216: raise SystemExit('FAIL: Ref enum order does not derive EF_BLOODDRAIN=216')
if 'EF_BLOODDRAIN:' not in ref or 'BloodDrain();' not in ref:
    raise SystemExit('FAIL: Ref caller does not route BloodDrain')
if 'case 216:\n            CRagEffect::BloodDrain(this);' not in exe:
    raise SystemExit('FAIL: canonical dispatcher lacks case216 BloodDrain route')
if 'if (m_type == 216) {\n        SpawnBloodDrain();\n        return true;\n    }' not in impl:
    raise SystemExit('FAIL: current ProcessEffectFrame lacks effect ID216 route')
ref_fn=body(ref,'void\tCRagEffect :: BloodDrain()','void\tCRagEffect :: EnergyDrain()')
exe_fn=body(exe,'void __thiscall CRagEffect::BloodDrain(CRagEffect *this)','//----- (006049B0)')
cur_fn=body(impl,'void CRagEffect::SpawnBloodDrain()','// Ref/RagEffect.cpp:8826-8852')
ordered('Ref',ref_fn,['if (m_stateCnt == 0)','for(int a = 0; a < 3; a++)','int i = (-90 + (a * 90))','LaunchEffectPrim(PP_3DPARTICLESPLINE)','m_duration = 60','m_deltaPos2.Set(0, -10, 0)','m_radius = g_prim->CalcDist(m_deltaPos.x, m_deltaPos.z)','g_prim->CalcDir(m_deltaPos.x, m_deltaPos.z)','m_longitude = -g_prim->m_roty','(g_prim->m_radius / g_prim->m_duration) * 2.0f','m_gravSpeed = 2.0f','-(g_prim->m_gravSpeed / g_prim->m_duration) * 2.0f','m_latiSpeed = -2.0f','-(g_prim->m_latiSpeed / g_prim->m_duration) * 2.0f','m_roll = i','MakeYRotation(GetRadian((int)g_prim->m_longitude))','MatrixMult(vector3d(0, 0, 1)','m_rotAxis[2] = g_prim->m_deltaPos3.z','m_red = 250','m_green = 100','m_blue = 100','m_size = 1.7f','particle1.spr','particle1.act','m_numSegment = 7','InitSegment()'])
ordered('EXE',exe_fn,['if ( !this->m_stateCnt )','v2 = -90','v8 = -90','LaunchEffectPrim(this, PP_3DPARTICLESPLINE','m_duration = 60','m_deltaPos2.x = 0.0','m_pos.x = -10.0','m_pos.y = 0.0','m_radius = CRenderObject::CalcDist','CRenderObject::CalcDir','m_longitude = -g_prim->m_roty','m_radius / (double)g_prim->m_duration','m_gravSpeed = 2.0','m_gravSpeed / (double)g_prim->m_duration * -2.0','m_latiSpeed = -2.0','m_latiSpeed / (double)g_prim->m_duration * -2.0','m_roll = (float)v8','MakeYRotation','m_deltaPos3.x = g_prim->m_matrix.v41 + g_prim->m_matrix.v31','m_rotAxis[0] = g_prim->m_deltaPos3.x','m_red = 250','m_green = 100','m_blue = 100','m_size = 1.7','byte_71B200','byte_71B1E8','m_numSegment = 7','InitSegment(g_prim)'])
ordered('current',cur_fn,['if (m_stateCnt != 0) return','index < 3','LaunchEffectPrim(PP_3DPARTICLESPLINE, vector3d{})','m_duration = 60','m_deltaPos2 = {0.0f, -10.0f, 0.0f}','const float dx = m_deltaPos.x - prim->m_pos.x','const float dz = m_deltaPos.z - prim->m_pos.z','std::sqrt(dx * dx + dz * dz)','std::atan2(dx, -dz) / 3.1415926283f','m_longitude = -prim->m_roty','m_speed = (prim->m_radius / prim->m_duration) * 2.0f','m_gravSpeed = 2.0f','-(prim->m_gravSpeed / prim->m_duration) * 2.0f','m_latiSpeed = -2.0f','-(prim->m_latiSpeed / prim->m_duration) * 2.0f','m_roll = static_cast<float>(-90 + index * 90)','MatrixAppendYRotation','prim->m_matrix.m[2][0] + prim->m_matrix.m[3][0]','prim->m_rotAxis = {prim->m_deltaPos3.x','m_red = 250','m_green = 100','m_blue = 100','m_size = 1.7f','particle1.spr','particle1.act','m_numSegments = 7','InitReferenceSegments(prim)'])
for label,fn in [('Ref',ref_fn),('EXE',exe_fn),('current',cur_fn)]:
    if re.search(r'\b(?:rand|random)\s*\(',fn): raise SystemExit(f'FAIL: {label} unexpected RNG in BloodDrain')
# Resolve the canonical sprite/action pointers from PE data and compare emitted C++ requests byte-for-byte.
bin_data=(root/'HighPriest.exe').read_bytes(); pe=struct.unpack_from('<I',bin_data,0x3c)[0]; coff=pe+4
n=struct.unpack_from('<H',bin_data,coff+2)[0]; opts=struct.unpack_from('<H',bin_data,coff+16)[0]; opt=coff+20
base=struct.unpack_from('<I',bin_data,opt+28)[0]; sec=opt+opts
def cstring(va):
    rva=va-base
    for i in range(n):
        o=sec+i*40; vs,sv,rs,rp=struct.unpack_from('<IIII',bin_data,o+8)
        if sv<=rva<sv+max(vs,rs):
            off=rp+rva-sv; return bin_data[off:bin_data.index(b'\0',off)]
    raise SystemExit(f'FAIL: PE pointer {va:#x} outside sections')
req=re.findall(r'prim->SetParticleResources\(("(?:\\.|[^"\\])*"),\s*("(?:\\.|[^"\\])*")\)',cur_fn)
if len(req)!=1 or ast.literal_eval(req[0][0]).encode('latin-1')!=cstring(0x71B200) or ast.literal_eval(req[0][1]).encode('latin-1')!=cstring(0x71B1E8):
    raise SystemExit('FAIL: BloodDrain resource requests differ from canonical PE bytes')
print('PASS: BloodDrain Ref/EXE/current ID216 branch, origin setup, XZ distance/direction, payload, rolls, segment and PE resource bytes match')

