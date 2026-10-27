from pathlib import Path
import ast,re,struct
root=Path(__file__).resolve().parents[2]
ref=(root/'Ref/dhxj/RagEffect.cpp').read_text(encoding='latin-1'); header=(root/'Ref/dhxj/RagEffect.h').read_text(encoding='latin-1')
exe=(root/'HighPriest.exe.c').read_text(encoding='latin-1'); impl=(root/'src/world/RagEffect.cpp').read_text(encoding='utf-8')
def body(text,start,end):
    b=text.index(end); return text[text.rindex(start,0,b):b]
def ordered(label,text,tokens):
    p=0
    for token in tokens:
        p=text.find(token,p)
        if p<0: raise SystemExit(f'FAIL: {label} missing/out of order: {token}')
        p+=len(token)
ids=re.findall(r'\bEF_[A-Z0-9_]+\b',header[header.index('EF_PONG'):header.index('EF_ENERGYDRAIN2')])
if 199+ids.index('EF_ENERGYDRAIN')!=217: raise SystemExit('FAIL: Ref enum order does not derive EF_ENERGYDRAIN=217')
if 'EF_ENERGYDRAIN:' not in ref or 'EnergyDrain();' not in ref: raise SystemExit('FAIL: Ref caller lacks EnergyDrain route')
if 'case 217:\n            CRagEffect::EnergyDrain(this);' not in exe: raise SystemExit('FAIL: canonical dispatcher lacks case217 EnergyDrain route')
if 'if (m_type == 217) {\n        SpawnEnergyDrain();\n        return true;\n    }' not in impl: raise SystemExit('FAIL: current ProcessEffectFrame lacks ID217 route')
ref_fn=body(ref,'void\tCRagEffect :: EnergyDrain()','void\tCRagEffect :: PotionPillar(')
exe_fn=body(exe,'void __thiscall CRagEffect::EnergyDrain(CRagEffect *this)','//----- (00604C30)')
cur_fn=body(impl,'void CRagEffect::SpawnEnergyDrain()','// Ref/RagEffect.cpp:8826-8852')
ordered('Ref',ref_fn,['if (m_stateCnt == 0)','for(int a = 0; a < 3; a++)','int i = (-90 + (a * 90))','LaunchEffectPrim(PP_3DPARTICLESPLINE)','m_duration = 60','m_deltaPos2.Set(0, -10, 0)','m_radius = g_prim->CalcDist(m_deltaPos.x, m_deltaPos.z)','g_prim->CalcDir(m_deltaPos.x, m_deltaPos.z)','m_longitude = -g_prim->m_roty','m_speed = (g_prim->m_radius / g_prim->m_duration) * 2.0f','m_gravSpeed = 2.0f','m_latiSpeed = -2.0f','m_roll = i','MakeYRotation(GetRadian((int)g_prim->m_longitude))','m_rotAxis[2] = g_prim->m_deltaPos3.z','m_red = 100','m_green = 100','m_blue = 250','m_size = 1.7f','particle1.spr','particle1.act','m_numSegment = 7','InitSegment()'])
ordered('EXE',exe_fn,['if ( !this->m_stateCnt )','v2 = -90','v8 = -90','LaunchEffectPrim(this, PP_3DPARTICLESPLINE','m_duration = 60','m_deltaPos2.x = 0.0','m_pos.x = -10.0','m_pos.y = 0.0','m_radius = CRenderObject::CalcDist','CRenderObject::CalcDir','m_longitude = -g_prim->m_roty','m_gravSpeed = 2.0','m_latiSpeed = -2.0','m_roll = (float)v8','MakeYRotation','m_rotAxis[0] = g_prim->m_deltaPos3.x','m_red = 100','m_green = 100','m_blue = 250','m_size = 1.7','byte_71B200','byte_71B1E8','m_numSegment = 7','InitSegment(g_prim)'])
ordered('current',cur_fn,['if (m_stateCnt != 0) return','index < 3','LaunchEffectPrim(PP_3DPARTICLESPLINE, vector3d{})','m_duration = 60','m_deltaPos2 = {0.0f, -10.0f, 0.0f}','const float dx = m_deltaPos.x - prim->m_pos.x','const float dz = m_deltaPos.z - prim->m_pos.z','std::sqrt(dx * dx + dz * dz)','std::atan2(dx, -dz) / 3.1415926283f','m_longitude = -prim->m_roty','m_speed = (prim->m_radius / prim->m_duration) * 2.0f','m_gravSpeed = 2.0f','m_latiSpeed = -2.0f','m_roll = static_cast<float>(-90 + index * 90)','MatrixAppendYRotation','prim->m_rotAxis = {prim->m_deltaPos3.x','m_red = 100','m_green = 100','m_blue = 250','m_size = 1.7f','particle1.spr','particle1.act','m_numSegments = 7','InitReferenceSegments(prim)'])
for label,fn in [('Ref',ref_fn),('EXE',exe_fn),('current',cur_fn)]:
    if re.search(r'\b(?:rand|random)\s*\(',fn): raise SystemExit(f'FAIL: {label} unexpected RNG')
data=(root/'HighPriest.exe').read_bytes(); pe=struct.unpack_from('<I',data,0x3c)[0]; coff=pe+4
n=struct.unpack_from('<H',data,coff+2)[0]; osz=struct.unpack_from('<H',data,coff+16)[0]; opt=coff+20
base=struct.unpack_from('<I',data,opt+28)[0]; sec=opt+osz
def cstr(va):
    rva=va-base
    for i in range(n):
        o=sec+i*40; vs,sv,rs,rp=struct.unpack_from('<IIII',data,o+8)
        if sv<=rva<sv+max(vs,rs):
            off=rp+rva-sv; return data[off:data.index(b'\0',off)]
    raise SystemExit(f'FAIL: canonical VA {va:#x} outside sections')
req=re.findall(r'prim->SetParticleResources\(("(?:\\.|[^"\\])*"),\s*("(?:\\.|[^"\\])*")\)',cur_fn)
if len(req)!=1 or ast.literal_eval(req[0][0]).encode('latin-1')!=cstr(0x71B200) or ast.literal_eval(req[0][1]).encode('latin-1')!=cstr(0x71B1E8):
    raise SystemExit('FAIL: EnergyDrain requests differ from canonical PE asset bytes')
print('PASS: EnergyDrain Ref/EXE/current ID217 branches, spline payload, blue tint, segment setup and PE resources match')

