from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]
LOCAL = (ROOT / "src/world/GroundScope.cpp").read_text(encoding="utf-8")
REF = (ROOT / "Ref/Skill.cpp").read_text(encoding="utf-8", errors="replace")


def body_after(text: str, signature: str) -> str:
    start = text.find(signature)
    assert start >= 0, f"missing function signature: {signature}"
    opening = text.find("{", start)
    assert opening >= 0, f"missing function body: {signature}"
    depth = 0
    for index in range(opening, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[opening + 1:index]
    raise AssertionError(f"unterminated function body: {signature}")


def local_scope_mapping(body: str) -> dict[int, int]:
    switch = re.search(r"switch\s*\(\s*skillId\s*\)\s*\{(.*?)\n\s*default\s*:", body, re.S)
    assert switch, "GetScopeSize skill switch not found"
    groups = re.findall(
        r"((?:\s*case\s+\d+\s*:\s*)+)return\s+(\d+)\s*;",
        switch.group(1),
    )
    mapping: dict[int, int] = {}
    for labels, size in groups:
        for skill in re.findall(r"case\s+(\d+)\s*:", labels):
            key = int(skill)
            assert key not in mapping, f"duplicate scope size for skill {key}"
            mapping[key] = int(size)
    return mapping


expected = {
    **{skill: 3 for skill in (11, 25, 47, 69, 80, 86, 88, 110, 129, 130, 137, 140, 381, 483)},
    **{skill: 5 for skill in (17, 21, 70, 91, 92, 484)},
    **{skill: 7 for skill in (79, 478)},
    **{skill: 9 for skill in (83, 89)},
    85: 11,
}
local_scope = body_after(LOCAL, "int CGroundScope::GetScopeSize(int skillId) const")
ref_scope = body_after(REF, "int __thiscall CGroundScope::GetScopeSize(CGroundScope *this, int skill_id)")
actual = local_scope_mapping(local_scope)
assert actual == expected, f"local scope mapping differs: {actual}"
for size in (3, 5, 7):
    assert re.search(rf"\breturn\s+{size}\s*;", ref_scope), f"Ref scope radius {size} not found"
assert re.search(r"\bresult\s*=\s*9\s*;", ref_scope), "Ref scope radius 9 branch not found"
assert re.search(r"\bresult\s*=\s*11\s*;", ref_scope), "Ref scope radius 11 branch not found"
for skill in expected:
    ref_has_case = re.search(rf"\bcase\s+{skill}\s*:", ref_scope)
    ref_has_comparison = re.search(rf"\bskill_id\s*(?:==|!=)\s*{skill}\b", ref_scope)
    assert ref_has_case or ref_has_comparison, f"Ref scope handler missing skill {skill}"

local_ctor = body_after(LOCAL, "CGroundScope::CGroundScope()")
ref_ctor = body_after(REF, "CGroundScope *__thiscall CGroundScope::CGroundScope(CGroundScope *this)")
for local_field, ref_field, value in (
    ("m_lastingTime", "m_lastingTime", r"0"),
    ("m_rot", "m_rot", r"0\.0f?"),
    ("m_scopeSize", "m_scope_size", r"1"),
):
    assert re.search(rf"this->{ref_field}\s*=\s*{value}\s*;", ref_ctor), f"Ref ctor default missing: {ref_field}"
    assert re.search(rf"{local_field}\s*=\s*{value}\s*;", local_ctor), f"local ctor default differs: {local_field}"

local_process = body_after(LOCAL, "u8 CGroundScope::OnProcess()")
ref_process = body_after(REF, "bool __thiscall CGroundScope::OnProcess(CGroundScope *this)")
assert "m_lastingTime >= timeGetTime() ? 1 : 0" in re.sub(r"\s+", " ", local_process)
assert re.search(r"return\s+this->m_lastingTime\s*>=\s*timeGetTime\(\)\s*;", ref_process)

local_send = body_after(LOCAL, "void CGroundScope::SendMsg(")
ref_send = body_after(REF, "void __thiscall CGroundScope::SendMsg(")
for message in (14, 22, 115, 117):
    assert re.search(rf"case\s+{message}\s*:", local_send), f"local message {message} missing"
    assert re.search(rf"case\s+{message}\s*:", ref_send), f"Ref message {message} missing"
assert "m_groundPosX = static_cast<int>(arg1)" in local_send
assert "m_groundPosY = static_cast<int>(arg2)" in local_send
assert "m_masterGid = static_cast<u32>(arg1)" in local_send
assert "m_lastingTime = static_cast<u32>(arg1)" in local_send
assert "GetScopeSize(static_cast<int>(arg1)) + 2" in local_send
assert "0.5 / static_cast<double>(scopeSize)" in local_send
for ref_fragment in (
    "this->m_ground_pos_x = arg1",
    "this->m_ground_pos_y = arg2",
    "this->m_master_gid = arg1",
    "this->m_lastingTime = arg1",
    "CGroundScope::GetScopeSize(this, arg1) + 2",
    "this->m_rot_speed = 0.5 / (double)v7",
):
    assert ref_fragment in ref_send, f"Ref SendMsg mapping missing: {ref_fragment}"

local_render = body_after(LOCAL, "void CGroundScope::Render(matrix* viewMatrix)")
ref_render = body_after(REF, "void __thiscall CGroundScope::Render(CGroundScope *this, matrix *vtm)")
for local_fragment in (
    "viewMatrix == nullptr || m_scopeSize <= 0",
    "gameMode == nullptr || gameMode->m_world == nullptr || gameMode->m_world->m_ground == nullptr",
    'g_texMgr.GetTexture("effect\\\\magic_target.tga", false)',
    "for (int row = 0; row < m_scopeSize; ++row)",
    "for (int column = 0; column < m_scopeSize; ++column)",
    "u0 * cosR + v0 * sinR",
    "-u0 * sinR + v0 * cosR",
    "(coord.x + half) / span",
    "(coord.y + half) / span",
    "originX + column",
    "originY + row",
    "m_rot += m_rotSpeed",
):
    assert local_fragment in local_render, f"local Render behavior missing: {local_fragment}"
for ref_fragment in (
    "half_scope_size = (double)m_scope_size * 0.5",
    "sin_r = v8",
    "cos_r = _cos(v9)",
    "color = -2130706433 - ((unsigned int)(__int64)(sin_r * -40.0) << 24)",
    "Texture = CTexMgr::GetTexture(&g_texMgr, aEffectMagicTar, 0)",
    "C3dGround::RenderGroundTile(ground, vtm, v16, v26, color, Texture, uv)",
    "this->m_rot = this->m_rot_speed + this->m_rot",
):
    assert ref_fragment in ref_render, f"Ref Render behavior missing: {ref_fragment}"
assert "const float originX =" not in local_render
assert "const int originX = m_groundPosX - m_scopeSize / 2" in local_render
assert "const int originY = m_groundPosY - m_scopeSize / 2" in local_render

print(f"CGroundScope source parity: {len(expected)} exact skill radii, ctor, lifetime, messages14/22/115/117, and Render tile/UV behavior")
