from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
ref_text = (ROOT / "Ref/dhxj/RagEffect.cpp").read_text(encoding="cp1252")
exe_text = (ROOT / "HighPriest.exe.c").read_text(encoding="cp1252")
src = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")

def function(text, marker):
    start = text.index(marker)
    brace = text.index("{", start)
    depth = 0
    for i in range(brace, len(text)):
        if text[i] == "{": depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0: return text[start:i + 1]
    raise AssertionError(f"unterminated: {marker}")

def ordered(body, steps, label):
    missing = [x for x in steps if x not in body]
    assert not missing, f"{label} missing: {missing}"
    offsets = []
    cursor = 0
    for step in steps:
        cursor = body.index(step, cursor)
        offsets.append(cursor)
        cursor += len(step)
    assert offsets == sorted(offsets), f"{label} order mismatch"

ref = function(ref_text, "void CRagEffect::Sightrasher()")
exe = function(exe_text[exe_text.index("void __thiscall CRagEffect::Sightrasher(CRagEffect *this)\n{"):],
               "void __thiscall CRagEffect::Sightrasher(CRagEffect *this)")
impl = function(src, "void CRagEffect::ApplySightrasher()")

ordered(ref, [
    "TransformVert(gameMode->GetView()->GetViewMatrix(), m_master->m_pos)",
    "if (m_stateCnt == 0)", "m_master->SendMsg(NULL, AM_DELETE_EFFECT, EF_SIGHT)",
    'PlayWave("effect\\\\wizard_sightrasher.wav"',
    "if (m_stateCnt % 5 == 0 && m_stateCnt < 20)",
    "for (a = 0; a < 8; a++)", "g_prim->m_pattern |= PT_DELTASPEED",
    "g_prim->m_animSpeed = 3 + random(2)", "g_prim->m_ChangeSpeedx = 0.5 * sn",
    "g_prim->m_ChangePoint = 35 - m_stateCnt", "g_prim->m_ChangeAccelx = -0.004f",
    'g_prim->SetSprName("misc\\\\Sight.spr")', "for (a = 0; a < 8; a++)",
    'g_prim->SetSprName("Shadow.spr")', "if (m_stateCnt == 10)",
    "g_prim = LaunchEffectPrim(PP_3DCIRCLE)", "g_prim->m_texture[0] = g_texMgr.GetTexture",
], "Ref Sightrasher")

# The decompiler expresses the same four-frame emission predicate directly;
# check its two child loops, one RNG site, change payload and terminal circle.
assert "m_stateCnt % 5 == 0" in ref and "m_stateCnt < 20" in ref, "Ref missing emission gate"
assert "!(this->m_stateCnt % 5)" in exe and "this->m_stateCnt < 20" in exe, "EXE missing emission gate"
for label, body in (("Ref", ref), ("EXE", exe)):
    assert body.count("a < 8") >= 2, f"{label} missing eight-child loops"
    assert "m_ChangePoint" in body and "m_ChangeAccelx" in body and "m_ChangeSpeedx" in body
    assert "m_stateCnt == 10" in body and "PP_3DCIRCLE" in body
assert exe.count("rand() % 2") == 1, "EXE should consume exactly one RNG draw per first-group child"
assert exe.count("rand() % 2") == 1 and "random(2)" in ref, "Ref/EXE RNG profile mismatch"

ordered(impl, [
    "ProjectReferenceEffectVertex(m_master->m_pos", "if (m_stateCnt == 0)",
    "m_master->SendMsg(nullptr, 109, 22, 0, 0)", 'TryPlayEffectWaveAt(m_effectPos, "effect\\\\wizard_sightrasher.wav")',
    "if (m_stateCnt % 5 == 0 && m_stateCnt < 20)", "for (int i = 0; i < 8; ++i)",
    "prim->m_pattern |= 0x400", "prim->m_animSpeed = 3 + rand() % 2",
    "prim->m_changeSpeed3d =", "prim->m_changePoint = 35 - m_stateCnt",
    'SetParticleResources("misc\\\\Sight.spr"', "for (int i = 0; i < 8; ++i)",
    'SetParticleResources("Shadow.spr"', "if (m_stateCnt == 10)",
    "LaunchEffectPrim(PP_3DCIRCLE", 'GetTexture("effect\\\\ring_yellow.tga", false)',
], "implementation Sightrasher")
assert "if (m_type == 62)" in function(src, "bool CRagEffect::ProcessEffectFrame()")
print("PASS: Ref, EXE and implementation match Sightrasher gates, ordered particle payloads, RNG, resources and dispatch")


