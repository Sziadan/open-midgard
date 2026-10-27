from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ref_text = (ROOT / "Ref/dhxj/RagEffect.cpp").read_text(encoding="cp1252")
ref_header = (ROOT / "Ref/dhxj/RagEffect.h").read_text(encoding="cp1252")
exe_text = (ROOT / "HighPriest.exe.c").read_text(encoding="cp1252")
src = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")

def function(text, marker, end=None):
    start = text.index(marker)
    if end:
        finish = text.index(end, start)
        return text[start:finish]
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

ref = function(ref_text, "void CRagEffect :: BowlingBash()", "void CRagEffect :: IceWall()")
exe = function(exe_text[exe_text.index("void __thiscall CRagEffect::BowlingBash(CRagEffect *this)\n{"):],
               "void __thiscall CRagEffect::BowlingBash(CRagEffect *this)")
impl = function(src, "void CRagEffect::ApplyBowlingBash()")
assert "EF_BOWLINGBASH" in ref_header
dispatch = exe_text[exe_text.index("CRagEffect::BowlingBash(this)") - 180:exe_text.index("CRagEffect::BowlingBash(this)")]
assert "case 73:" in dispatch

ordered(ref, [
    "if (m_stateCnt == 0 || m_stateCnt == 5)", "int angle = 180 - m_master->m_roty", "if (angle < 0)",
    "angle += 360", "LaunchEffectPrim(PP_3DCYLINDER)", "m_duration = 20 - m_stateCnt",
    "m_longitude = angle + m_stateCnt * 20", "MakeXRotation", "AppendYRotation",
    "m_outerSize = 8", "m_outerSpeed = 0.5f", "m_outerAccel = -0.03f", "m_innerSize = 3",
    "m_innerSpeed = 0.5f", "m_innerAccel = -0.03f", "m_heightSize = 3.5f",
    "m_alpha = 240 - m_stateCnt * 7", "m_fadeOutCnt = g_prim->m_duration - (g_prim->m_duration / 2)",
    'GetTexture("effect\\\\ring_blue.tga")', "if (m_stateCnt == 0)",
    "LaunchEffectPrim(PP_3DCIRCLE)", "m_duration = 50", "m_renderFlag |= RF_NODEPTHCHECK",
    "m_latitude = 90", "m_radius = 8.0f", "m_radiusSpeed = 0.7f",
    "m_radiusAccel = -(g_prim->m_radiusSpeed / 30)", "m_innerSize = 5.0f", "m_ChangePoint = 20",
    "m_ChangeAccel = 0", "m_ChangeSpeed = 0.08f", "m_alpha = 45", "m_fadeOutCnt = 35",
    "InitSegment()", 'GetTexture("effect\\\\ring_yellow.tga")',
], "Ref BowlingBash")
ordered(exe, [
    "if ( !m_stateCnt || m_stateCnt == 5 )", "180.0 - this->m_master->m_roty",
    "(int)v3 < 0 ? 0x168 : 0", "LaunchEffectPrim(this, PP_3DCYLINDER", "m_duration = 20 - this->m_stateCnt",
    "v4 + 20 * this->m_stateCnt", "matrix::MakeXRotation", "matrix::AppendYRotation",
    "m_outerSize = 8.0", "m_outerSpeed = 0.5", "m_outerAccel = -0.029999999", "m_innerSize = 3.0",
    "m_innerSpeed = 0.5", "m_innerAccel = -0.029999999", "m_heightSize = 3.5",
    "240 - 7 * this->m_stateCnt", "m_duration / 2", "aEffectRingBlue", "if ( !this->m_stateCnt )",
    "LaunchEffectPrim(this, PP_3DCIRCLE", "m_duration = 50", "BYTE1(m_renderFlag) |= 2u",
    "m_latitude = 90.0", "m_radius = 8.0", "m_radiusSpeed = 0.69999999",
    "m_radiusAccel = -(g_prim->m_radiusSpeed * 0.033333335)", "m_innerSize = 5.0",
    "m_ChangePoint = 20", "m_ChangeAccel = 0.0", "m_ChangeSpeed = 0.079999998",
    "m_alpha = 45.0", "m_fadeOutCnt = 35", "InitSegment(g_prim)", "aEffectRingYell",
], "EXE BowlingBash")

ordered(impl, [
    "if (m_stateCnt == 0 || m_stateCnt == 5)", "int angle = static_cast<int>(180.0f - m_master->m_roty)",
    "if (angle < 0) angle += 360", "LaunchEffectPrim(PP_3DCYLINDER, vector3d{})",
    "prim->m_duration = 20 - m_stateCnt", "angle + m_stateCnt * 20", "MatrixAppendXRotation",
    "MatrixAppendYRotation", "prim->m_outerSize = 8.0f", "prim->m_outerSpeed = 0.5f",
    "prim->m_outerAccel = -0.03f", "prim->m_innerSize = 3.0f", "prim->m_innerSpeed = 0.5f",
    "prim->m_innerAccel = -0.03f", "prim->m_heightSize = 3.5f",
    "240 - m_stateCnt * 7", "prim->m_duration - prim->m_duration / 2",
    'GetTexture("effect\\\\ring_blue.tga", false)', "if (m_stateCnt == 0)",
    "LaunchEffectPrim(PP_3DCIRCLE, vector3d{})", "prim->m_duration = 50", "prim->m_renderFlag |= 0x200",
    "prim->m_latitude = 90.0f", "prim->m_radius = 8.0f", "prim->m_radiusSpeed = 0.7f",
    "prim->m_radiusAccel = -(prim->m_radiusSpeed / 30.0f)", "prim->m_innerSize = 5.0f",
    "prim->m_changePoint = 20", "prim->m_changeAccel = 0.0f", "prim->m_changeSpeed = 0.08f",
    "prim->m_alpha = 45.0f", "prim->m_fadeOutCnt = 35", "ShiftCircleSegments(prim)",
    'GetTexture("effect\\\\ring_yellow.tga", false)',
], "implementation BowlingBash")
assert "rand()" not in ref and "random(" not in ref
assert "m_type == 73" in function(src, "bool CRagEffect::ProcessEffectFrame()")
assert "case 73:" in function(src, "void CRagEffect::Init(")
print("PASS: Ref, EXE and current BowlingBash state branches, payload order, textures, and effect-73 route match")
