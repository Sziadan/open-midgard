"""Compare ColdHit's PE/primary-Ref helper and verify current handler routes."""
from __future__ import annotations
import re
from pathlib import Path
root=Path(__file__).resolve().parents[2]
pe=(root/'HighPriest.exe.c').read_text(encoding='latin-1')
ref=(root/'Ref/RagEffect.cpp').read_text(encoding='latin-1')
local=(root/'src/world/RagEffect.cpp').read_text(encoding='utf-8')
def fn(text,sig):
    start=text.rindex(sig); opening=text.index('{',start); depth=0
    for i in range(opening,len(text)):
        if text[i]=='{':depth+=1
        elif text[i]=='}':
            depth-=1
            if not depth:return text[start:i+1]
    raise AssertionError(sig)
def compact(s):return re.sub(r'\s+','',s)
sig='void __thiscall CRagEffect::ColdHit(CRagEffect *this)'
if compact(fn(pe,sig))!=compact(fn(ref,sig)):raise SystemExit('ColdHit PE/primary Ref helper mismatch')
main=fn(pe,'unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)')
cases=re.findall(r'case\s+51\s*:(.*?)\bbreak\s*;',main,re.S)
if not any('CRagEffect::ColdHit(this)' in case for case in cases):raise SystemExit('PE effect51 caller missing')
if 'CRagEffect::ColdHit(this' in ref:raise SystemExit('Ref caller omission changed')
legacy=fn(local,'bool CRagEffect::ProcessEffectFrameLegacy()')
if 'if (m_type == 51)' not in legacy or 'SpawnColdHit();' not in legacy:raise SystemExit('effect51 local route missing')
handler=fn(local,'void CRagEffect::SpawnColdHit()')
for t in ('m_stateCnt == 0','angle < 360.0f','angle += 40.0f','PP_2DQUAD','PP_2DTEXTURE','m_stateCnt == 0 || m_stateCnt == 7','"effect\\\\ice.tga"','"effect\\\\smoke.tga"'):
    if t not in handler:raise SystemExit(f'ColdHit payload missing {t}')
quad=fn(local,'bool CEffectPrim::UsesReferenceScreenQuad() const')
texture=fn(local,'bool CEffectPrim::UsesReferenceScreenTexture() const')
if 'return m_type == PP_2DQUAD;' not in quad or 'm_master->GetEffectType() == 51' not in texture:raise SystemExit('ColdHit primitive family selectors missing')
print('ColdHit51 parity: PE/Ref helper, caller/ref omission, current route, nine-shard/smoke payload and type-wide quad/screen-texture routes passed')
