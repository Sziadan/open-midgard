"""Verify BowlingBash's PE/Ref helper and current primitive family binding."""
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
sig='void __thiscall CRagEffect::BowlingBash(CRagEffect *this)'
if compact(fn(pe,sig))!=compact(fn(ref,sig)):raise SystemExit('BowlingBash PE/primary Ref helper mismatch')
main=fn(pe,'unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)')
if 'CRagEffect::BowlingBash(this)' not in main[main.index('case 73:'):main.index('case 74:')]:raise SystemExit('PE effect73 caller missing')
if 'CRagEffect::BowlingBash(this' in ref:raise SystemExit('Ref caller omission changed')
handler=fn(local,'void CRagEffect::ApplyBowlingBash()')
for signature,token in (('bool CEffectPrim::UsesReferenceCircle() const','m_master->GetEffectType() == 73'),):
    if token not in fn(local,signature):raise SystemExit('effect73 circle family binding missing')
cylinder=fn(local,'bool CEffectPrim::UsesReferenceCylinder() const')
if not re.search(r'case\s+73\s*:',cylinder):raise SystemExit('effect73 cylinder family binding missing')
if 'if (m_type == 73)' not in local and 'case 73:' not in local:raise SystemExit('effect73 route missing')
print('BowlingBash73 parity: PE/Ref helper, canonical caller, Ref omission and circle/cylinder family bindings passed; payload remains covered by lifecycle tests')
