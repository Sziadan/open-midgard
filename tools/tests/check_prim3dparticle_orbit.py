from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
ref_text = (ROOT / "Ref/dhxj/RagEffectPrim.cpp").read_text(encoding="cp1252")
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
    offsets = [body.index(x) for x in steps]
    assert offsets == sorted(offsets), f"{label} order mismatch"

ref = function(ref_text, "bool\tCEffectPrim :: Prim3DParticleOrbit()")
exe = function(exe_text[exe_text.index("//----- (00642CE0)"):], "bool __thiscall CEffectPrim::Prim3DParticleOrbit(CEffectPrim *this)")
impl = function(src, "bool UpdateReferenceOrbitParticle(CEffectPrim* prim)")

ordered(ref, [
    "m_orgPos = m_master->m_pos", "m_radiusSpeed += m_radiusAccel", "m_gravSpeed += m_gravAccel",
    "m_longSpeed += m_longAccel", "m_radius += m_radiusSpeed", "m_longitude += m_longSpeed",
    "m_deltaPos2.y += m_gravSpeed", "m_orgPos += m_deltaPos2", "m_roll += m_rollSpeed",
    "if (m_stateCnt == m_ChangePoint)", "if (m_chRadiSpeed)", "m_radiusSpeed = m_chVal1[m_chCnt]",
    "m_radiusAccel = m_chVal2[m_chCnt]", "m_chCnt++", "m_matrix.MakeYRotation(GetRadian(m_longitude))",
    "m_deltaPos.MatrixMult(vector3d(0, 0, m_radius), m_matrix)", "m_pos = m_orgPos + m_deltaPos",
    "m_size += m_sizeSpeed", "m_latitude += m_latiSpeed", "m_matrix.AppendZRotation(m_latitude)",
    "ProcessAlpha()", "ShiftSegment()", "if (!AnimMotion())",
], "Ref orbit")
ordered(exe, [
    "this->m_master->m_pos", "this->m_radiusSpeed + this->m_radiusAccel", "this->m_gravSpeed + this->m_gravAccel",
    "this->m_longAccel + this->m_longSpeed", "v2 + this->m_radius", "v19 + this->m_longitude",
    "v17 + this->m_deltaPos2.y", "this->m_deltaPos2.x + this->m_orgPos.x", "this->m_rollSpeed + this->m_roll",
    "m_stateCnt == m_ChangePoint", "this->m_chRadiSpeed", "this->m_radiusSpeed = this->m_chVal1._First[v8 - 1]",
    "this->m_chCnt = v8", "this->m_radiusAccel = v9", "matrix::MakeYRotation",
    "m_radius * this->m_matrix.v31", "v11 = this->m_deltaPos.x + this->m_orgPos.x",
    "this->m_sizeSpeed + this->m_size", "this->m_latiSpeed + this->m_latitude",
    "matrix::AppendZRotation", "CEffectPrim::ProcessAlpha(this)",
    "CEffectPrim::ShiftSegment(this)", "CEffectPrim::AnimMotion(this) != 0",
], "EXE orbit")
ordered(impl, [
    "prim->m_orgPos = prim->m_master->EffectPosition()", "prim->m_radiusSpeed += prim->m_radiusAccel",
    "prim->m_gravSpeed += prim->m_gravAccel", "prim->m_longSpeed += prim->m_longAccel",
    "prim->m_radius += prim->m_radiusSpeed", "prim->m_longitude += prim->m_longSpeed",
    "prim->m_deltaPos2.y += prim->m_gravSpeed", "prim->m_orgPos, prim->m_deltaPos2",
    "prim->m_roll += prim->m_rollSpeed", "if (prim->m_stateCnt == prim->m_changePoint)",
    "if (prim->m_chRadiSpeed)", "prim->m_radiusSpeed = prim->m_chVal1[prim->m_chCnt]",
    "prim->m_radiusAccel = prim->m_chVal2[prim->m_chCnt]", "++prim->m_chCnt",
    "MatrixAppendYRotation", "prim->m_radius * prim->m_matrix.m[2][0]",
    "prim->m_pos = AddVec3(prim->m_orgPos, prim->m_deltaPos)", "prim->m_size += prim->m_sizeSpeed",
    "prim->m_latitude += prim->m_latiSpeed", "MatrixAppendZRotation",
    "ProcessReferenceParticleAlpha(prim)", "ShiftCircleSegments(prim)", "return AnimateReferenceParticle(prim)",
], "implementation orbit")

route = function(src, "bool CEffectPrim::ProcessPrimitiveFrame()")
start = route.index("if (m_type == PP_3DPARTICLEORBIT)")
end = route.index("if (m_type == PP_3DPARTICLE)", start)
assert "return UpdateReferenceOrbitParticle(this);" in route[start:end]
assert "UsesReferenceParticle()" not in route[start:end]
print("PASS: Ref, EXE and implementation match across 23 ordered orbit operations, keyed-radius branches and false-return dispatch")


