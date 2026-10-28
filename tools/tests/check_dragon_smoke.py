from pathlib import Path
import re
import sys
root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / 'tools'))
from ro_exe_resolve import ExeImage
ref = (root / 'Ref/dhxj/RagEffect.cpp').read_text(encoding='latin-1')
exe = (root / 'HighPriest.exe.c').read_text(encoding='latin-1')
impl = (root / 'src/world/RagEffect.cpp').read_text(encoding='utf-8')
index = (root / 'docs/skill/generated/effect_handler_index.tsv').read_text(encoding='utf-8')

def body(text, start, end):
    b = text.index(end)
    a = text.rindex(start, 0, b)
    return text[a:b]

def ordered(label, text, tokens):
    cursor = 0
    for token in tokens:
        pos = text.find(token, cursor)
        if pos < 0:
            raise SystemExit(f'FAIL: {label} missing/out of order: {token}')
        cursor = pos + len(token)

row = next((line for line in index.splitlines() if line.startswith('OnProcess2\tRef/RagEffect.cpp\t') and line.endswith('\tDragonSmoke\t11094')), None)
if row is None or row.split('\t')[2] != '373':
    raise SystemExit('FAIL: generated Ref handler index does not map DragonSmoke to ID373')
if 'case 0x175:\n      CRagEffect::DragonSmoke(this);' not in exe:
    raise SystemExit('FAIL: canonical dispatcher lacks DragonSmoke case0x175')
if 'case 373:\n        SpawnDragonSmoke();\n        return true;' not in impl:
    raise SystemExit('FAIL: current OnProcess2 lacks exact ID373 route')
if '"\\xC0\\xCC\\xC6\\xD1\\xC6\\xAE\\\\\\xB1\\xBC\\xB6\\xD2\\xBF\\xAC\\xB1\\xE2.spr"' not in impl or '"\\xC0\\xCC\\xC6\\xD1\\xC6\\xAE\\\\\\xB1\\xBC\\xB6\\xD2\\xBF\\xAC\\xB1\\xE2.act"' not in impl:
    raise SystemExit('FAIL: current DragonSmoke CP949 source literals changed')
image = ExeImage(root / 'HighPriest.exe')
for address, suffix in ((0x71B440, b'.spr'), (0x71B42C, b'.act')):
    asset = image.read_cstring_at(address)
    expected = b'\xC0\xCC\xC6\xD1\xC6\xAE\\\xB1\xBC\xB6\xD2\xBF\xAC\xB1\xE2' + suffix
    if asset is None or asset.raw != expected:
        raise SystemExit(f'FAIL: DragonSmoke PE asset bytes differ at {address:#x}: {asset!r}')
particle_route_start = impl.index('bool CEffectPrim::UsesReferenceParticle()')
particle_route_end = impl.index('CEffectPrim::~CEffectPrim()', particle_route_start)
if 'm_master->GetEffectType() == 373' not in impl[particle_route_start:particle_route_end]:
    raise SystemExit('FAIL: PP_3DPARTICLE effect ID373 lacks the current reference update route')
ref_body = body(ref, 'void\tCRagEffect :: DragonSmoke()', 'void\tCRagEffect :: DarkBreath()')
exe_body = body(exe, 'void __thiscall CRagEffect::DragonSmoke(CRagEffect *this)', '//----- (006043B0)')
cur_body = body(impl, 'void CRagEffect::SpawnDragonSmoke()', 'void CRagEffect::SpawnDarkBreath()')
ordered('Ref', ref_body, ['m_pos = m_master->m_pos', 'if (m_stateCnt == 0)', 'LaunchEffectPrim(PP_3DPARTICLE, m_deltaPos)', 'm_duration = m_duration', 'm_rollSpeed = 0.75f', '75 + random(30)', 'AppendYRotation(GetRadian(random(360)))', 'm_deltaPos2.y = -9.0f', 'random(3) + 3', 'm_size = 1.5f', 'm_duration - (g_prim->m_duration / 3)', 'chimneysmoke.spr', 'chimneysmoke.act', 'InitSegment()'])
ordered('EXE', exe_body, ['this->m_pos = this->m_master->m_pos', 'if ( !this->m_stateCnt )', 'LaunchEffectPrim(this, PP_3DPARTICLE, this->m_deltaPos)', 'm_duration = this->m_duration', 'm_rollSpeed = 0.75', 'v2 % 30 + 75', 'v3 % 360', 'm_deltaPos2.y = -9.0', 'rand() % 3 + 3', 'm_size = 1.5', 'm_duration - g_prim->m_duration / 3', '&byte_71B440', '&byte_71B42C', 'InitSegment(g_prim)'])
ordered('current', cur_body, ['ResolveCurrentMaster()', 'm_effectPos = master->m_pos', 'if (m_stateCnt != 0) return', 'LaunchEffectPrim(PP_3DPARTICLE, m_deltaPos)', 'm_duration = m_duration', 'm_rollSpeed = 0.75f', 'rand() % 30 + 75', 'rand() % 360', 'MatrixAppendYRotation', 'm_deltaPos2.y = -9.0f', 'rand() % 3 + 3', 'm_size = 1.5f', 'm_duration - prim->m_duration / 3', 'SetParticleResources(', 'm_numSegments = 1', 'InitReferenceSegments(prim)'])
for label, text in [('Ref', ref_body), ('EXE', exe_body), ('current', cur_body)]:
    draws = len(re.findall(r'\b(?:rand|random)\s*\(', text))
    if draws != 3:
        raise SystemExit(f'FAIL: {label} RNG call count is {draws}')
print('PASS: DragonSmoke Ref/EXE/current branch order, ID373 route, three RNG draws, payload, resources and segment setup match')



