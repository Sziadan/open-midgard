"""Compare Bottom2 effect137 PE/Ref helper and verify texture payload/order."""
from __future__ import annotations
import re
from pathlib import Path
root=Path(__file__).resolve().parents[2]
pe=(root/'HighPriest.exe.c').read_text(encoding='latin-1')
ref=(root/'Ref/RagEffect.cpp').read_text(encoding='latin-1')
local=(root/'src/world/RagEffect.cpp').read_text(encoding='utf-8')
header=(root/'src/world/RagEffect.h').read_text(encoding='utf-8')
dispatch=(root/'src/world/RagEffectOnProcessCases.generated.inc').read_text(encoding='utf-8')
def fn(text,sig):
    start=text.rindex(sig); opening=text.index('{',start); depth=0
    for i in range(opening,len(text)):
        if text[i]=='{': depth+=1
        elif text[i]=='}':
            depth-=1
            if not depth:return text[start:i+1]
    raise AssertionError(sig)
def compact(s):return re.sub(r'\s+','',s)
sig='void __thiscall CRagEffect::Bottom2(CRagEffect *this)'
pe_fn=fn(pe,sig); ref_fn=fn(ref,sig)
if compact(pe_fn)!=compact(ref_fn):raise SystemExit('PE/primary Ref Bottom2 helper mismatch')
main=fn(pe,'unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)')
if 'CRagEffect::Bottom2(this)' not in main[main.index('case 137:'):main.index('case 138:')]:raise SystemExit('canonical PE effect137 caller missing')
if 'CRagEffect::Bottom2(this' in ref:raise SystemExit('Ref caller omission changed; reevaluate PE route evidence')
if 'bool ProcessBottom2Effect137();' not in header or 'case 137: return ProcessBottom2Effect137();' not in dispatch:raise SystemExit('effect137 route missing')
handler=fn(local,'bool CRagEffect::ProcessBottom2Effect137()')
for t in ('m_stateCnt != 0','m_duration = 9999','static_cast<int>(180.0 - master->m_roty)','if (angle < 0) angle += 360','for (int side : {1, -1})','PP_3DTEXTURE','m_commonPattern = 1','m_pattern = 40','m_any = 50','rotation.m[2][0] * distance','rotation.m[0][0] * distance','side * 2.5f','m_longitude = static_cast<float>(angle + 90)','m_longitude = static_cast<float>(angle)','m_widthSize = 2.5f','m_heightSize = 15.0f','m_heightSpeed = 0.25f','m_alpha = 180.0f','m_fadeOutCnt = 9964','"effect\\\\magic_green.tga"'):
    if t not in handler:raise SystemExit(f'Bottom2 payload missing {t}')
if handler.count('LaunchEffectPrim(PP_3DTEXTURE')!=2:raise SystemExit('expected row3 then row1 launch per each of two sides')
if re.search(r'\b(?:rand|random)\s*\(',handler):raise SystemExit('unexpected Bottom2 RNG')
family=fn(local,'bool CEffectPrim::UsesReferenceTexture() const')
if 'm_master->GetEffectType() == 137' not in family:raise SystemExit('effect137 texture family route missing')
if 'char aEffectMagicGre[] = "effect\\\\magic_green.tga";' not in pe:raise SystemExit('canonical texture symbol changed')
print('Bottom2 137 parity: PE/Ref helper, caller/ref omission, route, four ordered textures, payload, no RNG and texture family passed')
