from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
ref_text = (ROOT / "Ref/dhxj/RagEffectPrim.cpp").read_text(encoding="cp1252")
exe_text = (ROOT / "HighPriest.exe.c").read_text(encoding="cp1252")
src_text = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")

def function(text, marker):
    start = text.index(marker)
    brace = text.index("{", start)
    depth = 0
    for i in range(brace, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[start:i + 1]
    raise AssertionError(f"unterminated function: {marker}")

def ordered(text, steps, label):
    missing = [step for step in steps if step not in text]
    assert not missing, f"{label} missing branch detail(s): {missing}"
    positions = [text.index(step) for step in steps]
    assert positions == sorted(positions), f"{label} branch detail order differs: {list(zip(steps, positions))}"

ref = function(ref_text, "bool\tCEffectPrim :: Prim3DParticle()")
exe = function(exe_text[exe_text.index("//----- (00624" ):], "bool __thiscall CEffectPrim::Prim3DParticle(CEffectPrim *this)")
impl = function(src_text, "bool UpdateLinearReferenceParticle(CEffectPrim* prim)")

# Ref enum values are mechanically paired with canonical numeric pattern branches.
ref_bits = {
    "PT_DELTASPEED": 0x400, "PT_NOWAIT": 0x001, "PT_SPRINKLE": 0x002,
    "PT_CURVE": 0x004, "PT_BLOW": 0x008, "PT_CHKTARGETPOS": 0x100,
    "PT_ROTATESPR": 0x020,
}
for token, value in ref_bits.items():
    assert re.search(rf"\b{token}\s*=\s*0x0*{value:x}\b", (ROOT / "Ref/dhxj/RagEffect.h").read_text(encoding="cp1252"), re.I), token
    assert token in ref, f"Ref branch missing {token}"
    assert re.search(rf"m_pattern\s*&\s*(?:0x)?0*{value:x}\b", exe, re.I), f"EXE branch missing {token}=0x{value:X}"

ref_steps = [
    "m_orgPos = m_master->m_pos + m_deltaPos2", "if (m_stateCnt == m_ChangePoint)",
    "m_speed += m_accel", "m_sizeSpeed += m_sizeAccel", "if (m_pattern & PT_DELTASPEED)",
    "else if (((m_pattern & PT_NOWAIT) || m_stateCnt > m_fadeInCnt)",
    "if (m_pattern & PT_SPRINKLE)", "if (m_pattern & PT_CURVE)", "if (m_pattern & PT_BLOW)",
    "m_pos = m_orgPos + m_deltaPos", "if (m_pattern & PT_CHKTARGETPOS)",
    "if (m_pattern & PT_ROTATESPR)", "ProcessAlpha()", "ShiftSegment()", "if (!AnimMotion())",
]
exe_steps = [
    "this->m_master->m_pos.x + this->m_deltaPos2.x", "m_stateCnt == this->m_ChangePoint",
    "this->m_accel + this->m_speed", "this->m_sizeAccel + this->m_sizeSpeed", "m_pattern & 0x400",
    "m_stateCnt > this->m_fadeInCnt", "m_pattern & 2", "m_pattern & 4", "m_pattern & 8",
    "this->m_orgPos.x + this->m_deltaPos.x", "m_pattern & 0x100", "m_pattern & 0x20",
    "CEffectPrim::ProcessAlpha(this)", "CEffectPrim::ShiftSegment(this)", "CEffectPrim::AnimMotion(this) != 0",
]
for label, text, steps in (("Ref", ref, ref_steps), ("EXE", exe, exe_steps)):
    for step in steps:
        assert step in text, f"{label} missing branch/order anchor: {step}"
    positions = [text.index(step) for step in steps]
    assert positions == sorted(positions), f"{label} top-level branch order differs"

impl_steps = [
    "prim->m_orgPos = AddVec3(prim->m_master->EffectPosition(), prim->m_deltaPos2)",
    "if (prim->m_stateCnt == prim->m_changePoint)", "const double speed = static_cast<double>(prim->m_accel) + prim->m_speed",
    "prim->m_sizeSpeed += prim->m_sizeAccel", "if ((prim->m_pattern & 0x400) != 0)",
    "(prim->m_pattern & 1) != 0 || prim->m_stateCnt > prim->m_fadeInCnt", "if ((prim->m_pattern & 2) != 0)",
    "if ((prim->m_pattern & 4) != 0", "if ((prim->m_pattern & 8) != 0",
    "prim->m_pos = AddVec3(prim->m_orgPos, prim->m_deltaPos)", "if ((prim->m_pattern & 0x100) != 0)",
    "if ((prim->m_pattern & 0x20) != 0)", "ProcessReferenceParticleAlpha(prim)",
    "ShiftCircleSegments(prim)", "return AnimateReferenceParticle(prim)",
]
for step in impl_steps:
    assert step in impl, f"implementation missing pattern/operation: {step}"
impl_positions = [impl.index(step) for step in impl_steps]
assert impl_positions == sorted(impl_positions), "implementation top-level operation order differs"
sprinkle_steps = ["rand() % 8 + 12", "rand() % 5 + 5", "rand() % 5 + 5",
                 "if (reverseDirection()) prim->m_deltaPos3.x", "if (reverseDirection()) prim->m_deltaPos3.z",
                 "prim->m_deltaPos.x += prim->m_deltaPos3.x"]
assert [impl.index(step) for step in sprinkle_steps] == sorted(impl.index(step) for step in sprinkle_steps), "sprinkle RNG order differs"

# Branch-level value writes and RNG order are paired independently for each source branch.
ordered(ref, [
    "m_speed3d.x = m_ChangeSpeedx", "m_speed3d.y = m_ChangeSpeedy", "m_speed3d.z = m_ChangeSpeedz",
    "m_deltaPosAccel.x = m_ChangeAccelx", "m_deltaPosAccel.y = m_ChangeAccely", "m_deltaPosAccel.z = m_ChangeAccelz",
], "Ref change-point")
ordered(exe, [
    "this->m_speed3d.x = this->m_ChangeSpeedx", "this->m_speed3d.y = m_ChangeSpeedy",
    "this->m_speed3d.z = m_ChangeSpeedz", "this->m_deltaPosAccel.x = m_ChangeAccelx",
    "this->m_deltaPosAccel.y = m_ChangeAccely", "this->m_deltaPosAccel.z = m_ChangeAccelz",
], "EXE change-point")
ordered(impl, [
    "prim->m_speed3d = prim->m_changeSpeed3d", "prim->m_deltaPosAccel = prim->m_changeAccel3d",
], "implementation change-point")

ordered(ref, [
    "m_speed3d.x += m_deltaPosAccel.x", "m_speed3d.y += m_deltaPosAccel.y", "m_speed3d.z += m_deltaPosAccel.z",
    "m_deltaPos += m_speed3d", "m_roll += m_rollSpeed", "m_size += m_sizeSpeed",
], "Ref 0x400 delta-speed")
ordered(impl, [
    "if ((prim->m_pattern & 0x400) != 0)", "prim->m_speed3d = AddVec3(prim->m_speed3d, prim->m_deltaPosAccel)",
    "prim->m_deltaPos = AddVec3(prim->m_deltaPos, prim->m_speed3d)", "prim->m_roll += prim->m_rollSpeed",
    "prim->m_size += prim->m_sizeSpeed",
], "implementation 0x400 delta-speed")
ref_linear = ref[ref.index("else if (((m_pattern & PT_NOWAIT)"):ref.index("if (m_pattern & PT_SPRINKLE)")]
impl_linear = impl[impl.index("else if ((prim->m_pattern & 1)"):impl.index("if ((prim->m_pattern & 2)")]
ordered(ref_linear, [
    "m_speed3d.MatrixMult(vector3d(0.0f, 0.0f, m_speed), m_matrix)", "m_deltaPos += m_speed3d",
    "m_roll += m_rollSpeed", "m_size += m_sizeSpeed",
], "Ref ordinary/fade-in movement")
ordered(impl_linear, [
    "prim->m_speed3d = {", "prim->m_deltaPos = AddVec3(prim->m_deltaPos, prim->m_speed3d)",
    "prim->m_roll += prim->m_rollSpeed", "prim->m_size += prim->m_sizeSpeed",
], "implementation ordinary/fade-in movement")

ordered(ref, ["m_count = random(8) + 12", "random(5) + 5", "random(5) + 5",
              "if (random(2) == 0)", "if (random(2) == 0)", "m_deltaPos.x += m_deltaPos3.x"], "Ref 0x2 sprinkle")
ordered(exe, ["rand() % 8 + 12", "rand() % 5 + 5", "rand() % 5 + 5",
              "v14 = rand() & 0x80000001", "v16 = rand() & 0x80000001",
              "this->m_deltaPos3.x + this->m_deltaPos.x"], "EXE 0x2 sprinkle")
ordered(ref, ["random(35) + 15", "random(80)", "random(80)", "MakeYRotation(GetRadian((int)m_longitude))",
              "AppendXRotation(GetRadian((int)m_latitude))", "random(10) + 5"], "Ref 0x4 curve")
ordered(exe, ["rand() % 35 + 15", "rand() % 80", "rand() % 80", "matrix::MakeYRotation",
              "matrix::AppendXRotation", "rand() % 10 + 5"], "EXE 0x4 curve")
ordered(impl, ["rand() % 35 + 15", "prim->m_longitude =", "prim->m_latitude =",
               "MatrixAppendYRotation", "MatrixAppendXRotation", "rand() % 10 + 5"], "implementation 0x4 curve")
ordered(ref, ["random(10)", "random(10)", "MakeXRotation(GetRadian((int)m_latitude))",
              "AppendYRotation(GetRadian((int)m_longitude))", "random(15) + 5"], "Ref 0x8 blow")
ordered(exe, ["rand() % 10", "rand() % 10", "matrix::MakeXRotation", "matrix::AppendYRotation",
              "rand() % 15 + 5"], "EXE 0x8 blow")
impl_blow = impl[impl.index("if ((prim->m_pattern & 8) != 0"):]
ordered(impl_blow, ["prim->m_longitude = static_cast<float>(prim->m_longitude - 5.0 + rand() % 10)",
               "prim->m_latitude = static_cast<float>(prim->m_latitude - 5.0 + rand() % 10)",
               "MatrixAppendXRotation", "MatrixAppendYRotation", "rand() % 15 + 5"], "implementation 0x8 blow")
assert "CalcDist(m_master->m_deltaPos.x, m_master->m_deltaPos.z) <= 3.0f" in ref
assert "CRenderObject::CalcDist(this, this->m_master->m_deltaPos.x, this->m_master->m_deltaPos.z) <= 3.0" in exe
assert "std::sqrt(static_cast<double>(squared)) <= 3.0" in impl
assert "m_roll = m_roty - gameMode->GetView()->GetCurLongitude()" in ref
assert "this->m_roll = this->m_roty - CModeMgr::GetGameMode(&g_modeMgr)->m_view->m_cur.longitude" in exe
assert "prim->m_roll = prim->m_roty - mode->m_view->GetCameraLongitude()" in impl
ref_route = re.search(r"case\s+PP_3DPARTICLE\s*:(.*?)case\s+PP_3DPARTICLEGRAVITY\s*:", ref_text, re.S)
assert ref_route and "if (!Prim3DParticle())" in ref_route.group(1)
exe_dispatch_start = exe_text.rindex("unsigned __int8 __thiscall CEffectPrim::OnProcess(CEffectPrim *this)")
exe_route = re.search(r"case 5\s*:(.*?)case 6\s*:", exe_text[exe_dispatch_start:], re.S)
assert exe_route and "result = CEffectPrim::Prim3DParticle(this);" in exe_route.group(1) and "if ( !result )" in exe_route.group(1)
current_dispatch = function(src_text, "bool CEffectPrim::ProcessPrimitiveFrame()")
route_start = current_dispatch.index("if (m_type == PP_3DPARTICLE) {")
route_end = current_dispatch.index("if (m_type == PP_3DPARTICLEGRAVITY)", route_start)
route = current_dispatch[route_start:route_end]
assert "return UpdateLinearReferenceParticle(this);" in route
assert "UsesReferenceParticle()" not in route and "UpdateFreeParticlePrimitive" not in route
print(f"PASS: Ref, EXE and implementation match across {len(ref_bits)} pattern IDs, {len(ref_steps)} ordered branches, sprinkle RNG order and false-return dispatch")
