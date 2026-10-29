from pathlib import Path
import sys
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage  # noqa: E402

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


ref = function(ref_text, "bool\tCRagEffect :: CartRevolution()")
exe = function(exe_text[exe_text.index("unsigned __int8 __thiscall CRagEffect::CartRevolution(CRagEffect *this)\n{"):],
               "unsigned __int8 __thiscall CRagEffect::CartRevolution(CRagEffect *this)")
impl = function(src, "bool CRagEffect::ApplyCartRevolution()")
assert "EF_CARTREVOLUTION" in ref_header
assert "#if defined(BALANCE_PATCH)" in ref_text[ref_text.index("bool\tCRagEffect :: CartRevolution()") - 80:]
route = exe_text[exe_text.index("CRagEffect::CartRevolution(this)") - 160:exe_text.index("CRagEffect::CartRevolution(this)")]
assert "case 170:" in route

ordered(ref, [
    "if (m_stateCnt == 7 || m_stateCnt == 20)",
    'PlayWave("effect\\\\EF_MagnumBreak.wav", m_pos.x-pPos.x, 0,  m_pos.z-pPos.z)',
    "LaunchEffectPrim(PP_3DCIRCLE)", "m_duration = 20", "m_renderFlag |= RF_NODEPTHCHECK",
    "m_maxAlpha = 180", "m_latitude = 90", "m_radiusSpeed = 1.75f",
    "m_radiusAccel = -(g_prim->m_radiusSpeed / g_prim->m_duration) / 2.0f",
    "m_innerSize = 5.0f", "m_alpha = 0", "m_alphaSpeed = g_prim->m_maxAlpha / 15",
    "m_fadeOutCnt = g_prim->m_duration - 15", "InitSegment()",
    'GetTexture("effect\\\\ring_yellow.tga")', "LaunchEffectPrim(PP_3DSPHERE)",
    "m_duration = 20", "m_radiusSpeed = 1.35f",
    "m_radiusAccel = -(g_prim->m_radiusSpeed / g_prim->m_duration) / 2.0f",
    "m_longSpeed = 3.0f", "m_alpha = 0", "m_maxAlpha = 240",
    "m_alphaSpeed = g_prim->m_maxAlpha / 7", "m_fadeOutCnt = g_prim->m_duration - 15",
    'GetTexture("effect\\\\bigbang.tga")', "if (m_stateCnt >= 300)",
    "return false", "if (!ProcessEZ2STR())", "return true",
], "Ref CartRevolution")
ordered(exe, [
    "if ( m_stateCnt == 7 || m_stateCnt == 20 )", "PlayWave(aEffectEfMagnum, x, 0.0, z, 250, 40, 1.0)",
    "LaunchEffectPrim(this, PP_3DCIRCLE", "m_duration = 20", "BYTE1(m_renderFlag) |= 2u",
    "m_maxAlpha = 180.0", "m_latitude = 90.0", "m_radiusSpeed = 1.75",
    "m_radiusSpeed / (double)g_prim->m_duration * -0.5", "m_innerSize = 5.0",
    "m_alpha = 0.0", "m_alphaSpeed = g_prim->m_maxAlpha * 0.06666667",
    "m_fadeOutCnt = g_prim->m_duration - 15", "InitSegment(g_prim)", "aEffectRingYell",
    "LaunchEffectPrim(this, PP_3DSPHERE", "m_duration = 20", "m_radiusSpeed = 1.35",
    "m_radiusSpeed / (double)g_prim->m_duration * -0.5", "m_longSpeed = 3.0",
    "m_alpha = 0.0", "m_maxAlpha = 240.0", "m_alphaSpeed = g_prim->m_maxAlpha * 0.14285715",
    "m_fadeOutCnt = g_prim->m_duration - 15", "aEffect_27", "if ( this->m_stateCnt >= 300 )",
    "return 0", "ProcessEZ2STR(this, 0.0, -80.0, 0.0)", "return 1",
], "EXE CartRevolution")
ordered(impl, [
    "if (m_stateCnt == 7 || m_stateCnt == 20)", 'PlayWave("effect\\\\EF_MagnumBreak.wav"',
    "LaunchEffectPrim(PP_3DCIRCLE, vector3d{})", "circle->m_duration = 20",
    "circle->m_renderFlag |= 0x200", "circle->m_maxAlpha = 180.0f", "circle->m_latitude = 90.0f",
    "circle->m_radiusSpeed = 1.75f", "circle->m_radiusAccel = -(circle->m_radiusSpeed / circle->m_duration) / 2.0f",
    "circle->m_innerSize = 5.0f", "circle->m_alpha = 0.0f", "circle->m_alphaSpeed = circle->m_maxAlpha / 15",
    "circle->m_fadeOutCnt = circle->m_duration - 15", "ShiftCircleSegments(circle)",
    'GetTexture("effect\\\\ring_yellow.tga", false)', "LaunchEffectPrim(PP_3DSPHERE, vector3d{})",
    "sphere->m_duration = 20", "sphere->m_radiusSpeed = 1.35f",
    "sphere->m_radiusAccel = -(sphere->m_radiusSpeed / sphere->m_duration) / 2.0f",
    "sphere->m_longSpeed = 3.0f", "sphere->m_alpha = 0.0f", "sphere->m_maxAlpha = 240.0f",
    "sphere->m_alphaSpeed = sphere->m_maxAlpha / 7", "sphere->m_fadeOutCnt = sphere->m_duration - 15",
    'GetTexture("effect\\\\", false)', "if (m_stateCnt >= 300) return false",
    "ProcessEZ2STR();", "return true",
], "implementation CartRevolution")

assert "case 170:" in function(src, "void CRagEffect::Init(CRenderObject* master")
assert "if (m_type == 170) return ApplyCartRevolution();" in function(src, "bool CRagEffect::ProcessEffectFrameLegacy()")
assert "random(" not in ref and "rand()" not in ref and "SendMsg" not in ref
assert 'char aEffectEfMagnum[] = "effect\\\\EF_MagnumBreak.wav"' in exe_text
assert 'char aEffectRingYell[] = "effect\\\\ring_yellow.tga"' in exe_text
assert 'char aEffect_27[] = "effect\\\\"' in exe_text
print("PASS: Ref/EXE/current CartRevolution state branches, ordered payloads, effect-170 routes and wave bytes match; canonical texture symbol remains truncated to its verified prefix")
