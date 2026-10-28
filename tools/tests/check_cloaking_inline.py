"""Verify canonical inline Cloaking effect120 behavior and local route."""
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
dispatch=(root/'src/world/RagEffectOnProcessCases.generated.inc').read_text(encoding='utf-8')
def fn(text,sig):
    start=text.rindex(sig); opening=text.index('{',start); depth=0
    for i in range(opening,len(text)):
        if text[i]=='{':depth+=1
        elif text[i]=='}':
            depth-=1
            if not depth:return text[start:i+1]
    raise AssertionError(sig)
def compact(s):return re.sub(r'\s+','',s)
helper='void __thiscall CRagEffect::Cloaking(CRagEffect *this)'
if compact(fn(pe,helper))!=compact(fn(ref,helper)):raise SystemExit('Cloaking PE/primary Ref helper mismatch')
for src,label in ((pe,'PE'),(ref,'Ref')):
    if 'CRagEffect::Cloaking(this' in src:raise SystemExit(f'{label} unexpectedly calls Cloaking helper')
main=fn(pe,'unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)')
case=main[main.index('case 120:'):main.index('case 121:')]
for t in ('if ( !this->m_stateCnt )','PlayWave(aEffectAssasinC, xu, 0.0, zu, 250, 40, 1.0)','CRenderObject::SetArgb(*p_m_master, 50, -1, -1, -1)','this->m_shadowOn = 0'):
    if t not in case:raise SystemExit(f'inline PE effect120 behavior missing {t}')
handler=fn(local,'bool CRagEffect::ProcessCloaking120()')
for t in ('m_stateCnt == 0','"effect\\\\assasin_cloaking.wav"','master->SetArgb(50, -1, -1, -1)','m_shadowOn = false'):
    if t not in handler:raise SystemExit(f'local Cloaking behavior missing {t}')
if 'bool ProcessCloaking120();' not in header or 'case 120: return ProcessCloaking120();' not in dispatch:raise SystemExit('effect120 route missing')
resolved=SymbolTable(ExeImage()).resolve_name('aEffectAssasinC')
if resolved is None or resolved.raw!=b'effect\\assasin_cloaking.wav':raise SystemExit(f'canonical cloaking wave bytes changed: {resolved!r}')
print('Cloaking120 parity: helper/caller distinction, inline PE wave/tint/shadow behavior, local route and exact wave bytes passed')
