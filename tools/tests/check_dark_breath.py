from pathlib import Path
import ast, re, struct
root=Path(__file__).resolve().parents[2]
ref=(root/'Ref/dhxj/RagEffect.cpp').read_text(encoding='latin-1')
header=(root/'Ref/dhxj/RagEffect.h').read_text(encoding='latin-1')
exe=(root/'HighPriest.exe.c').read_text(encoding='latin-1')
impl=(root/'src/world/RagEffect.cpp').read_text(encoding='utf-8')

def body(text,start,end):
    b=text.index(end)
    a=text.rindex(start,0,b)
    return text[a:b]
def ordered(label,text,tokens):
    pos=0
    for token in tokens:
        pos=text.find(token,pos)
        if pos<0: raise SystemExit(f'FAIL: {label} missing/out of order: {token}')
        pos+=len(token)
block=header[header.index('EF_PONG'):header.index('EF_ENERGYDRAIN')]
ids=re.findall(r'\bEF_[A-Z0-9_]+\b',block)
if 199+ids.index('EF_DARKBREATH') != 212: raise SystemExit('FAIL: Ref enum ordering does not derive EF_DARKBREATH=212')
onprocess2_start=exe.index('unsigned __int8 __thiscall CRagEffect::OnProcess2(CRagEffect *this)')
if exe.index('case 212:',onprocess2_start) < onprocess2_start:
    raise SystemExit('FAIL: canonical main dispatcher lacks inline effect ID212 branch')
if 'if (m_type == 212) {\n        SpawnDarkBreath();\n        return true;\n    }' not in impl:
    raise SystemExit('FAIL: current ProcessEffectFrame lacks ID212 route')
ref_fn=body(ref,'void\tCRagEffect :: DarkBreath()','void\tCRagEffect :: SummonSlave()')
exe_fn=body(exe,'void __thiscall CRagEffect::DarkBreath(CRagEffect *this)','//----- (006044B0)')
cur_fn=body(impl,'void CRagEffect::SpawnDarkBreath()','// Ref/RagEffect.cpp:8826-8852')
ordered('Ref',ref_fn,['if (m_stateCnt == 0)','LaunchEffectPrim(PP_3DPARTICLE)','m_duration = 65','m_renderFlag |= RF_NODEPTHCHECK','m_deltaPos2.y -= 20','m_size = (float)(80) / 100','m_animSpeed = 1','m_green = 0','m_blue = 0','m_fadeOutCnt = 60','DarkBreath.spr','DarkBreath.act','InitSegment()'])
ordered('EXE',exe_fn,['if ( !this->m_stateCnt )','LaunchEffectPrim(this, PP_3DPARTICLE','m_duration = 65','m_renderFlag','m_deltaPos2.y - 20.0','m_size = 0.80000001','m_animSpeed = 1','m_green = 0','m_blue = 0','m_fadeOutCnt = 60','&byte_71A250','&byte_71A238','InitSegment(g_prim)'])
ordered('current',cur_fn,['if (m_stateCnt != 0) return','LaunchEffectPrim(PP_3DPARTICLE, vector3d{})','m_duration = 65','m_renderFlag |= 0x200','m_deltaPos2.y -= 20.0f','static_cast<float>(80) / 100.0f','m_animSpeed = 1','m_green = 0','m_blue = 0','m_fadeOutCnt = 60','DarkBreath.spr','DarkBreath.act','m_numSegments = 1','InitReferenceSegments(prim)'])
# Resolve both canonical data pointers from the executable's PE section table and compare to the current exact requests.
bin_data=(root/'HighPriest.exe').read_bytes(); pe=struct.unpack_from('<I',bin_data,0x3c)[0]; coff=pe+4
n=struct.unpack_from('<H',bin_data,coff+2)[0]; opt_size=struct.unpack_from('<H',bin_data,coff+16)[0]; opt=coff+20
base=struct.unpack_from('<I',bin_data,opt+28)[0]; sec=opt+opt_size

def cstring_at_va(va):
    rva=va-base
    for i in range(n):
        o=sec+i*40; vs,va0,rs,rp=struct.unpack_from('<IIII',bin_data,o+8)
        if va0<=rva<va0+max(vs,rs):
            off=rp+(rva-va0); return bin_data[off:bin_data.index(b'\0',off)]
    raise SystemExit(f'FAIL: VA {va:#x} outside PE sections')
canonical_spr=cstring_at_va(0x71A250); canonical_act=cstring_at_va(0x71A238)
strings=re.findall(r'prim->SetParticleResources\(("(?:\\.|[^"\\])*"),\s*("(?:\\.|[^"\\])*")\)',cur_fn)
if len(strings)!=1 or ast.literal_eval(strings[0][0]).encode('latin-1')!=canonical_spr or ast.literal_eval(strings[0][1]).encode('latin-1')!=canonical_act:
    raise SystemExit('FAIL: current DarkBreath asset request differs from executable bytes at 0x71A250/0x71A238')
if 'm_master->GetEffectType() == 212' not in impl[impl.index('bool CEffectPrim::UsesReferenceParticle()'):impl.index('CEffectPrim::~CEffectPrim()')]:
    raise SystemExit('FAIL: PP_3DPARTICLE ID212 lacks current reference updater route')
print('PASS: DarkBreath Ref/EXE/current branch order, derived ID212, dispatcher, exact PE sprite/action bytes and PP_3DPARTICLE route match')
