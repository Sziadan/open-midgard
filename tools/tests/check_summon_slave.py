from pathlib import Path
import re
root=Path(__file__).resolve().parents[2]
ref=(root/'Ref/dhxj/RagEffect.cpp').read_text(encoding='latin-1')
header=(root/'Ref/dhxj/RagEffect.h').read_text(encoding='latin-1')
exe=(root/'HighPriest.exe.c').read_text(encoding='latin-1')
impl=(root/'src/world/RagEffect.cpp').read_text(encoding='utf-8')
def body(text,start,end):
    b=text.index(end); return text[text.rindex(start,0,b):b]
def ordered(label,text,tokens):
    cursor=0
    for token in tokens:
        pos=text.find(token,cursor)
        if pos<0: raise SystemExit(f'FAIL: {label} missing/out of order: {token}')
        cursor=pos+len(token)
ids=re.findall(r'\bEF_[A-Z0-9_]+\b',header[header.index('EF_PONG'):header.index('EF_ENERGYDRAIN')])
if 199+ids.index('EF_SUMMONSLAVE')!=215: raise SystemExit('FAIL: Ref enum ordering does not derive SummonSlave=215')
if 'EFFECT\\\\alpha_center.tga' not in exe[exe.index('char aEffectAlphaCen_0'):exe.index('char aEffectAlphaCen_0')+120]:
    raise SystemExit('FAIL: canonical alpha-center texture symbol is not exact')
if 'if (m_type == 215) {\n        SpawnSummonSlave();\n        return true;\n    }' not in impl:
    raise SystemExit('FAIL: current ProcessEffectFrame lacks effect ID215 route')
ref_fn=body(ref,'void\tCRagEffect :: SummonSlave()','void\tCRagEffect :: BloodDrain()')
exe_fn=body(exe,'void __thiscall CRagEffect::SummonSlave(CRagEffect *this)','//----- (00604730)')
cur_fn=body(impl,'void CRagEffect::SpawnSummonSlave()','// Ref/RagEffect.cpp:8826-8852')
ordered('Ref',ref_fn,['TransformVert(gameMode->GetView()->GetViewMatrix(), m_master->m_pos)','m_tlvertX = tlvert.x','m_tlvertY = tlvert.y','if (m_stateCnt == 0)','for (int i = 0; i < 20; i++)','LaunchEffectPrim(PP_2DFLASH)','m_duration = 40','m_deltaPos2.y = -40','random(360)','random(60) + 10','-(g_prim->m_longSpeed / g_prim->m_duration) / 1.5f','random(25) + 5','random(40) + 20','random(30) + 20','m_alpha = 0','m_maxAlpha = 200','m_alphaSpeed = g_prim->m_maxAlpha / 10.0f','m_fadeOutCnt = g_prim->m_duration - (g_prim->m_duration / 3)','EFFECT\\\\alpha_center.tga','} else if (m_stateCnt == 35)','SendMsg(NULL, AM_MAKE_SMOKE_FFECT)'])
ordered('EXE',exe_fn,['ProjectVertex','this->m_tlvertX = vert.x','this->m_tlvertY = y','if ( m_stateCnt )','if ( m_stateCnt == 35 )','m_master->CGameObject::SendMsg','v13 = 20','LaunchEffectPrim(this, PP_2DFLASH','m_duration = 40','m_deltaPos2.y = -40.0','rand() % 360','rand() % 60 + 10','* -0.66666669','rand() % 25 + 5','rand() % 40 + 20','rand() % 30 + 20','m_alpha = 0.0','m_maxAlpha = 200.0','m_alphaSpeed = g_prim->m_maxAlpha * 0.1','m_fadeOutCnt = g_prim->m_duration - g_prim->m_duration / 3','aEffectAlphaCen_0'])
ordered('current',cur_fn,['GetCurrentGameMode()','ResolveCurrentMaster()','ProjectReferenceEffectVertex(master->m_pos, mode->m_view->GetViewMatrix(), projected)','m_tlvertX = projected.x','m_tlvertY = projected.y','if (m_stateCnt == 0)','for (int i = 0; i < 20; ++i)','LaunchEffectPrim(PP_2DFLASH, vector3d{})','m_duration = 40','m_deltaPos2.y = -40.0f','rand() % 360','rand() % 60 + 10','-(prim->m_longSpeed / prim->m_duration) / 1.5f','rand() % 25 + 5','rand() % 40 + 20','rand() % 30 + 20','m_alpha = 0.0f','m_maxAlpha = 200.0f','m_alphaSpeed = prim->m_maxAlpha / 10.0f','m_fadeOutCnt = prim->m_duration - prim->m_duration / 3','EFFECT\\\\alpha_center.tga','m_stateCnt == 35 && master','master->SendMsg(nullptr, 0, 63, 0, 0)'])
for label,fn in [('Ref',ref_fn),('EXE',exe_fn),('current',cur_fn)]:
    count=len(re.findall(r'\b(?:rand|random)\s*\(',fn))
    if count!=5: raise SystemExit(f'FAIL: {label} per-particle RNG count expected 5, got {count}')
print('PASS: SummonSlave Ref/EXE/current branch order, derived ID215, 20x5 RNG draws, projection, texture and state35 message match')
