"""Compare Coin's PE/primary-Ref helper and verify canonical effect10 routing."""
from __future__ import annotations
import re,sys
from pathlib import Path
root=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(root/'tools'))
from ro_exe_resolve import ExeImage,SymbolTable
pe=(root/'HighPriest.exe.c').read_text(encoding='latin-1')
ref=(root/'Ref/RagEffect.cpp').read_text(encoding='latin-1')
local=(root/'src/world/RagEffect.cpp').read_text(encoding='utf-8')
header=(root/'src/world/RagEffect.h').read_text(encoding='utf-8')
def fn(text,sig):
    start=text.rindex(sig); opening=text.index('{',start); depth=0
    for i in range(opening,len(text)):
        if text[i]=='{':depth+=1
        elif text[i]=='}':
            depth-=1
            if not depth:return text[start:i+1]
    raise AssertionError(sig)
def compact(s):return re.sub(r'\s+','',s)
sig='bool __thiscall CRagEffect::Coin(CRagEffect *this)'
pe_fn=fn(pe,sig);ref_fn=fn(ref,sig)
if compact(pe_fn)!=compact(ref_fn):raise SystemExit('PE/primary Ref Coin helper mismatch')
main=fn(pe,'unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)')
if 'CRagEffect::Coin(this)' not in main[main.index('case 10:'):main.index('case 11:')]:raise SystemExit('canonical PE effect10 caller missing')
if 'CRagEffect::Coin(this' in ref:raise SystemExit('Ref caller omission changed')
legacy=fn(local,'bool CRagEffect::ProcessEffectFrameLegacy()')
coin=fn(local,'bool CRagEffect::ProcessCoin()')
if 'if (m_type == 10)' not in legacy or 'return ProcessCoin();' not in legacy:raise SystemExit('effect10 current legacy route missing')
for t in ('!g_session.m_isMinEffect','m_stateCnt == 20','i < 5','PP_3DPARTICLEGRAVITY','rand() % 30 + 40','30 * (rand() % 12)','rand() % 2 + 2','rand() % 50 + 50','rand() % 100 + 50','m_playWaveType = 1','m_animSpeed = 1','m_numSegments = 2','ProcessEZ2STR(0.0f,-80.0f,0.0f)','return m_stateCnt < 100'):
    if t not in coin:raise SystemExit(f'Coin payload missing {t}')
if coin.count('rand()')!=5:raise SystemExit('Coin must preserve five ordered random draws per particle')
family=fn(local,'bool CEffectPrim::UsesReferenceParticle() const')
if 'm_master->GetEffectType() == 10' not in family:raise SystemExit('effect10 reference particle family route missing')
if 'bool ProcessCoin();' not in header:raise SystemExit('ProcessCoin declaration missing')
symbols=SymbolTable(ExeImage())
assets=[symbols.resolve_name('byte_71B510'),symbols.resolve_name('byte_71B500')]
if any(x is None for x in assets) or assets[0].raw!=b'\xc0\xcc\xc6\xd1\xc6\xae\\\xb5\xbf\xc0\xfc.spr' or assets[1].raw!=b'\xc0\xcc\xc6\xd1\xc6\xae\\\xb5\xbf\xc0\xfc.act':
    raise SystemExit(f'Coin canonical resource bytes changed: {assets!r}')
print('Coin10 parity: PE/Ref helper, caller/Ref omission, existing route, gate, five-draw particle payload, assets and renderer family passed')
