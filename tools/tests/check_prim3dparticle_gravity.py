from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
ref_text = (ROOT / "Ref/dhxj/RagEffectPrim.cpp").read_text(encoding="cp1252")
exe_text = (ROOT / "HighPriest.exe.c").read_text(encoding="cp1252")
src_text = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")

def extract(text, start, end):
    a = text.index(start)
    b = text.index(end, a)
    return text[a:b]

ref = extract(ref_text, "bool\tCEffectPrim :: Prim3DParticleGravity()", "bool\tCEffectPrim :: Prim3DParticleOrbit()")
exe_start = exe_text.index("//----- (00642AB0)")
exe = extract(exe_text[exe_start:], "bool __thiscall CEffectPrim::Prim3DParticleGravity(CEffectPrim *this)", "//----- (00642CE0)")
impl = extract(src_text, "bool UpdateReferenceGravityParticle(CEffectPrim* prim)", "// Ref/RagEffectPrim.cpp:77-224")

ref_steps = [
    "m_orgPos = m_master->m_pos + m_deltaPos2", "m_speed += m_accel", "m_gravSpeed += m_gravAccel",
    "m_speed = (m_speed < 0) ? 0 : m_speed", "m_speed3d.MatrixMult(vector3d(0.0f, 0.0f, m_speed), m_matrix)",
    "m_deltaPos.x += m_speed3d.x", "m_deltaPos.y += m_gravSpeed", "m_deltaPos.z += m_speed3d.z",
    "m_pos = m_orgPos + m_deltaPos", "m_size += m_sizeSpeed",
    "if (m_gravSpeed > 0 && m_pos.y > gameMode->GetWorld()->m_attr->GetHeight(m_pos.x, m_pos.z))",
    "m_gravSpeed /= -1.5f", "if (m_playWaveType == 1)", "random(3) + 1",
    "PlayWave(buf, m_pos.x-pPos.x, 0,  m_pos.z-pPos.z)", "ProcessAlpha()", "ShiftSegment()", "if (!AnimMotion())",
]
exe_steps = [
    "p_m_pos->x + this->m_deltaPos2.x", "this->m_accel + this->m_speed",
    "this->m_gravSpeed = this->m_gravAccel + this->m_gravSpeed", "if ( v7 < 0.0 )",
    "v7 * this->m_matrix.v31 + this->m_matrix.v41", "this->m_speed3d.x + this->m_deltaPos.x",
    "this->m_deltaPos.y + this->m_gravSpeed", "this->m_speed3d.z + this->m_deltaPos.z",
    "this->m_orgPos.x + this->m_deltaPos.x", "this->m_sizeSpeed + this->m_size",
    "C3dAttr::GetHeight(GameMode->m_world->m_attr, this->m_pos.x, this->m_pos.z) < (double)this->m_pos.y",
    "this->m_playWaveType == 1", "this->m_gravSpeed = this->m_gravSpeed * -0.66666669",
    "v12 % 3 + 1", "PlayWave(buf, x, 0.0, z, 250, 40, 1.0)",
    "CEffectPrim::ProcessAlpha(this)", "CEffectPrim::ShiftSegment(this)", "CEffectPrim::AnimMotion(this) != 0",
]
for label, text, steps in (("Ref", ref, ref_steps), ("EXE", exe, exe_steps)):
    for step in steps:
        assert step in text, f"{label} missing branch/operation: {step}"
    positions = [text.index(step) for step in steps]
    assert positions == sorted(positions), f"{label} operation/branch order mismatch: {[(step, text.index(step)) for step in steps]}"

impl_steps = [
    "prim->m_orgPos = AddVec3(prim->m_master->EffectPosition(), prim->m_deltaPos2)",
    "double speed = static_cast<double>(prim->m_accel) + prim->m_speed",
    "prim->m_speed = static_cast<float>(speed);\n    prim->m_gravSpeed += prim->m_gravAccel",
    "if (speed < 0.0) speed = 0.0;\n    prim->m_speed = static_cast<float>(speed);\n    prim->m_speed3d",
    "static_cast<float>(speed * prim->m_matrix.m[2][0] + prim->m_matrix.m[3][0])",
    "prim->m_deltaPos.x += prim->m_speed3d.x", "prim->m_deltaPos.y += prim->m_gravSpeed",
    "prim->m_deltaPos.z += prim->m_speed3d.z", "prim->m_pos = AddVec3(prim->m_orgPos, prim->m_deltaPos)",
    "prim->m_size += prim->m_sizeSpeed",
    "prim->m_gravSpeed > 0.0f\n        && prim->m_pos.y > mode->m_world->m_attr->GetHeight(prim->m_pos.x, prim->m_pos.z)",
    "prim->m_gravSpeed * -0.66666669", "prim->m_playWaveType == 1", "char waveName[50]",
    "rand() % 3 + 1", "soundMode->m_world->m_player->m_pos",
    "PlayWave(waveName, prim->m_pos.x - listener.x, 0.0f, prim->m_pos.z - listener.z, 250, 40, 1.0f)",
    "ProcessReferenceParticleAlpha(prim)", "ShiftCircleSegments(prim)", "return AnimateReferenceParticle(prim)",
]
for step in impl_steps:
    assert step in impl, f"current implementation missing expected behavior: {step}"
positions = [impl.index(step) for step in impl_steps]
assert positions == sorted(positions), f"current implementation operation/branch order mismatch: {[(step, impl.index(step)) for step in impl_steps]}"
assert "mode &&" not in impl and "if (mode->m_world->m_player)" not in impl, "source-different null guards remain"
assert "std::sprintf(waveName" in impl and "if (prim->m_gravSpeed > 0.0f" in impl
ref_route_match = re.search(r"case\s+PP_3DPARTICLEGRAVITY\s*:(.*?)case\s+PP_3DPARTICLEORBIT\s*:", ref_text, re.S)
assert ref_route_match and "if (!Prim3DParticleGravity())" in ref_route_match.group(1)
exe_dispatch_start = exe_text.rindex("unsigned __int8 __thiscall CEffectPrim::OnProcess(CEffectPrim *this)")
exe_route_match = re.search(r"case 6\s*:(.*?)case 7\s*:", exe_text[exe_dispatch_start:], re.S)
assert exe_route_match and "result = CEffectPrim::Prim3DParticleGravity(this);" in exe_route_match.group(1)
assert "if ( !result )" in exe_route_match.group(1)
current_route = extract(src_text, "if (m_type == PP_3DPARTICLEGRAVITY)", "if (m_type == PP_3DPARTICLESPLINE)")
assert "return UpdateReferenceGravityParticle(this);" in current_route
assert "UsesReferenceParticle" not in current_route and "UpdateFreeParticlePrimitive" not in current_route
print(f"PASS: Ref, EXE and implementation match across {len(ref_steps)} ordered gravity operations, collision/sound branches, and false-return dispatch")
