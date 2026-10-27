from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
ref = (ROOT / "Ref/dhxj/RagEffectPrim.cpp").read_text(encoding="cp1252")
exe = (ROOT / "HighPriest.exe.c").read_text(encoding="cp1252")
current = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")

def body(text, signature, next_signature):
    start = text.index(signature)
    end = text.index(next_signature, start)
    return text[start:end]

r = body(ref, "bool\tCEffectPrim :: Prim3DParticleSpline()", "bool\tCEffectPrim :: Prim3DTexture()")
exe_start = exe.index("//----- (006428C0)")
e = body(exe[exe_start:], "bool __thiscall CEffectPrim::Prim3DParticleSpline(CEffectPrim *this)", "//----- (00642AB0)")

# Pair each reference operation with its canonical executable equivalent, in order.
ref_steps = [
    "m_orgPos = m_master->m_pos + m_deltaPos2", "m_speed += m_accel",
    "m_latitude += m_latiSpeed", "m_latiSpeed += m_latiAccel",
    "m_gravSpeed += m_gravAccel", "m_speed3d.MatrixMult(vector3d(0.0f, 0.0f, m_speed - m_gravSpeed), m_matrix)",
    "m_rotMat.MakeTransform(m_rotAxis, GetRadian(m_roll))",
    "m_deltaPos3.MatrixMult(vector3d(0, m_latitude, 0), m_rotMat)", "m_size += m_sizeSpeed",
    "m_deltaPos -= m_speed3d", "m_pos = m_orgPos + m_deltaPos + m_deltaPos3",
    "ProcessAlpha", "ShiftSegment", "if (!AnimMotion())",
]
exe_steps = [
    "this->m_deltaPos2.x + m_master->m_pos.x", "this->m_accel + this->m_speed",
    "this->m_latitude = this->m_latitude + this->m_latiSpeed",
    "this->m_latiAccel + this->m_latiSpeed", "this->m_gravAccel + this->m_gravSpeed",
    "v8 * this->m_matrix.v31 + this->m_matrix.v41",
    "matrix::MakeTransform(p_m_rotMat, this->m_rotAxis, rotangle)",
    "m_latitude * p_m_rotMat->v21 + p_m_rotMat->v41", "this->m_sizeSpeed + this->m_size",
    "this->m_deltaPos.x - this->m_speed3d.x", "this->m_orgPos.x + this->m_deltaPos.x + this->m_deltaPos3.x",
    "CEffectPrim::ProcessAlpha(this)", "CEffectPrim::ShiftSegment(this)", "CEffectPrim::AnimMotion(this) != 0",
]
for step in ref_steps:
    assert step in r, f"Ref missing step: {step}"
for step in exe_steps:
    assert step in e, f"EXE missing step: {step}"
assert [r.index(x) for x in ref_steps] == sorted(r.index(x) for x in ref_steps), "Ref operation order changed"
assert [e.index(x) for x in exe_steps] == sorted(e.index(x) for x in exe_steps), f"EXE operation order changed: {[(x, e.index(x)) for x in exe_steps]}"
assert "Prim3DParticleSpline" in r and "return CEffectPrim::AnimMotion(this) != 0" in e
impl = body(current, "bool UpdateReferenceParticleSpline(CEffectPrim* prim)", "} // namespace")
impl_steps = [
    "prim->m_orgPos = AddVec3(prim->m_master->EffectPosition(), prim->m_deltaPos2)", "prim->m_speed += prim->m_accel",
    "prim->m_latitude += prim->m_latiSpeed", "prim->m_latiSpeed += prim->m_latiAccel",
    "prim->m_gravSpeed += prim->m_gravAccel", "const float speed = prim->m_speed - prim->m_gravSpeed",
    "prim->m_matrix.m[2][0] + prim->m_matrix.m[3][0]", "GetRadian(static_cast<int>(prim->m_roll))",
    "prim->m_rotAxis[0] * sine", "prim->m_deltaPos3 = {", "prim->m_size += prim->m_sizeSpeed",
    "prim->m_deltaPos.x - prim->m_speed3d.x", "prim->m_pos = AddVec3(AddVec3(prim->m_orgPos, prim->m_deltaPos), prim->m_deltaPos3)",
    "ProcessReferenceParticleAlpha(prim)", "ShiftCircleSegments(prim)", "return AnimateReferenceParticle(prim)",
]
for step in impl_steps:
    assert step in impl, f"Implementation missing step: {step}"
assert [impl.index(x) for x in impl_steps] == sorted(impl.index(x) for x in impl_steps), "Implementation operation order differs"
route = current[current.index("if (m_type == PP_3DPARTICLESPLINE)"):]
assert "UpdateReferenceParticleSpline(this); // Ref case 0x08 ignores Prim3DParticleSpline's result." in route
assert "return true;" in route
assert "UpdateFreeParticlePrimitive(this, true, true)" not in route
assert "std::sin(timeSeconds * 4.0f" in current, "Generic updater no longer retains its other existing logic"
ref_dispatch_match = re.search(r"case\s+PP_3DPARTICLESPLINE\s*:(.*?)case\s+PP_3DTEXTURE\s*:", ref, re.S)
assert ref_dispatch_match, "Ref PP_3DPARTICLESPLINE dispatcher case missing"
ref_dispatch = ref_dispatch_match.group(1)
exe_dispatch_start = exe.rindex("unsigned __int8 __thiscall CEffectPrim::OnProcess(CEffectPrim *this)")
exe_dispatch_match = re.search(r"case 8\s*:(.*?)case 9\s*:", exe[exe_dispatch_start:], re.S)
assert exe_dispatch_match, "EXE PP_3DPARTICLESPLINE dispatcher case missing"
exe_dispatch = exe_dispatch_match.group(1)
assert "Prim3DParticleSpline();" in ref_dispatch and "return" not in ref_dispatch
assert "CEffectPrim::Prim3DParticleSpline(this);" in exe_dispatch and "result =" not in exe_dispatch
assert "UpdateReferenceParticleSpline(this); // Ref case 0x08 ignores Prim3DParticleSpline's result." in route
print(f"PASS: Ref, EXE and implementation match across {len(ref_steps)} ordered operations; case 0x08 return is ignored")
