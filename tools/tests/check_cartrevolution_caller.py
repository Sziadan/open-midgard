"""Compare CartRevolution's PE/Ref helper, routed implementation and ambiguous texture bytes."""
from __future__ import annotations
import re,sys
from pathlib import Path
root=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(root/'tools'))
from ro_exe_resolve import ExeImage,SymbolTable
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
sig='unsigned __int8 __thiscall CRagEffect::CartRevolution(CRagEffect *this)'
pe_fn=fn(pe,sig);ref_fn=fn(ref,sig)
if compact(pe_fn)!=compact(ref_fn):raise SystemExit('CartRevolution PE/primary Ref helper mismatch')
main=fn(pe,'unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)')
if 'CRagEffect::CartRevolution(this)' not in main[main.index('case 170:'):main.index('case 171:')]:raise SystemExit('PE effect170 caller missing')
if 'CRagEffect::CartRevolution(this' in ref:raise SystemExit('Ref caller omission changed')
handler=fn(local,'bool CRagEffect::ApplyCartRevolution()')
for t in ('m_stateCnt == 7 || m_stateCnt == 20','PP_3DCIRCLE','PP_3DSPHERE','m_radiusSpeed = 1.75f','m_radiusSpeed = 1.35f','m_longSpeed = 3.0f','m_alphaSpeed = circle->m_maxAlpha / 15','m_alphaSpeed = sphere->m_maxAlpha / 7','m_fadeOutCnt = circle->m_duration - 15','m_fadeOutCnt = sphere->m_duration - 15','"effect\\\\ring_yellow.tga"','"effect\\\\"','m_stateCnt >= 300','ProcessEZ2STR()'):
    if t not in handler:raise SystemExit(f'CartRevolution payload missing {t}')
if re.search(r'\b(?:rand|random)\s*\(',handler):raise SystemExit('unexpected CartRevolution RNG')
circle=fn(local,'bool CEffectPrim::UsesReferenceCircle() const')
sphere=fn(local,'bool CEffectPrim::UsesReferenceSphere() const')
if 'm_master->GetEffectType() == 170' not in circle or 'm_master->GetEffectType() == 170' not in sphere:raise SystemExit('effect170 circle/sphere reference families missing')
resolved=SymbolTable(ExeImage()).resolve_name('aEffect_27')
if resolved is None or resolved.raw!=b'effect\\':raise SystemExit(f'canonical aEffect_27 bytes changed: {resolved!r}')
print(f'CartRevolution170 parity: PE/Ref helper, PE caller/Ref omission, payload, no RNG, circle/sphere routes and exact texture bytes {resolved.raw!r} passed')
