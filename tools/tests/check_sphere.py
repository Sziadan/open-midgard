from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
ref_text = (ROOT / "Ref/dhxj/RagEffect.cpp").read_text(encoding="cp1252")
ref_header = (ROOT / "Ref/dhxj/RagEffect.h").read_text(encoding="cp1252")
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
    raise AssertionError(f"unterminated {marker}")

def ordered(body, steps, label):
    cursor = 0
    for step in steps:
        try: cursor = body.index(step, cursor) + len(step)
        except ValueError: raise AssertionError(f"{label} missing/out of order: {step}")

ref = function(ref_text, "void CRagEffect :: Sphere()")
exe = function(exe_text[exe_text.index("void __thiscall CRagEffect::Sphere(CRagEffect *this)\n{"):],
               "void __thiscall CRagEffect::Sphere(CRagEffect *this)")
impl = function(src, "void CRagEffect::ApplySphere()")
assert "EF_SPHERE" in ref_header

ordered(ref, [
    "m_pos = m_master->m_pos", "if (m_stateCnt % 10 == 0)", "LaunchEffectPrim(PP_3DPARTICLEORBIT)",
    "m_duration = 500 - m_stateCnt", "m_duration = 300", "m_radius = 0.1f",
    "m_radiusSpeed = 0.15f", "m_gravSpeed = 0.15f", "m_longitude = 2", "m_longSpeed = 8.0f",
    "m_longAccel = -0.01f", "m_size = 1.2f", "MakeYRotation(GetRadian((int)g_prim->m_longitude))",
    "MatrixMult(vector3d(0, -30, g_prim->m_radius)", "m_chRadiSpeed = TRUE", "m_chCnt = 0",
    "m_chPoint.push_back(95)", "m_chVal1.push_back((float)-0.15)",
    "m_chVal2.push_back(g_prim->m_radiusAccel)", 'SetSprName("misc\\\\particle1.spr")',
    'SetActName("misc\\\\particle1.act")', "InitSegment()",
], "Ref Sphere")
ordered(exe, [
    "this->m_pos = this->m_master->m_pos", "!(this->m_stateCnt % 10)",
    "LaunchEffectPrim(this, PP_3DPARTICLEORBIT", "this->m_duration = 500 - this->m_stateCnt",
    "g_prim->m_duration = 300", "g_prim->m_radius = 0.1", "g_prim->m_radiusSpeed = 0.15000001",
    "g_prim->m_gravSpeed = 0.15000001", "g_prim->m_longitude = 2.0", "g_prim->m_longSpeed = 8.0",
    "g_prim->m_longAccel = -0.0099999998", "g_prim->m_size = 1.2", "GetRadian((__int64)g_prim->m_longitude)",
    "matrix::MakeYRotation", "_X = 1119748096", "g_prim->m_deltaPos2.x =",
    "g_prim->m_chRadiSpeed = 1", "g_prim->m_chCnt = 0", "m_chPoint",
    "_X = -1105618534", "m_chVal1",
    "p_m_radiusAccel = &g_prim->m_radiusAccel", "SetSprName(g_prim, &byte_71B200)",
    "SetActName(g_prim, &byte_71B1E8)", "CEffectPrim::InitSegment(g_prim)",
], "EXE Sphere")
assert "GetTexture" not in re.sub(r"//.*", "", ref)
ordered(impl, [
    "m_effectPos = m_master->m_pos", "if (m_stateCnt % 10 != 0) return",
    "LaunchEffectPrim(PP_3DPARTICLEORBIT, vector3d{})", "m_duration = 500 - m_stateCnt",
    "prim->m_duration = 300", "prim->m_radius = 0.1f", "prim->m_radiusSpeed = 0.15f",
    "prim->m_gravSpeed = 0.15f", "prim->m_longitude = 2.0f", "prim->m_longSpeed = 8.0f",
    "prim->m_longAccel = -0.01f", "prim->m_size = 1.2f", "MatrixAppendYRotation",
    "prim->m_deltaPos2 = {", "prim->m_chRadiSpeed = true", "prim->m_chCnt = 0",
    "prim->m_chPoint.push_back(95.0f)", "prim->m_chVal1.push_back(-0.15f)",
    "prim->m_chVal2.push_back(prim->m_radiusAccel)", 'SetParticleResources("\\xC0',
    "ShiftCircleSegments(prim)",
], "implementation Sphere")
route = function(src, "bool CRagEffect::ProcessEffectFrame()")
assert "if (m_type == 72)" in route and "ApplySphere();" in route
assert "case 72:" in function(src, "void CRagEffect::Init(")
print("PASS: Ref, EXE and current Sphere cadence, keyed-radius schedule, payload and dispatch match")
