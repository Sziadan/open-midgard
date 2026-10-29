"""Compare Bottom (effect114) PE/Ref helper and verify primitive order and payload."""
from __future__ import annotations
import re
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from ro_exe_resolve import ExeImage  # noqa: E402
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
sig='void __thiscall CRagEffect::Bottom(CRagEffect *this)'
pe_fn=fn(pe,sig); ref_fn=fn(ref,sig)
if compact(pe_fn)!=compact(ref_fn):raise SystemExit('PE/primary Ref Bottom helper mismatch')
main=fn(pe,'unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)')
if 'CRagEffect::Bottom(this)' not in main[main.index('case 114:'):main.index('case 115:')]:raise SystemExit('canonical PE effect114 caller missing')
if 'CRagEffect::Bottom(this' in ref:raise SystemExit('Ref caller omission changed; reevaluate PE route evidence')
if 'bool ProcessBottom114();' not in header or 'case 114: return ProcessBottom114();' not in dispatch:raise SystemExit('effect114 route missing')
handler=fn(local,'bool CRagEffect::ProcessBottom114()')
for t in ('m_stateCnt != 0','m_duration = 9999','static_cast<int>(180.0 - master->m_roty)','if (angle < 0) angle += 360','for (int side : {1, -1})','PP_3DTEXTURE','m_commonPattern = 1','m_pattern = 40','m_any = 50','rotation.m[2][0] * distance','rotation.m[0][0] * distance','m_longitude = static_cast<float>(angle + 90)','m_longitude = static_cast<float>(angle)','m_widthSize = 5.0f','m_heightSize = 15.0f','m_heightSpeed = 0.25f','m_maxAlpha = 180.0f','m_fadeOutCnt = cross->m_duration - 35','"effect\\\\magic_violet.tga"'):
    if t not in handler:raise SystemExit(f'Bottom payload missing {t}')
if handler.count('LaunchEffectPrim(PP_3DTEXTURE')!=2:raise SystemExit('expected two ordered texture launches per loop iteration')
if re.search(r'\b(?:rand|random)\s*\(',handler):raise SystemExit('unexpected Bottom RNG')
family=fn(local,'bool CEffectPrim::UsesReferenceTexture() const')
if 'm_master->GetEffectType() == 114' not in family:raise SystemExit('effect114 texture family route missing')
if 'char aEffectMagicVio[] = "effect\\\\magic_violet.tga";' not in pe:raise SystemExit('canonical texture symbol changed')
resource=ExeImage(root/'HighPriest.exe').read_cstring_at(0x71BAB8)
if resource is None or resource.raw != b'effect\\magic_violet.tga':raise SystemExit('effect\\magic_violet.tga PE bytes differ at VA 0x71BAB8')
print('Bottom114 parity: PE/Ref helper, caller/ref omission, route, four ordered textures, payload, no RNG, texture family and exact PE resource bytes passed')
