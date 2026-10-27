from pathlib import Path

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
            if depth == 0:
                return text[start:i + 1]
    raise AssertionError(f"unterminated {marker}")


def ordered(body, steps, label):
    cursor = 0
    for step in steps:
        try:
            cursor = body.index(step, cursor) + len(step)
        except ValueError:
            raise AssertionError(f"{label} missing/out of order: {step}")


ref = function(ref_text, "void\tCRagEffect :: VenomDust2()")
exe = function(exe_text[exe_text.index("void __thiscall CRagEffect::VenomDust2(CRagEffect *this)\n{"):],
               "void __thiscall CRagEffect::VenomDust2(CRagEffect *this)")
impl = function(src, "void CRagEffect::ApplyVenomDust2()")
assert "EF_VENOMDUST2" in ref_header
route = exe_text[exe_text.index("CRagEffect::VenomDust2(this)") - 120:exe_text.index("CRagEffect::VenomDust2(this)")]
assert "case 171:" in route

ordered(ref, [
    "if (m_stateCnt % 5 == 0)", "int a, r = 1", "for(a = 0; a < r; a++)",
    "LaunchEffectPrim(PP_3DPARTICLE)", "m_duration = 10",
    "MakeYRotation(GetRadian(random(360)))", "MakeXRotation(GetRadian(90))",
    "random(20) + 20", "/ 80", "m_accel = -(g_prim->m_speed / g_prim->m_duration) / 2.0f",
    "m_size = (float)(80) / 100", "m_rollSpeed = 3.0f", "m_alpha = 70",
    "m_alphaSpeed = g_prim->m_maxAlpha / 10.0f", "m_fadeOutCnt = g_prim->m_duration - (g_prim->m_duration / 5)",
    'SetSprName("misc\\\\particle3.spr")', 'SetActName("misc\\\\particle3.act")', "InitSegment()",
], "Ref VenomDust2")
ordered(exe, [
    "if ( !(this->m_stateCnt % 5) )", "LaunchEffectPrim(this, PP_3DPARTICLE", "m_duration = 10",
    "v1 = rand()", "v1 % 360", "MakeYRotation", "GetRadian(90)", "MakeXRotation",
    "rand() % 20 + 20", "* 0.0125", "m_speed / (double)g_prim->m_duration * -0.5",
    "m_size = 0.80000001", "m_rollSpeed = 3.0", "m_alpha = 70.0",
    "m_alphaSpeed = g_prim->m_maxAlpha * 0.1", "m_duration / 5", "byte_71BC24",
    "byte_71BC0C", "InitSegment(g_prim)",
], "EXE VenomDust2")
ordered(impl, [
    "if (m_stateCnt % 5 != 0) return", "LaunchEffectPrim(PP_3DPARTICLE, vector3d{})",
    "prim->m_duration = 10", "rand() % 360", "MatrixAppendYRotation", "MatrixAppendXRotation",
    "rand() % 20 + 20", "/ 80.0f", "prim->m_accel = -(prim->m_speed / prim->m_duration) / 2.0f",
    "prim->m_size = 0.8f", "prim->m_rollSpeed = 3.0f", "prim->m_alpha = 70.0f",
    "prim->m_alphaSpeed = prim->m_maxAlpha / 10.0f", "prim->m_fadeOutCnt = prim->m_duration - prim->m_duration / 5",
    'SetParticleResources("\\xC0\\xCC\\xC6\\xD1\\xC6\\xAE\\\\particle3.spr"',
    '"\\xC0\\xCC\\xC6\\xD1\\xC6\\xAE\\\\particle3.act"', "ShiftCircleSegments(prim)",
], "implementation VenomDust2")
assert "case 171:" in function(src, "void CRagEffect::Init(CRenderObject* master")
assert "if (m_type == 171)" in function(src, "bool CRagEffect::ProcessEffectFrame()")
assert "rand()" not in ref and "random(360)" in ref and "random(20)" in ref
assert 'char byte_71BC24 = \'\\xC0\'' in exe_text
assert 'char byte_71BC0C = \'\\xC0\'' in exe_text
print("PASS: Ref/EXE/current VenomDust2 cadence, one-particle loop, two-draw order, payload and effect-171 routes match; opaque EXE resources map to verified particle3 callsite bytes")

