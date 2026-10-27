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

ref = function(ref_text, "void CRagEffect :: Cone()")
exe = function(exe_text[exe_text.index("void __thiscall CRagEffect::Cone(CRagEffect *this)\n{"):],
               "void __thiscall CRagEffect::Cone(CRagEffect *this)")
impl = function(src, "void CRagEffect::ApplyCone()")

assert "EF_CONE" in ref_header
assert "case 71" in exe_text[exe_text.index("CRagEffect::Cone(this)") - 160:exe_text.index("CRagEffect::Cone(this)")]
ordered(ref, [
    "if (m_stateCnt % 130 == 0)", "LaunchEffectPrim(PP_3DPARTICLEORBIT)", "m_duration = 200",
    "m_duration = 300", "m_radius = 0.1f", "m_radiusSpeed = 0.2f", "m_gravSpeed = 0.1f",
    "m_gravAccel = 0.005f", "m_longitude = 2", "m_longSpeed = 15.0f", "m_longAccel = -0.01f",
    "m_size = 1.2f", "MakeYRotation(GetRadian((int)g_prim->m_longitude))",
    "MatrixMult(vector3d(0, -30, g_prim->m_radius)", 'SetSprName("misc\\\\particle1.spr")',
    'SetActName("misc\\\\particle1.act")', "InitSegment()",
], "Ref Cone")
ordered(exe, [
    "!(this->m_stateCnt % 130)", "LaunchEffectPrim(this, PP_3DPARTICLEORBIT", "this->m_duration = 200",
    "g_prim->m_duration = 300", "g_prim->m_radius = 0.1", "g_prim->m_radiusSpeed = 0.2",
    "g_prim->m_gravSpeed = 0.1", "g_prim->m_gravAccel = 0.0049999999", "g_prim->m_longitude = 2.0",
    "g_prim->m_longSpeed = 15.0", "g_prim->m_longAccel = -0.0099999998", "g_prim->m_size = 1.2",
    "GetRadian((__int64)g_prim->m_longitude)", "matrix::MakeYRotation", "m_matrix.v31 - g_prim->m_matrix.v21 * 30.0",
    "CRenderObject::SetSprName(g_prim, &byte_71B200)", "CRenderObject::SetActName(g_prim, &byte_71B1E8)",
    "CEffectPrim::InitSegment(g_prim)",
], "EXE Cone")
ref_active = re.sub(r"//.*", "", ref)
exe_active = re.sub(r"//.*", "", exe)
assert "GetTexture" not in ref_active and "rand()" not in exe_active and "random(" not in ref_active
ordered(impl, [
    "if (m_stateCnt % 130 != 0) return", "LaunchEffectPrim(PP_3DPARTICLEORBIT, vector3d{})",
    "m_duration = 200", "prim->m_duration = 300", "prim->m_radius = 0.1f",
    "prim->m_radiusSpeed = 0.2f", "prim->m_gravSpeed = 0.1f", "prim->m_gravAccel = 0.005f",
    "prim->m_longitude = 2.0f", "prim->m_longSpeed = 15.0f", "prim->m_longAccel = -0.01f",
    "prim->m_size = 1.2f", "MatrixAppendYRotation", "prim->m_deltaPos2 = {",
    'SetParticleResources("\\xC0', "ShiftCircleSegments(prim)",
], "implementation Cone")
route = function(src, "bool CRagEffect::ProcessEffectFrame()")
assert "if (m_type == 71)" in route and "ApplyCone();" in route
assert "case 71:" in function(src, "void CRagEffect::Init(")
print("PASS: Ref, EXE and current Cone effect ID, cadence, ordered payload, transform, resources and route match")
