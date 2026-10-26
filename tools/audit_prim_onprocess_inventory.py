#!/usr/bin/env python3
"""Inventory CEffectPrim::OnProcess source cases and current split dispatch routes."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffectPrim.cpp").read_text(encoding="utf-8", errors="replace")
REF_EFFECT2 = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
REF_DHXJ_EFFECT2 = (ROOT / "Ref/dhxj/RagEffect2.cpp").read_text(encoding="cp1252", errors="replace")
REF_DHXJ_EFFECT_HEADER = (ROOT / "Ref/dhxj/RagEffect.h").read_text(encoding="cp1252", errors="replace")
CURRENT = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")


def body(text: str, marker: str) -> str:
    start = text.index(marker)
    opening = text.index("{", start)
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[opening + 1:pos]
    raise AssertionError(f"unclosed function body: {marker}")


def check(label: str, condition: bool) -> None:
    assert condition, f"FAIL {label}"
    print(f"PASS {label}")


exe_marker = "unsigned __int8 __thiscall CEffectPrim::OnProcess(CEffectPrim *this)\n{"
ref_marker = "unsigned __int8 __thiscall CEffectPrim::OnProcess(CEffectPrim *this)\n{"
current_marker = "u8 CEffectPrim::OnProcess()"
exe_body = body(EXE, exe_marker)
ref_body = body(REF, ref_marker)
current_body = body(CURRENT, current_marker)
exe_switch_start = exe_body.index("switch ( this->m_type )", exe_body.index("LABEL_10:"))
source_dispatch = body(exe_body[exe_switch_start:], "switch ( this->m_type )")
source_outer = exe_body[:exe_switch_start]
source_cases = re.findall(r"\bcase\s+(0x[\da-fA-F]+|\d+)\s*:", source_dispatch)
source_ids = {int(value, 0) for value in source_cases}
outer_cases = {int(value, 0) for value in re.findall(r"\bcase\s+(0x[\da-fA-F]+|\d+)\s*:", source_outer)}
source_copy_ids = outer_cases - {0, 0x10}
current_copy_switch = body(current_body, "switch (static_cast<int>(m_type))")
current_copy_ids = {int(value, 0) for value in re.findall(r"\bcase\s+(0x[\da-fA-F]+|\d+)\s*:", current_copy_switch)}

enum_values = {name: int(value, 16) for name, value in re.findall(r"\b(PP_[A-Z0-9_]+)\s*=\s*0x([0-9A-Fa-f]+)", HEADER)}
type_aliases = {name: enum_values[enum_name] for name, enum_name in re.findall(
    r"constexpr int (kPrimType\w+)\s*=\s*(PP_\w+)", HEADER) if enum_name in enum_values}
current_dispatch = body(CURRENT, "bool CEffectPrim::ProcessPrimitiveFrame()")
current_explicit_ids = set()
for name in re.findall(r"\bkPrimType\w+", current_dispatch):
    if name in type_aliases:
        current_explicit_ids.add(type_aliases[name])
for name in re.findall(r"\bPP_[A-Z0-9_]+\b", current_dispatch):
    if name in enum_values:
        current_explicit_ids.add(enum_values[name])
for value in re.findall(r"static_cast<EFFECTPRIMID>\((0x[\da-fA-F]+|\d+)\)", current_dispatch):
    current_explicit_ids.add(int(value, 0))

check("Ref OnProcess body and canonical EXE body are whitespace-normalized identical",
      re.sub(r"\s+", " ", ref_body) == re.sub(r"\s+", " ", exe_body))
check("source has 128 main-dispatch labels plus 17 outer-gate labels, covering 128 unique ids",
      len(source_cases) == 128 and len(outer_cases) == 17 and len(source_ids) == 128)
check("current outer position-copy IDs match the source's 15 pre-gate cases",
      len(source_copy_ids) == 15 and current_copy_ids == source_copy_ids)
check("current wrapper retains null, frame-skip, failure, single-tick and inclusive-duration gates",
      "if (!m_master)" in current_body
      and "m_gi[0].life == 99" in current_body
      and "if (!ProcessPrimitiveFrame())" in current_body
      and current_body.count("++m_stateCnt;") == 1
      and "return m_stateCnt <= m_duration;" in current_body)
check("current split dispatch has an explicit route inventory for review",
      len(current_explicit_ids) == 84 and {0x00, 0x01, 0x02, 0x04, 0x09, 0x0A, 0x0B, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x1A, 0x1B, 0x1C, 0x1D, 0x20, 0x22, 0x23, 0x25, 0x26, 0x27, 0x28, 0x2A, 0x2B, 0x2C, 0x63}.issubset(current_explicit_ids)
      and "bool CEffectPrim::ProcessPrimitiveFrame()" in CURRENT)
check("case 0x04/0x0E inline circle/sphere mutations are routed unconditionally",
      re.search(r"case 4:.*?ProcessAlpha\(this\);", source_dispatch, re.S) is not None
      and re.search(r"case 0xE:.*?ProcessAlpha\(this\);", source_dispatch, re.S) is not None
      and "if (m_type == PP_2DCIRCLE) {\n        UpdateReferenceScreenCircle(this);" in current_dispatch
      and "if (m_type == PP_3DSPHERE) {\n        UpdateReferenceSphere(this);" in current_dispatch)
check("case 0x11 no-op and case 0x14 Y increment match source and current helper branches",
      re.search(r"case 0x11:\s*goto \$L145961;", source_dispatch) is not None
      and re.search(r"case 0x14:\s*this->m_pos.y = this->m_pos.y \+ 5\.0;\s*goto \$L145961;", source_dispatch) is not None
      and re.search(r"if \(m_type == PP_3DSHINELIGHT\)\s*\{\s*return true;\s*\}", current_dispatch) is not None
      and re.search(r"if \(m_type == PP_3DLINE\)\s*\{\s*m_pos.y \+= 5\.0f;\s*return true;\s*\}", current_dispatch) is not None)
source_no_master = body(REF, "bool __thiscall CEffectPrim::Prim3DParticleNoMaster(CEffectPrim *this)\n{")
exe_no_master = body(EXE, "bool __thiscall CEffectPrim::Prim3DParticleNoMaster(CEffectPrim *this)\n{")
current_no_master = body(CURRENT, "bool UpdateReferenceNoMasterParticle(CEffectPrim* prim)")
check("source Prim3DParticleNoMaster Ref and EXE bodies match and case 0x15 routes to its dedicated helper",
      re.sub(r"\s+", " ", source_no_master) == re.sub(r"\s+", " ", exe_no_master)
      and re.search(r"case 0x15:\s*result = CEffectPrim::Prim3DParticleNoMaster\(this\);", source_dispatch) is not None
      and "if (m_type == PP_3DPARTICLE_NOMASTER) {\n        return UpdateReferenceNoMasterParticle(this);" in current_dispatch)
check("case 0x15 branch and operation order maps change-point, 0x400/fade-in, 2/4/8, stored origin, 0x20, alpha, segments and animation",
      all(token in current_no_master for token in (
          "prim->m_stateCnt == prim->m_changePoint", "prim->m_pattern & 0x400",
          "prim->m_pattern & 1", "prim->m_pattern & 2", "prim->m_pattern & 4",
          "prim->m_pattern & 8", "prim->m_pattern & 0x20", "AddVec3(origin, prim->m_deltaPos)",
          "ProcessReferenceParticleAlpha(prim);", "ShiftCircleSegments(prim);",
          "return AnimateReferenceParticle(prim);"))
      and current_no_master.index("prim->m_pattern & 2") < current_no_master.index("prim->m_pattern & 4")
      < current_no_master.index("prim->m_pattern & 8") < current_no_master.index("AddVec3(origin, prim->m_deltaPos)")
      < current_no_master.index("ProcessReferenceParticleAlpha(prim);")
      < current_no_master.index("ShiftCircleSegments(prim);")
      < current_no_master.index("return AnimateReferenceParticle(prim);"))
source_no_master_texture = body(REF, "bool __thiscall CEffectPrim::Prim3DTextureNoMaster(CEffectPrim *this)\n{")
exe_no_master_texture = body(EXE, "bool __thiscall CEffectPrim::Prim3DTextureNoMaster(CEffectPrim *this)\n{")
current_no_master_texture = body(CURRENT, "bool UpdateReferenceNoMasterTexture(CEffectPrim* prim)")
check("source Prim3DTextureNoMaster Ref/EXE bodies match and case 0x16 routes to its dedicated helper",
      re.sub(r"\s+", " ", source_no_master_texture) == re.sub(r"\s+", " ", exe_no_master_texture)
      and re.search(r"case 0x16:\s*result = CEffectPrim::Prim3DTextureNoMaster\(this\);", source_dispatch) is not None
      and "if (m_type == PP_3DTEXTURE_NOMASTER) {\n        return UpdateReferenceNoMasterTexture(this);" in current_dispatch)
check("case 0x16 implementation preserves source branch and call order",
      all(token in current_no_master_texture for token in (
          "prim->m_stateCnt == prim->m_changePoint", "(pattern & 0x400)",
          "(pattern & 0x100)", "(pattern & 0x200)", "(pattern & 0x40)",
          "(pattern & 2)", "(pattern & 0x10)", "(pattern & 4)",
          "prim->m_pattern & 0x20", "prim->m_pattern & 0x800", "ProcessReferenceParticleAlpha(prim);",
          "return motion != prim->m_totalTexture;"))
      and current_no_master_texture.index("(pattern & 0x400)")
      < current_no_master_texture.index("AddVec3(origin, prim->m_deltaPos)")
      < current_no_master_texture.index("(pattern & 0x200)")
      < current_no_master_texture.index("(pattern & 2)")
      < current_no_master_texture.index("(pattern & 0x10)")
      < current_no_master_texture.index("(pattern & 4)")
      < current_no_master_texture.index("prim->m_pattern & 0x20")
      < current_no_master_texture.index("prim->m_pattern & 0x800")
      < current_no_master_texture.index("ProcessReferenceParticleAlpha(prim);")
      < current_no_master_texture.index("return motion != prim->m_totalTexture;"))
def compact(text):
    return re.sub(r"\s+", "", text)

case16_branch_pairs = [
    ("if ( (m_pattern & 0x400) != 0 )", "if ((pattern & 0x400) != 0)"),
    ("if ( (m_pattern & 0x100) == 0 )", "else if ((pattern & 0x100) == 0)"),
    ("this->m_speed3d.x = m_speed * this->m_matrix.v31 + this->m_matrix.v41;",
     "static_cast<float>(speed * prim->m_matrix.m[2][0] + prim->m_matrix.m[3][0])"),
    ("this->m_speed3d.y = m_speed * this->m_matrix.v32 + this->m_matrix.v42;",
     "static_cast<float>(speed * prim->m_matrix.m[2][1] + prim->m_matrix.m[3][1])"),
    ("this->m_speed3d.z = m_speed * this->m_matrix.v33 + this->m_matrix.v43;",
     "static_cast<float>(speed * prim->m_matrix.m[2][2] + prim->m_matrix.m[3][2])"),
    ("if ( CRenderObject::CalcDist(this, this->m_master->m_deltaPos.x, this->m_master->m_deltaPos.z) <= 3.0 )",
     "if (std::sqrt(static_cast<double>(squared)) <= 3.0)"),
    ("if ( v20 > this->m_maxHeightSize )", "if (prim->m_heightSize > prim->m_maxHeightSize)"),
    ("if ( (v21 & 0x200) != 0 )", "if ((pattern & 0x200) != 0)"),
    ("if ( (v21 & 0x40) != 0 )", "if ((pattern & 0x40) != 0)"),
    ("if ( (v26 & 2) != 0 )", "if ((pattern & 2) != 0)"),
    ("if ( (v26 & 0x10) != 0 )", "if ((pattern & 0x10) != 0)"),
    ("if ( (this->m_pattern & 4) != 0 )", "if ((pattern & 4) != 0)"),
    ("C3dAttr::GetHeight(v31->m_world->m_attr, this->m_pos.x, this->m_pos.z) < (double)this->m_pos.y",
     "GetHeight(prim->m_pos.x, prim->m_pos.z) < prim->m_pos.y"),
    ("if ( (v35 & 0x20) != 0 && !(this->m_stateCnt % this->m_any) )",
     "if ((prim->m_pattern & 0x20) != 0 && prim->m_stateCnt % prim->m_any == 0)"),
    ("if ( v36 && ((m_red = this->m_red, m_red == 254) || this->m_blue == 254 || this->m_green == 254) )",
     "if ((prim->m_pattern & 0x800) != 0\n        && (prim->m_red == 254 || prim->m_blue == 254 || prim->m_green == 254))"),
    ("if ( v20 > this->m_maxHeightSize )\n  {\n    this->m_heightAccel = 0.0;\n    this->m_heightSpeed = 0.0;",
     "if (prim->m_heightSize > prim->m_maxHeightSize) {\n        prim->m_heightAccel = 0.0f;\n        prim->m_heightSpeed = 0.0f;"),
    ("if ( v49 <= 255 )\n    {\n      if ( v49 < 0 )\n        this->m_green = 0;\n    }\n    else\n    {\n      this->m_green = 255;",
     "if (prim->m_green > 255) prim->m_green = 255; else if (prim->m_green < 0) prim->m_green = 0;"),
    ("if ( this->m_stateCnt % this->m_animSpeed )\n    return 1;",
     "if (prim->m_stateCnt % prim->m_animSpeed != 0) return true;"),
]
check("case 0x16 source-to-implementation branch predicates and effects are paired",
      all(compact(source_branch) in compact(source_no_master_texture)
          and compact(current_branch) in compact(current_no_master_texture)
          for source_branch, current_branch in case16_branch_pairs))
source_orbit = body(REF, "bool __thiscall CEffectPrim::Prim2DTextureOrbit(CEffectPrim *this)\n{")
exe_orbit = body(EXE, "bool __thiscall CEffectPrim::Prim2DTextureOrbit(CEffectPrim *this)\n{")
current_orbit = body(CURRENT, "bool UpdateReference2DTextureOrbit(CEffectPrim* prim)")
check("source Prim2DTextureOrbit Ref/EXE bodies match and case 0x01 routes to its dedicated helper",
      compact(source_orbit) == compact(exe_orbit)
      and re.search(r"case 1:\s*result = CEffectPrim::Prim2DTextureOrbit\(this\);", source_dispatch) is not None
      and "if (m_type == PP_2DTEXTUREORBIT) {\n        return UpdateReference2DTextureOrbit(this);" in current_dispatch)
orbit_branch_pairs = [
    ("this->m_orgPos.x = m_master->m_tlvertX + this->m_deltaPos2.x;", "prim->m_orgPos.x = anchor.x + prim->m_deltaPos2.x;"),
    ("this->m_orgPos.y = m_master->m_tlvertY + this->m_deltaPos2.y;", "prim->m_orgPos.y = anchor.y + prim->m_deltaPos2.y;"),
    ("v3 = this->m_radiusAccel + this->m_radiusSpeed;", "prim->m_radiusSpeed += prim->m_radiusAccel;"),
    ("v11 = this->m_longAccel + this->m_longSpeed;", "prim->m_longSpeed += prim->m_longAccel;"),
    ("v9 = this->m_widthAccel + this->m_widthSpeed;", "prim->m_widthSpeed += prim->m_widthAccel;"),
    ("v8 = this->m_heightAccel + this->m_heightSpeed;", "prim->m_heightSpeed += prim->m_heightAccel;"),
    ("v10 = this->m_rollAccel + this->m_rollSpeed;", "prim->m_rollSpeed += prim->m_rollAccel;"),
    ("this->m_roll = v10 + this->m_roll;", "prim->m_roll += prim->m_rollSpeed;"),
    ("this->m_pos.x = GetSin((__int64)v4) * this->m_radius + this->m_orgPos.x;",
     "prim->m_pos.x = GetSin(static_cast<int>(prim->m_longitude)) * prim->m_radius + prim->m_orgPos.x;"),
    ("this->m_pos.y = this->m_orgPos.y - GetCos((__int64)this->m_longitude) * this->m_radius;",
     "prim->m_pos.y = prim->m_orgPos.y - GetCos(static_cast<int>(prim->m_longitude)) * prim->m_radius;"),
    ("if ( this->m_stateCnt % this->m_animSpeed )\n    return 1;", "if (prim->m_stateCnt % prim->m_animSpeed != 0) return true;"),
    ("this->m_curMotion = v6 % this->m_totalTexture;", "prim->m_curMotion = motion % prim->m_totalTexture;"),
    ("return v6 != this->m_totalTexture;", "return motion != prim->m_totalTexture;"),
]
check("case 0x01 source-to-implementation anchor, integration, orbit and animation branches are paired",
      all(compact(source_branch) in compact(source_orbit)
          and compact(current_branch) in compact(current_orbit)
          for source_branch, current_branch in orbit_branch_pairs)
      and current_orbit.index("prim->m_radiusSpeed += prim->m_radiusAccel;")
      < current_orbit.index("prim->m_radius += prim->m_radiusSpeed;")
      < current_orbit.index("prim->m_pos.x =")
      < current_orbit.index("ProcessReferenceParticleAlpha(prim);")
      < current_orbit.index("const int motion = ++prim->m_curMotion;"))
source_texture = body(REF, "bool __thiscall CEffectPrim::Prim2DTexture(CEffectPrim *this)\n{")
exe_texture = body(EXE, "bool __thiscall CEffectPrim::Prim2DTexture(CEffectPrim *this)\n{")
current_texture = body(CURRENT, "bool UpdateReferenceScreenTexture(CEffectPrim* prim)")
check("source Prim2DTexture Ref/EXE bodies match and cases 0/2 route unconditionally",
      compact(source_texture) == compact(exe_texture)
      and re.search(r"case 0:\s*case 2:\s*result = CEffectPrim::Prim2DTexture\(this\);", source_dispatch) is not None
      and "if (m_type == PP_2DTEXTURE || m_type == PP_2DQUAD) {\n        return UpdateReferenceScreenTexture(this);" in current_dispatch)
texture_branch_pairs = [
    ("if ( m_ChangeWidthSpeed != 0.000099999997 )\n      this->m_widthSpeed = m_ChangeWidthSpeed;",
     "if (prim->m_changeWidthSpeed != 0.000099999997f) prim->m_widthSpeed = prim->m_changeWidthSpeed;"),
    ("if ( m_ChangeHeightSpeed != 0.000099999997 )\n      this->m_heightSpeed = m_ChangeHeightSpeed;",
     "if (prim->m_changeHeightSpeed != 0.000099999997f) prim->m_heightSpeed = prim->m_changeHeightSpeed;"),
    ("if ( m_ChangeSpeed != 0.000099999997 )\n      this->m_speed = m_ChangeSpeed;",
     "if (prim->m_changeSpeed != 0.000099999997f) prim->m_speed = prim->m_changeSpeed;"),
    ("if ( m_ChangeWidthAccel != 0.000099999997 )\n      this->m_widthAccel = m_ChangeWidthAccel;",
     "if (prim->m_changeWidthAccel != 0.000099999997f) prim->m_widthAccel = prim->m_changeWidthAccel;"),
    ("if ( m_ChangeHeightAccel != 0.000099999997 )\n      this->m_heightAccel = m_ChangeHeightAccel;",
     "if (prim->m_changeHeightAccel != 0.000099999997f) prim->m_heightAccel = prim->m_changeHeightAccel;"),
    ("if ( m_ChangeAccel != 0.000099999997 )\n      this->m_accel = m_ChangeAccel;",
     "if (prim->m_changeAccel != 0.000099999997f) prim->m_accel = prim->m_changeAccel;"),
    ("if ( (m_pattern & 4) != 0 )", "if ((pattern & 4) != 0)"),
    ("if ( (m_pattern & 2) != 0 && this->m_stateCnt >= this->m_count )",
     "if ((pattern & 2) != 0 && prim->m_stateCnt >= prim->m_count)"),
    ("if ( (m_pattern & 0x80u) != 0 )", "if ((pattern & 0x80) != 0)"),
    ("if ( m_alpha > this->m_minAlpha )", "prim->m_alpha <= prim->m_minAlpha"),
    ("else if ( m_alpha < this->m_maxAlpha )", "prim->m_alpha >= prim->m_maxAlpha"),
    ("this->m_speed = this->m_accel + this->m_speed;", "prim->m_speed += prim->m_accel;"),
    ("this->m_deltaPos.x = GetSin((__int64)this->m_longitude) * this->m_speed + this->m_deltaPos.x;",
     "GetSin(static_cast<int>(prim->m_longitude))) * prim->m_speed + prim->m_deltaPos.x"),
    ("this->m_deltaPos.y = v18;", "prim->m_deltaPos.y = static_cast<float>(deltaY);"),
    ("this->m_pos.x = v21;", "prim->m_pos.x += prim->m_deltaPos.x;"),
    ("this->m_pos.y = v18 + this->m_pos.y;", "prim->m_pos.y = static_cast<float>(deltaY+prim->m_pos.y);"),
    ("if ( v19 <= 255 )", "if (prim->m_red > 255) prim->m_red = 0;"),
    ("if ( v19 < 0 )\n      this->m_red = 255;", "if (prim->m_red < 0) prim->m_red = 255;"),
    ("if ( v22 <= 255 )", "if (prim->m_green > 255) prim->m_green = 255;"),
    ("if ( v22 < 0 )\n      this->m_green = 0;", "if (prim->m_green < 0) prim->m_green = 0;"),
    ("if ( m_blue <= 255 )", "if (prim->m_blue > 255) prim->m_blue = 255;"),
    ("if ( m_blue < 0 )\n      this->m_blue = 0;", "if (prim->m_blue < 0) prim->m_blue = 0;"),
    ("if ( (this->m_pattern & 8) != 0 && !(this->m_stateCnt % this->m_count) )",
     "if ((pattern & 8) != 0 && prim->m_stateCnt % prim->m_count == 0)"),
    ("this->m_longitude = this->m_longitude - 8.0 + (double)(rand() % 16);",
     "prim->m_longitude = static_cast<float>(prim->m_longitude - 8.0 + rand() % 16);"),
    ("this->m_count = rand() % 10 + 10;", "prim->m_count = rand() % 10 + 10;"),
    ("if ( this->m_stateCnt % this->m_animSpeed )\n    return 1;", "if (prim->m_stateCnt % prim->m_animSpeed != 0) return true;"),
    ("this->m_curMotion = v25 % this->m_totalTexture;", "prim->m_curMotion %= prim->m_totalTexture;"),
    ("return v25 != this->m_totalTexture;", "return prim->m_curMotion != prim->m_totalTexture;"),
]
check("case 0x02 source-to-implementation predicates, RGB, RNG and animation effects are paired",
      all(compact(source_branch) in compact(source_texture)
          and compact(current_branch) in compact(current_texture)
          for source_branch, current_branch in texture_branch_pairs)
      and current_texture.index("if ((pattern & 4) != 0)")
      < current_texture.index("prim->m_speed += prim->m_accel;")
      < current_texture.index("if ((pattern & 2) != 0")
      < current_texture.index("if ((pattern & 0x80) != 0)")
      < current_texture.index("prim->m_roll =")
      < current_texture.index("prim->m_deltaPos.x =")
      < current_texture.index("prim->m_red += prim->m_redSpeed;")
      < current_texture.index("if ((pattern & 8) != 0")
      < current_texture.index("ProcessReferenceParticleAlpha(prim);")
      < current_texture.index("++prim->m_curMotion;")
      and source_texture.index("if ( (m_pattern & 4) != 0 )")
      < source_texture.index("this->m_speed = this->m_accel + this->m_speed;")
      < source_texture.index("if ( (m_pattern & 2) != 0")
      < source_texture.index("if ( (m_pattern & 0x80u) != 0 )")
      < source_texture.index("this->m_roll = v11 + this->m_roll;")
      < source_texture.index("this->m_deltaPos.x = GetSin")
      < source_texture.index("this->m_red = v16;")
      < source_texture.index("if ( (this->m_pattern & 8) != 0")
      < source_texture.index("CEffectPrim::ProcessAlpha(this);")
      < source_texture.index("if ( this->m_stateCnt % this->m_animSpeed )"))
source_quad = body(REF, "void __thiscall CEffectPrim::Prim3DQuadHorn(CEffectPrim *this)\n{")
exe_quad = body(EXE, "void __thiscall CEffectPrim::Prim3DQuadHorn(CEffectPrim *this)\n{")
current_quad = body(CURRENT, "void UpdateReferenceQuadHorn(CEffectPrim* prim)")
check("source Prim3DQuadHorn Ref/EXE bodies match and case 0x09 routes every type unconditionally",
      compact(source_quad) == compact(exe_quad)
      and re.search(r"case 9:\s*CEffectPrim::Prim3DQuadHorn\(this\);", source_dispatch) is not None
      and "if (m_type == PP_3DQUADHORN) {\n        UpdateReferenceQuadHorn(this);\n        return true;" in current_dispatch)
quad_branch_pairs = [
    ("this->m_master->m_pos.x + this->m_deltaPos2.x", "prim->m_master->EffectPosition(),prim->m_deltaPos2"),
    ("this->m_master->m_pos.y + this->m_deltaPos2.y", "prim->m_orgPos = AddVec3(prim->m_master->EffectPosition(),prim->m_deltaPos2)"),
    ("this->m_master->m_pos.z + this->m_deltaPos2.z", "prim->m_orgPos = AddVec3(prim->m_master->EffectPosition(),prim->m_deltaPos2)"),
    ("if ( this->m_stateCnt == this->m_ChangePoint )", "if (prim->m_stateCnt == prim->m_changePoint)"),
    ("this->m_speed = this->m_ChangeSpeed;", "prim->m_speed = prim->m_changeSpeed;"),
    ("this->m_accel = m_ChangeAccel;", "prim->m_accel = prim->m_changeAccel;"),
    ("this->m_speed3d.x = v4 * this->m_matrix.v31 + this->m_matrix.v41;",
     "static_cast<float>(speed * prim->m_matrix.m[2][0] + prim->m_matrix.m[3][0])"),
    ("this->m_speed3d.y = v4 * this->m_matrix.v32 + this->m_matrix.v42;",
     "static_cast<float>(speed * prim->m_matrix.m[2][1] + prim->m_matrix.m[3][1])"),
    ("this->m_speed3d.z = v4 * this->m_matrix.v33 + this->m_matrix.v43;",
     "static_cast<float>(speed * prim->m_matrix.m[2][2] + prim->m_matrix.m[3][2])"),
    ("if ( (m_pattern & 1) != 0 && this->m_stateCnt == this->m_count )",
     "if ((prim->m_pattern & 1) != 0 && prim->m_stateCnt == prim->m_count)"),
    ("this->m_stateCnt == this->m_duration - 30", "prim->m_stateCnt == prim->m_duration-30"),
    ("this->m_speed = this->m_ChangeSpeedy;", "prim->m_speed = prim->m_changeSpeedY;"),
    ("this->m_accel = m_ChangeAccely;", "prim->m_accel = prim->m_changeAccelY;"),
    ("CEffectPrim::ProcessAlpha(this);", "ProcessReferenceParticleAlpha(prim);"),
]
check("case 0x09 source-to-implementation anchor, matrix, stop, restart and alpha branches are paired",
      all(compact(source_branch) in compact(source_quad)
          and compact(current_branch) in compact(current_quad)
          for source_branch, current_branch in quad_branch_pairs)
      and current_quad.index("prim->m_orgPos =")
      < current_quad.index("if (prim->m_stateCnt == prim->m_changePoint)")
      < current_quad.index("prim->m_speed3d = {")
      < current_quad.index("prim->m_pos = AddVec3(prim->m_orgPos,prim->m_deltaPos);")
      < current_quad.index("if ((prim->m_pattern & 1) != 0")
      < current_quad.index("if ((prim->m_pattern & 2) != 0)")
      < current_quad.index("ProcessReferenceParticleAlpha(prim);")
      and source_quad.index("this->m_pos.y = v10;")
      < source_quad.index("if ( (m_pattern & 1) != 0")
      < source_quad.index("if ( (m_pattern & 2) != 0")
      < source_quad.index("CEffectPrim::ProcessAlpha(this);"))
source_shift = body(REF, "void __thiscall CEffectPrim::ShiftSegment(CEffectPrim *this)\n{")
exe_shift = body(EXE, "void __thiscall CEffectPrim::ShiftSegment(CEffectPrim *this)\n{")
current_shift = body(CURRENT, "void ShiftReferenceSegments(CEffectPrim* prim)")
check("source ShiftSegment Ref/EXE bodies match and the dedicated implementation preserves its history branch",
      compact(source_shift) == compact(exe_shift)
      and "const int count = prim->m_numSegments;" in current_shift
      and "for (int index = count - 1; index > 0; --index)" in current_shift)
shift_branch_pairs = [
    ("i = this->m_numSegment - 1;", "const int count = prim->m_numSegments;"),
    ("*(v2 - 1) = *(v2 - 36);", "destination = prim->m_segments[static_cast<size_t>(index - 1)];"),
    ("v3 = v1->m_alpha - v1->m_alpha / (double)v1->m_numSegment * v7;",
     "static_cast<double>(prim->m_alpha) / count * step"),
    ("this->m_blue", "prim->m_blue"),
    ("this->m_green", "prim->m_green"),
    ("this->m_red", "prim->m_red"),
    ("this->m_size - this->m_size / (double)this->m_numSegment * v7 * 0.5;",
     "static_cast<double>(prim->m_size) / count * step * 0.5"),
    ("v1->m_segment[0].pos.x = v1->m_pos.x;", "current.pos = prim->m_pos;"),
    ("v1->m_segment[0].segPos[0].x = v1->m_segPos[0].x;",
     "prim->m_segmentQuadPositions[0] = prim->m_segPos;"),
    ("v1->m_segment[0].segPos[3].z = v1->m_segPos[3].z;",
     "prim->m_segmentQuadPositions[0] = prim->m_segPos;"),
    ("v1->m_segment[0].size = v1->m_size;", "current.size = prim->m_size;"),
    ("v1->m_segment[0].radius = m_radius;", "current.radius = prim->m_radius;"),
    ("v1->m_segment[0].alpha = m_alpha;", "current.alpha = prim->m_alpha;"),
    ("qmemcpy(&v1->m_segment[0].mat, &v1->m_matrix, sizeof(v1->m_segment[0].mat));",
     "current.transform = prim->m_matrix;"),
    ("v1->m_segment[0].longitude = m_longitude;", "current.longitude = prim->m_longitude;"),
]
check("ShiftSegment source-to-implementation payload, attenuation and zero-count slot-zero branches are paired",
      all(compact(source_branch) in compact(source_shift)
          and compact(current_branch) in compact(current_shift)
          for source_branch, current_branch in shift_branch_pairs)
      and "prim->m_segmentQuadPositions[0] = prim->m_segPos;" in current_shift
      and current_shift.index("destination = prim->m_segments")
      < current_shift.index("destination.alpha =")
      < current_shift.index("destination.argb =")
      < current_shift.index("destination.size =")
      < current_shift.index("EffectSegmentState& current")
      and source_shift.index("v2[32] = v3;")
      < source_shift.index("v2[50] = this->m_size")
      < source_shift.index("v1->m_segment[0].pos.x ="))
source_spiral = body(REF, "void __thiscall CEffectPrim::Prim3DSpiral(CEffectPrim *this)\n{")
exe_spiral = body(EXE, "void __thiscall CEffectPrim::Prim3DSpiral(CEffectPrim *this)\n{")
current_spiral = body(CURRENT, "void UpdateReferenceSpiral(CEffectPrim* prim)")
check("source Prim3DSpiral Ref/EXE bodies match and ids 0x0A/0x0B share the source handler",
      compact(source_spiral) == compact(exe_spiral)
      and re.search(r"case\s+0xA:\s*case\s+0xB:\s*CEffectPrim::Prim3DSpiral\(this\);", source_dispatch)
      and "if (m_type == PP_3DSPLINE || m_type == PP_3DSPIRAL)" in current_dispatch
      and "UpdateReferenceSpiral(this);" in current_dispatch)
spiral_source_effects = [
    "this->m_master->m_pos.x + this->m_deltaPos2.x",
    "this->m_master->m_pos.y + this->m_deltaPos2.y",
    "this->m_master->m_pos.z + this->m_deltaPos2.z",
    "GetSin((__int64)this->m_longitude)", "GetCos((__int64)this->m_longitude)",
    "GetSin((__int64)(this->m_longSpeed + this->m_longitude))",
    "GetCos((__int64)(this->m_longSpeed + this->m_longitude))",
    "this->m_segPos[0].x = vec1", "this->m_segPos[1].x = vec2",
    "this->m_segPos[2].y = v8", "this->m_segPos[3].y = v9",
    "CEffectPrim::ProcessAlpha(this);", "CEffectPrim::ShiftSegment(this);",
    "this->m_longitude = this->m_longSpeed + this->m_longitude;",
    "this->m_deltaPos2.y = this->m_deltaPos2.y + this->m_heightSpeed;",
]
spiral_current_effects = [
    "AddVec3(prim->m_master->EffectPosition(), prim->m_deltaPos2)",
    "GetSin(static_cast<int>(longitude))", "GetCos(static_cast<int>(longitude))",
    "GetSin(static_cast<int>(nextLongitude))", "GetCos(static_cast<int>(nextLongitude))",
    "prim->m_segPos[0] = {x0, y0, z0};", "prim->m_segPos[1] = {x1, y1, z1};",
    "prim->m_segPos[2] = {x0, y2, z0};", "prim->m_segPos[3] = {x1, y3, z1};",
    "ProcessReferenceParticleAlpha(prim);", "ShiftReferenceSegments(prim);",
    "prim->m_longitude = prim->m_longSpeed + prim->m_longitude;",
    "prim->m_deltaPos2.y = prim->m_deltaPos2.y + prim->m_heightSpeed;",
]
check("case 0x0A/0x0B source-to-implementation origin, orbit, four vertices and post-shift updates are paired",
      all(compact(effect) in compact(source_spiral) for effect in spiral_source_effects)
      and all(compact(effect) in compact(current_spiral) for effect in spiral_current_effects)
      and current_spiral.index("prim->m_orgPos =") < current_spiral.index("const float x0 =")
      < current_spiral.index("const float x1 =") < current_spiral.index("prim->m_segPos[0]")
      < current_spiral.index("prim->m_segPos[1]") < current_spiral.index("prim->m_segPos[2]")
      < current_spiral.index("prim->m_segPos[3]")
      < current_spiral.index("ProcessReferenceParticleAlpha(prim);")
      < current_spiral.index("ShiftReferenceSegments(prim);")
      < current_spiral.index("prim->m_longitude =")
      < current_spiral.index("prim->m_deltaPos2.y ="))
source_texture3d = body(REF, "bool __thiscall CEffectPrim::Prim3DTexture(CEffectPrim *this)\n{")
exe_texture3d = body(EXE, "bool __thiscall CEffectPrim::Prim3DTexture(CEffectPrim *this)\n{")
current_texture3d = body(CURRENT, "bool UpdateReference3DTexture(CEffectPrim* prim)")
check("source Prim3DTexture Ref/EXE bodies match and ids 0x0F/0x10 share an unconditional current route",
      compact(source_texture3d) == compact(exe_texture3d)
      and re.search(r"case\s+0xF:\s*case\s+0x10:\s*CEffectPrim::Prim3DTexture\(this\);", source_dispatch)
      and "if (m_type == PP_3DTEXTURE || m_type == PP_3DCROSSTEXTURE)" in current_dispatch
      and "UpdateReference3DTexture(this); // Source case 0x0F/0x10 ignores the bool result." in current_dispatch)
texture3d_source_branches = [
    "if ( this->m_ChangePoint == this->m_stateCnt )", "this->m_isChangeProcessed = 1;",
    "if ( m_ChangeWidthSpeed != 0.000099999997 )", "if ( m_ChangeHeightSpeed != 0.000099999997 )",
    "if ( m_ChangeSpeed != 0.000099999997 )", "if ( m_ChangeWidthAccel != 0.000099999997 )",
    "if ( m_ChangeHeightAccel != 0.000099999997 )", "if ( m_ChangeAccel != 0.000099999997 )",
    "if ( m_ChangeSpeedx != 0.000099999997 )", "if ( m_ChangeSpeedy != 0.000099999997 )",
    "if ( m_ChangeSpeedz != 0.000099999997 )", "this->m_ChangeAccelx = 0.000099999997;",
    "this->m_ChangeAccely = 0.000099999997;", "this->m_ChangeAccelz = 0.000099999997;",
    "if ( (m_pattern & 0x400) != 0 )", "if ( (m_pattern & 0x100) == 0 )",
    "CRenderObject::CalcDist(this, this->m_master->m_deltaPos.x, this->m_master->m_deltaPos.z) <= 3.0",
    "this->m_heightSize = v19;", "this->m_longitude = this->m_longitude + this->m_longSpeed;",
    "if ( (v20 & 0x200) != 0 )", "if ( (v20 & 0x40) != 0 )",
    "if ( (v25 & 2) != 0 )", "if ( (v25 & 0x10) != 0 )",
    "if ( (this->m_pattern & 4) != 0 )", "if ( (v34 & 0x20) != 0",
    "if ( v35 &&", "if ( v45 <= 255 )", "CEffectPrim::ProcessAlpha(this);",
    "return v51 != this->m_totalTexture;",
]
texture3d_current_branches = [
    "if (prim->m_changePoint == prim->m_stateCnt)", "prim->m_isChangeProcessed = true;",
    "if (prim->m_changeWidthSpeed != sentinel)", "if (prim->m_changeHeightSpeed != sentinel)",
    "if (prim->m_changeSpeed != sentinel)", "if (prim->m_changeWidthAccel != sentinel)",
    "if (prim->m_changeHeightAccel != sentinel)", "if (prim->m_changeAccel != sentinel)",
    "if (prim->m_changeSpeed3d.x != sentinel)", "if (prim->m_changeSpeed3d.y != sentinel)",
    "if (prim->m_changeSpeed3d.z != sentinel)", "prim->m_changeAccel3d = {sentinel, sentinel, sentinel};",
    "if ((pattern & 0x400) != 0)", "else if ((pattern & 0x100) == 0)",
    "if (std::sqrt(static_cast<double>(squared)) <= 3.0)",
    "prim->m_heightSize += prim->m_heightSpeed;", "prim->m_longitude += prim->m_longSpeed;",
    "if ((pattern & 0x200) != 0)", "if ((pattern & 0x40) != 0)",
    "if ((pattern & 2) != 0)", "if ((pattern & 0x10) != 0)",
    "if ((pattern & 4) != 0)", "if ((pattern & 0x20) != 0", "if ((pattern & 0x800) != 0",
    "if (prim->m_red > 255)", "ProcessReferenceParticleAlpha(prim);",
    "return motion != prim->m_totalTexture;",
]
check("case 0x0F/0x10 change-point, motion, collision, orientation, master, ground, RGB and animation branches are paired",
      all(compact(branch) in compact(source_texture3d) for branch in texture3d_source_branches)
      and all(compact(branch) in compact(current_texture3d) for branch in texture3d_current_branches)
      and current_texture3d.index("prim->m_orgPos =") < current_texture3d.index("if (prim->m_changePoint")
      < current_texture3d.index("prim->m_speed3d = AddVec3")
      < current_texture3d.index("prim->m_widthSpeed +=")
      < current_texture3d.index("if ((pattern & 0x400)")
      < current_texture3d.index("prim->m_deltaPos = AddVec3")
      < current_texture3d.index("if ((pattern & 0x400)", current_texture3d.index("prim->m_deltaPos = AddVec3"))
      < current_texture3d.index("prim->m_pos = AddVec3")
      < current_texture3d.index("if ((pattern & 0x200)")
      < current_texture3d.index("if ((pattern & 2)")
      < current_texture3d.index("if ((pattern & 0x10)")
      < current_texture3d.index("if ((pattern & 4)")
      < current_texture3d.index("if ((pattern & 0x20)")
      < current_texture3d.index("if ((pattern & 0x800)")
      < current_texture3d.index("ProcessReferenceParticleAlpha(prim);")
      < current_texture3d.index("if (prim->m_stateCnt % prim->m_animSpeed"))
source_cylinder_half = body(REF, "bool __thiscall CEffectPrim::Prim3DCylinderHalf(CEffectPrim *this)\n{")
exe_cylinder_half = body(EXE, "bool __thiscall CEffectPrim::Prim3DCylinderHalf(CEffectPrim *this)\n{")
current_cylinder = body(CURRENT, "bool UpdateReferenceCylinderState(CEffectPrim* prim)")
check("source CylinderHalf Ref duplicates and canonical EXE body match; case 0x12 ignores the helper result",
      compact(source_cylinder_half) == compact(exe_cylinder_half)
      and re.search(r"case\s+0x12:\s*CEffectPrim::Prim3DCylinderHalf\(this\);\s*goto\s+\$L145961;", source_dispatch)
      and "if (m_type == PP_3DCYLINDERHALF)" in current_dispatch
      and "UpdateReferenceCylinderState(this); // Source case 0x12 ignores the bool result." in current_dispatch
      and re.search(r"if \(m_type == PP_3DCYLINDERHALF\)\s*\{\s*UpdateReferenceCylinderState\(this\);[^}]*return true;", current_dispatch))
cylinder_source_effects = [
    "this->m_master->m_pos.x + this->m_deltaPos2.x", "this->m_master->m_pos.y + this->m_deltaPos2.y",
    "this->m_master->m_pos.z + this->m_deltaPos2.z", "this->m_speed = this->m_accel + this->m_speed;",
    "this->m_outerSpeed = this->m_outerAccel + this->m_outerSpeed;",
    "this->m_innerSpeed = this->m_innerAccel + this->m_innerSpeed;",
    "this->m_heightSpeed = this->m_heightAccel + this->m_heightSpeed;",
    "matrix::MakeXRotation(p_m_matrix, angle);", "matrix::AppendYRotation(p_m_matrix, anglea);",
    "this->m_speed3d.x = v5 * p_m_matrix->v21 + p_m_matrix->v41;",
    "this->m_deltaPos.x = this->m_speed3d.x + this->m_deltaPos.x;",
    "this->m_longitude = this->m_longSpeed + this->m_longitude;",
    "this->m_outerSize = v18;", "this->m_innerSize = v6;", "this->m_heightSize = v7;",
    "if ( v7 > this->m_maxHeightSize )", "if ( (this->m_pattern & 1) != 0 )",
    "CEffectPrim::ProcessAlpha(this);", "return v10 != this->m_totalTexture;",
]
cylinder_current_effects = [
    "AddVec3(prim->m_master->EffectPosition(), prim->m_deltaPos2)",
    "prim->m_speed += prim->m_accel;", "prim->m_outerSpeed += prim->m_outerAccel;",
    "prim->m_innerSpeed += prim->m_innerAccel;", "prim->m_heightSpeed += prim->m_heightAccel;",
    "MatrixAppendXRotation(prim->m_matrix", "MatrixAppendYRotation(prim->m_matrix",
    "move * prim->m_matrix.m[1][0] + prim->m_matrix.m[3][0]",
    "prim->m_deltaPos = AddVec3(prim->m_deltaPos, prim->m_speed3d);",
    "prim->m_longitude += prim->m_longSpeed;", "prim->m_outerSize += prim->m_outerSpeed;",
    "prim->m_innerSize += prim->m_innerSpeed;", "prim->m_heightSize += prim->m_heightSpeed;",
    "if (prim->m_heightSize > prim->m_maxHeightSize)",
    "if ((prim->m_pattern & 1) != 0 && prim->m_stateCnt == prim->m_fadeOutCnt)",
    "ProcessReferenceParticleAlpha(prim);", "return prim->m_curMotion != prim->m_totalTexture;",
]
check("case 0x12 anchor, speed, matrix, dimensions, fade branch and animation return are paired",
      all(compact(effect) in compact(source_cylinder_half) for effect in cylinder_source_effects)
      and all(compact(effect) in compact(current_cylinder) for effect in cylinder_current_effects)
      and current_cylinder.index("prim->m_orgPos =") < current_cylinder.index("prim->m_speed +=")
      < current_cylinder.index("MatrixIdentity") < current_cylinder.index("prim->m_speed3d =")
      < current_cylinder.index("prim->m_deltaPos =") < current_cylinder.index("prim->m_pos =")
      < current_cylinder.index("prim->m_longitude +=") < current_cylinder.index("prim->m_outerSize +=")
      < current_cylinder.index("ProcessReferenceParticleAlpha(prim);")
      < current_cylinder.index("if (prim->m_stateCnt % prim->m_animSpeed"))
source_flash3d = body(REF, "void __thiscall CEffectPrim::Prim3DFlash(CEffectPrim *this)\n{")
exe_flash3d = body(EXE, "void __thiscall CEffectPrim::Prim3DFlash(CEffectPrim *this)\n{")
current_flash3d = body(CURRENT, "void UpdateReferenceFlash(CEffectPrim* prim)")
check("source Prim3DFlash Ref/EXE bodies match and case 0x13 reaches an unconditional current route",
      compact(source_flash3d) == compact(exe_flash3d)
      and re.search(r"case\s+0x13:\s*CEffectPrim::Prim3DFlash\(this\);\s*goto\s+\$L145961;", source_dispatch)
      and "if (m_type == PP_3DFLASH)" in current_dispatch
      and "UpdateReferenceFlash(this);" in current_dispatch)
flash_source_effects = [
    "this->m_pos.y = m_master->m_tlvertY + this->m_deltaPos2.y;",
    "this->m_pos.x = m_master->m_tlvertX + this->m_deltaPos2.x;",
    "if ( !v3 && this->m_heightSize <= 0.0 )", "this->m_heightSpeed = 0.0;",
    "this->m_heightAccel = 0.0;", 'Trace("m_heightSize  = %d", LODWORD(m_heightSize));',
    "if ( this->m_stateCnt == this->m_ChangePoint )",
    "this->m_heightSpeed = this->m_ChangeHeightSpeed;",
    "this->m_longSpeed = m_ChangeSpeed;", "this->m_longAccel = m_ChangeAccel;",
    "if ( this->m_heightSize < 0.0 )", "this->m_heightSize = 0.0;",
    "this->m_heightSpeed = v7;", "this->m_arcAngle = this->m_widthSpeed + this->m_arcAngle;",
    "this->m_longitude = v8 + this->m_longitude;",
    "this->m_heightSize = v9 + this->m_heightSize;", "CEffectPrim::ProcessAlpha(this);",
]
flash_current_effects = [
    "prim->m_pos.y = anchor.y + prim->m_deltaPos2.y;",
    "prim->m_pos.x = anchor.x + prim->m_deltaPos2.x;",
    "(prim->m_pattern & 1) != 0 && prim->m_heightSize <= 0.0f",
    "prim->m_heightSpeed = 0.0f;", "prim->m_heightAccel = 0.0f;",
    "Trace(\"m_heightSize  = %d\", static_cast<int>(traceValue));",
    "prim->m_stateCnt == prim->m_changePoint", "prim->m_heightSpeed = prim->m_changeHeightSpeed;",
    "prim->m_longSpeed = prim->m_changeSpeed;", "prim->m_longAccel = prim->m_changeAccel;",
    "prim->m_heightSize = 0.0f;", "prim->m_heightSpeed *= -1.0f;",
    "prim->m_arcAngle += prim->m_widthSpeed;", "prim->m_longitude = static_cast<float>(angularSpeed + prim->m_longitude);",
    "prim->m_heightSize += prim->m_heightSpeed;", "ProcessReferenceParticleAlpha(prim);",
]
check("case 0x13 Y/X anchor, pattern stop and trace, change point, reflection and alpha branches are paired in order",
      all(compact(effect) in compact(source_flash3d) for effect in flash_source_effects)
      and all(compact(effect) in compact(current_flash3d) for effect in flash_current_effects)
      and current_flash3d.index("prim->m_pos.y =") < current_flash3d.index("prim->m_pos.x =")
      < current_flash3d.index("if ((prim->m_pattern & 1)")
      < current_flash3d.index("Trace(")
      < current_flash3d.index("if (prim->m_stateCnt == prim->m_changePoint)")
      < current_flash3d.index("if (prim->m_heightSize < 0.0f)")
      < current_flash3d.index("prim->m_longSpeed = static_cast<float>(angularSpeed)")
      < current_flash3d.index("prim->m_heightSpeed +=")
      < current_flash3d.index("prim->m_arcAngle +=")
      < current_flash3d.index("prim->m_longitude =")
      < current_flash3d.index("prim->m_heightSize +=")
      < current_flash3d.index("ProcessReferenceParticleAlpha(prim);"))
source_map_magic = body(EXE, "bool __thiscall CEffectPrim::PrimMapMagicZone(CEffectPrim *this)\n{")
current_map_magic = body(CURRENT, "bool UpdateReferenceMapMagicZone(CEffectPrim* prim)")
check("case 0x1A Ref dispatch and canonical EXE helper map to a dedicated current route",
      re.search(r"case\s+0x1A:\s*CEffectPrim::PrimMapMagicZone\(this\);\s*goto\s+\$L145961;", source_dispatch)
      and "if (m_type == PP_MAPMAGICZONE)" in current_dispatch
      and "UpdateReferenceMapMagicZone(this); // Source case 0x1A ignores the bool result." in current_dispatch
      and "return true;" in current_dispatch[current_dispatch.index("if (m_type == PP_MAPMAGICZONE)"):])
map_magic_source_effects = [
    "this->m_pos = this->m_master->m_pos;",
    "LODWORD(this->m_GI[0].height[4]) == 1096810496",
    "v1 = --this->m_GI[0].RotStart;", "if ( v1 < 0 )",
    "this->m_GI[0].RotStart = v1 + 360;",
    "return CEffectPrim::AnimTexture(this) != 0;",
    "v3 = ++this->m_GI[0].RotStart;", "if ( v3 >= 360 )",
    "this->m_GI[0].RotStart = v3 - 360;",
]
map_magic_current_effects = [
    "prim->m_pos = prim->m_master->EffectPosition();",
    "std::memcpy(&heightBits, &prim->m_gi[0].height[4], sizeof(heightBits));",
    "if (heightBits == 1096810496u)", "prim->m_gi[0].rotStart - 1",
    "if (rotStart < 0)", "rotStart + 360",
    "prim->m_gi[0].rotStart + 1", "if (rotStart >= 360)", "rotStart - 360",
    "if (prim->m_stateCnt % prim->m_animSpeed != 0)", "++prim->m_curMotion;",
    "prim->m_curMotion %= prim->m_totalTexture;", "return motion != prim->m_totalTexture;",
]
check("case 0x1A bit-pattern direction, signed wrap, master position and animation branches are paired",
      all(compact(effect) in compact(source_map_magic) for effect in map_magic_source_effects)
      and all(compact(effect) in compact(current_map_magic) for effect in map_magic_current_effects)
      and current_map_magic.index("prim->m_pos =") < current_map_magic.index("std::memcpy")
      < current_map_magic.index("if (heightBits ==")
      < current_map_magic.index("prim->m_gi[0].rotStart - 1")
      < current_map_magic.index("if (rotStart < 0)")
      < current_map_magic.index("prim->m_gi[0].rotStart + 1")
      < current_map_magic.index("if (prim->m_stateCnt % prim->m_animSpeed")
      < current_map_magic.index("++prim->m_curMotion;"))
source_map_particle_decompiled = body(REF_EFFECT2, "unsigned __int8 __thiscall CEffectPrim::PrimMapParticle(CEffectPrim *this)\n{")
exe_map_particle = body(EXE, "unsigned __int8 __thiscall CEffectPrim::PrimMapParticle(CEffectPrim *this)\n{")
source_map_particle = body(REF_DHXJ_EFFECT2, "CEffectPrim :: PrimMapParticle()")
current_map_particle = body(CURRENT, "bool UpdateReferenceMapParticle(CEffectPrim* prim)")
check("case 0x1B Ref and canonical EXE helper bodies match and dispatch ignores completion",
      compact(source_map_particle_decompiled) == compact(exe_map_particle)
      and re.search(r"case\s+0x1B:\s*CEffectPrim::PrimMapParticle\(this\);\s*goto\s+\$L145961;", source_dispatch)
      and "if (m_type == PP_MAPPARTICLE)" in current_dispatch
      and "UpdateReferenceMapParticle(this); // Source case 0x1B ignores the bool result." in current_dispatch
      and len(current_explicit_ids) == 84)
map_particle_effects = [
    "m_GI[0].alphaT=pView->GetCurLongitude();", "m_GI[0].alphaT=360-m_GI[0].alphaT;",
    "m_GI[1].alphaT=360-m_GI[1].alphaT+90;", "if (m_GI[1].alphaT>=360)",
    "m_pos = m_master->m_pos;", "if (m_GI[ec].life == 1)",
    "if (m_GI[ec].height[i+1] > m_GI[ec].height[i])", "m_GI[ec].height[i]+=1;",
    "if (m_GI[ec].height[i] > m_GI[ec].height[i+1])",
    "m_GI[ec].height[i+1] = random(int(m_GI[ec].height[i]));",
    "m_GI[ec].height[i]-=1;", "if (m_GI[ec].height[i] <= m_GI[ec].height[i+1])",
    "m_GI[ec].height[i+1] = 359 - random(int(m_GI[ec].height[i]));",
    "for (i=4;i<8;i++)", "g_sinTable[int(m_GI[ec].height[4])] * 20.0f",
    "g_cosTable[int(m_GI[ec].height[7])] * 20.0f", "m_GI[ec].vecB_pre.y < 0",
    "m_GI[ec].vecB_now.y < 0", "m_GI[ec].vecT_pre.y < 0", "m_GI[ec].vecT_now.y < 0",
    "return true;",
]
map_particle_current_effects = [
    "const short longitude = static_cast<short>(view->GetCameraLongitude());",
    "static_cast<short>(360 - longitude)", "static_cast<short>(360 - latitude + 90)",
    "if (cameraLatitude >= 360)", "prim->m_pos = prim->m_master->EffectPosition();",
    "if (gi.life != 1) continue;", "if (target > current)", "current += 1.0f;",
    "if (current > target)", "rand() % range", "current -= 1.0f;",
    "if (current <= target)", "359 - (range == 0 ? 0 : rand() % range)",
    "for (int angle = 4; angle < 8; ++angle)", "GetSin(static_cast<int>(gi.height[4])) * 20.0f",
    "GetCos(static_cast<int>(gi.height[7])) * 20.0f", "if (gi.arcBaselineY < 0.0f)",
    "if (gi.sparkY < 0.0f)", "if (gi.vecTPreY < 0.0f)", "if (gi.vecTNowY < 0.0f",
    "return true;",
]
check("case 0x1B branch predicates, mutations and all four orbit correction branches map source-to-implementation",
      all(compact(effect) in compact(source_map_particle) for effect in map_particle_effects)
      and all(compact(effect) in compact(current_map_particle) for effect in map_particle_current_effects)
      and current_map_particle.index("const short longitude") < current_map_particle.index("prim->m_pos =")
      < current_map_particle.index("if (gi.life != 1)")
      < current_map_particle.index("if (target > current)")
      < current_map_particle.index("for (int angle = 4")
      < current_map_particle.index("gi.vecBPreX =")
      < current_map_particle.index("if (gi.arcBaselineY <")
      < current_map_particle.index("if (gi.sparkY <")
      < current_map_particle.index("if (gi.vecTPreY <")
      < current_map_particle.index("if (gi.vecTNowY <"))
source_aura_decompiled = body(REF_EFFECT2, "bool __thiscall CEffectPrim::Prim3DAura(CEffectPrim *this)\n{")
exe_aura = body(EXE, "bool __thiscall CEffectPrim::Prim3DAura(CEffectPrim *this)\n{")
source_aura = body(REF_DHXJ_EFFECT2, "CEffectPrim :: Prim3DAura()")
current_aura = body(CURRENT, "bool UpdateReference3DAura(CEffectPrim* prim)")
source_anim_texture = body(REF, "unsigned __int8 __thiscall CEffectPrim::AnimTexture(CEffectPrim *this)\n{")
exe_anim_texture = body(EXE, "unsigned __int8 __thiscall CEffectPrim::AnimTexture(CEffectPrim *this)\n{")
check("case 0x1C Ref and canonical EXE aura bodies match and dispatch discards animation completion",
      compact(source_aura_decompiled) == compact(exe_aura)
      and re.search(r"case\s+0x1C:\s*CEffectPrim::Prim3DAura\(this\);\s*goto\s+\$L145961;", source_dispatch)
      and "if (m_type == PP_3DAURA)" in current_dispatch
      and "UpdateReference3DAura(this); // Source case 0x1C ignores the bool result." in current_dispatch
      and len(current_explicit_ids) == 84)
aura_source_effects = [
    "m_pos = m_master->m_pos;", "for (ec=0;ec<2;ec++)", "m_GI[ec].rise_angle+=3;",
    "if (m_GI[ec].rise_angle>=360)", "m_GI[ec].rise_angle-=360;",
    "if (!AnimTexture())", "return false;", "return true;",
]
aura_current_effects = [
    "prim->m_pos = prim->m_master->EffectPosition();", "for (int index = 0; index < 2; ++index)",
    "prim->m_gi[index].riseAngle + 3", "if (riseAngle >= 360)",
    "riseAngle - 360", "if (prim->m_stateCnt % prim->m_animSpeed == 0)",
    "const int motion = ++prim->m_curMotion;", "if (prim->m_repeatAnim)",
    "motion % prim->m_totalTexture", "else if (motion == prim->m_totalTexture)",
    "return false;", "return true;",
]
check("case 0x1C source-to-implementation maps both unconditional angle updates and AnimTexture branches in order",
      compact(source_anim_texture) == compact(exe_anim_texture)
      and all(compact(effect) in compact(source_aura) for effect in aura_source_effects)
      and all(compact(effect) in compact(current_aura) for effect in aura_current_effects)
      and current_aura.index("prim->m_pos =") < current_aura.index("for (int index")
      < current_aura.index("const short riseAngle")
      < current_aura.index("if (prim->m_stateCnt %")
      < current_aura.index("const int motion =")
      < current_aura.index("if (prim->m_repeatAnim)")
      < current_aura.index("else if (motion =="))
source_aura2_decompiled = body(REF_EFFECT2, "unsigned __int8 __thiscall CEffectPrim::Prim3DAura_2(CEffectPrim *this)\n{")
exe_aura2 = body(EXE, "unsigned __int8 __thiscall CEffectPrim::Prim3DAura_2(CEffectPrim *this)\n{")
source_aura2 = body(REF_DHXJ_EFFECT2, "CEffectPrim :: Prim3DAura_2()")
current_aura2 = body(CURRENT, "bool UpdateReference3DAura2(CEffectPrim* prim)")
check("case 0x1D Ref/EXE helper bodies match and the dispatcher ignores its return",
      compact(source_aura2_decompiled) == compact(exe_aura2)
      and re.search(r"case\s+0x1D:\s*CEffectPrim::Prim3DAura_2\(this\);\s*goto\s+\$L145961;", source_dispatch)
      and "if (m_type == PP_3DAURA_2)" in current_dispatch
      and "UpdateReference3DAura2(this); // Source case 0x1D ignores the bool result." in current_dispatch
      and len(current_explicit_ids) == 84)
aura2_source_effects = [
    "m_GI[0].alphaT=pView->GetCurLongitude();", "m_GI[1].alphaT=360-m_GI[1].alphaT+90;",
    "if (m_GI[1].alphaT>=360)", "m_pos = m_master->m_pos;", "for (ec=0;ec<4;ec++)",
    "if (m_GI[ec].life == 1)", "for (i=0;i<16;i+=2)",
    "if (m_GI[ec].height[i+1] > m_GI[ec].height[i])", "m_GI[ec].height[i]+=3;",
    "if (m_GI[ec].height[i] > 359)", "m_GI[ec].height[i] = 359;",
    "if (m_GI[ec].height[i] > m_GI[ec].height[i+1])",
    "random(int(m_GI[ec].height[i]))", "m_GI[ec].height[i]-=2;",
    "if (m_GI[ec].height[i] < 0)", "m_GI[ec].height[i] = 0;",
    "if (m_GI[ec].height[i] <= m_GI[ec].height[i+1])",
    "359 - random(int(m_GI[ec].height[i]))",
    "if (m_GI[ec].vecB_pre.y < 0)", "m_GI[ec].flag1[0] == 11",
    "m_GI[ec].vecB_pre.y<-50", "m_GI[ec].vecB_pre.y<-30",
    "m_GI[ec].vecB_pre.y -= m_GI[ec].max_height;",
    "if (m_GI[ec].vecB_now.y < 0)", "m_GI[ec].vecB_now.y<-30",
    "m_GI[ec].vecB_now.y -= m_GI[ec].max_height;",
    "if (m_GI[ec].vecT_pre.y < 0)", "m_GI[ec].vecT_pre.y -= m_GI[ec].max_height;",
    "if (m_GI[ec].vecT_now.y < 0)", "m_GI[ec].vecT_now.y -= m_GI[ec].max_height;",
    "return true;",
]
aura2_current_effects = [
    "static_cast<short>(360 - longitude)", "static_cast<short>(360 - latitude + 90)",
    "if (cameraLatitude >= 360)", "prim->m_pos = prim->m_master->EffectPosition();",
    "if (gi.life != 1) continue;", "for (int angle = 0; angle < 16; angle += 2)",
    "if (target > current)", "current += 3.0f;", "if (current > 359.0f) current = 359.0f;",
    "if (current > target)", "rand() % range", "current -= 2.0f;",
    "if (current < 0.0f) current = 0.0f;", "if (current <= target)",
    "359 - (range == 0 ? 0 : rand() % range)", "if (gi.arcBaselineY < 0.0f)",
    "if (gi.flag[0] == 11)", "if (gi.arcBaselineY < -50.0f)",
    "else if (gi.arcBaselineY < -30.0f)", "gi.arcBaselineY -= gi.maxHeight;",
    "if (gi.sparkY < 0.0f)", "if (gi.sparkY < -30.0f)", "gi.sparkY -= gi.maxHeight;",
    "if (gi.vecTPreY < 0.0f)", "gi.vecTPreY -= gi.maxHeight;",
    "if (gi.vecTNowY < 0.0f)", "gi.vecTNowY -= gi.maxHeight;", "return true;",
]
check("case 0x1D source-to-implementation covers every angle/vector branch and preserves branch/RNG order",
      all(compact(effect) in compact(source_aura2) for effect in aura2_source_effects)
      and all(compact(effect) in compact(current_aura2) for effect in aura2_current_effects)
      and current_aura2.index("const short longitude") < current_aura2.index("prim->m_pos =")
      < current_aura2.index("if (gi.life != 1)")
      < current_aura2.index("for (int angle = 0")
      < current_aura2.index("if (gi.arcBaselineY <")
      < current_aura2.index("gi.arcBaselineY -=")
      < current_aura2.index("if (gi.sparkY <")
      < current_aura2.index("gi.sparkY -=")
      < current_aura2.index("if (gi.vecTPreY <")
      < current_aura2.index("gi.vecTPreY -=")
      < current_aura2.index("if (gi.vecTNowY <")
      < current_aura2.index("gi.vecTNowY -="))
source_chook_decompiled = body(REF_EFFECT2, "unsigned __int8 __thiscall CEffectPrim::PrimCHOOKGI(CEffectPrim *this)\n{")
exe_chook = body(EXE, "unsigned __int8 __thiscall CEffectPrim::PrimCHOOKGI(CEffectPrim *this)\n{")
source_chook = body(REF_DHXJ_EFFECT2, "CEffectPrim :: PrimCHOOKGI()")
current_chook = body(CURRENT, "bool UpdateReferenceChookGi(CEffectPrim* prim)")
check("case 0x20 Ref/EXE helper bodies match and dispatcher honors the always-true handler result",
      compact(source_chook_decompiled) == compact(exe_chook)
      and re.search(r"case\s+0x20:\s*result = CEffectPrim::PrimCHOOKGI\(this\);\s*if\s*\(\s*!result\s*\)\s*return result;\s*goto\s+\$L145961;", source_dispatch)
      and "if (m_type == PP_CHOOKGI)" in current_dispatch
      and "return UpdateReferenceChookGi(this); // Source tests this return; all helper paths return true." in current_dispatch
      and len(current_explicit_ids) == 84)
chook_source_effects = [
    "m_GI[0].alphaT=pView->GetCurLongitude();", "m_GI[1].alphaT=360-m_GI[1].alphaT+90;",
    "m_pos = m_master->m_pos;", "if ( m_GI[ec].life == 1 )", "m_GI[ec].process++;",
    "if (m_GI[ec].process<=200)", "m_GI[ec].alphaB+=1;", "for (i=0;i<6;i+=2)",
    "if (m_GI[ec].height[i+1] > m_GI[ec].height[i])", "m_GI[ec].height[i]+=1;",
    "m_GI[ec].height[i]-=1;",
    "m_GI[ec].flag1[0] == 0", "if (m_GI[ec].process%2 == 0)",
    "m_GI[ec].height[6]>=360", "m_GI[ec].height[10] == 1",
    "m_GI[ec].vecB_now.x = 0.7f * g_sinTable[int(m_GI[ec].height[0])];",
    "m_GI[ec].vecB_now.x += 5.5f * g_sinTable[int(m_GI[ec].height[6])];",
    "m_GI[ec].vecB_now.z += 5.5f * g_cosTable[int(m_GI[ec].height[6])];",
    "m_GI[ec].flag1[0] == 0", "m_GI[ec].vecB_now.x = 1.0f *",
    "m_GI[ec].vecB_now.x = 0.5f *", "m_GI[ec].vecB_now.x += 7.0f *",
    "m_GI[ec].vecB_now.y += m_GI[ec].height[7];",
    "m_GI[ec].height[8]+=5+random(5);", "m_GI[ec].height[8]>=360", "return true;",
]
chook_current_effects = [
    "static_cast<short>(360 - longitude)", "static_cast<short>(360 - latitude + 90)",
    "if (cameraLatitude >= 360)", "prim->m_pos = prim->m_master->EffectPosition();",
    "if (gi.life != 1) return true;", "++gi.process;", "if (gi.process <= 200) ++gi.alphaB;",
    "for (int angle = 0; angle < 6; angle += 2)", "if (target > current)",
    "current += 1.0f;", "if (current > 359.0f) current = 359.0f;", "if (current > target)",
    "current -= 1.0f;", "if (current < 0.0f) current = 0.0f;", "if (current <= target)",
    "if (gi.flag[0] == 0 || gi.process % 2 == 0)", "gi.height[6] += 1.0f;",
    "if (gi.height[6] >= 360.0f) gi.height[6] -= 360.0f;", "if (gi.height[10] == 1.0f)",
    "gi.sparkX = 0.7f * GetSin", "gi.sparkX += 5.5f * GetSin", "gi.sparkZ += 5.5f * GetCos",
    "const float radialScale = gi.flag[0] == 0 ? 1.0f : 0.5f;", "gi.sparkX += 7.0f * GetSin",
    "gi.sparkZ += 7.0f * GetCos", "gi.sparkY += gi.height[7];",
    "gi.height[8] += static_cast<float>(5 + rand() % 5);", "if (gi.height[8] >= 360.0f)",
    "return true;",
]
check("case 0x20 source-to-implementation maps alpha/process/angle/vector branches and final RNG order",
      all(compact(effect) in compact(source_chook) for effect in chook_source_effects)
      and all(compact(effect) in compact(current_chook) for effect in chook_current_effects)
      and current_chook.index("const short longitude") < current_chook.index("prim->m_pos =")
      < current_chook.index("if (gi.life != 1)")
      < current_chook.index("++gi.process") < current_chook.index("for (int angle")
      < current_chook.index("if (gi.flag[0] == 0 ||")
      < current_chook.index("if (gi.height[10] == 1.0f)")
      < current_chook.index("gi.sparkY += gi.height[7];")
      < current_chook.index("gi.height[8] += static_cast<float>(5 + rand() % 5);")
      < current_chook.rindex("return true;"))
source_gi3_decompiled = body(REF_EFFECT2, "void __thiscall CEffectPrim::PrimGI3(CEffectPrim *this)\n{")
exe_gi3 = body(EXE, "void __thiscall CEffectPrim::PrimGI3(CEffectPrim *this)\n{")
source_gi3 = body(REF_DHXJ_EFFECT2, "CEffectPrim :: PrimGI3()")
current_gi3 = body(CURRENT, "void UpdateReferenceGi3(CEffectPrim* prim)")
check("case 0x22 Ref/EXE helper bodies match, source has 21 samples and dispatcher keeps the shared tick",
      compact(source_gi3_decompiled) == compact(exe_gi3)
      and re.search(r"case\s+0x22:\s*CEffectPrim::PrimGI3\(this\);\s*goto\s+\$L145961;", source_dispatch)
      and "if (m_type == PP_GI_3)" in current_dispatch
      and "UpdateReferenceGi3(this);" in current_dispatch
      and re.search(r"#define\s+E_DIVISION\s+21", REF_DHXJ_EFFECT_HEADER)
      and len(current_explicit_ids) == 84)
gi3_source_effects = [
    "middle=(E_DIVISION-1)/2;", "m2=90/middle;", "for (;ec<4;ec++)",
    "if (m_GI[ec].life == 1)", "m_GI[ec].process+=1;", "m_GI[ec].distance-=0.01f;",
    "if (m_GI[ec].process>=10)", "m_GI[ec].alphaB-=1;", "if (m_GI[ec].alphaB<=0)",
    "m_GI[ec].alphaB = 0;", "if (m_GI[ec].process>30)", "m_GI[ec].life=0;",
    "m_GI[ec].distance = 20.0f;", "m_GI[ec].rise_angle=81 + 9;",
    "m_GI[ec].alphaB+=10;", "for (i = 0 ; i<E_DIVISION ; i++)",
    "m_GI[ec].height[i]=m_GI[ec].max_height*g_sinTable[m_GI[ec].process];",
]
gi3_current_effects = [
    "for (EffectGi& gi : prim->m_gi)", "if (gi.life != 1) continue;", "++gi.process;",
    "static_cast<double>(gi.distance) - 0.0099999998", "if (gi.process < 10)",
    "gi.alphaB = static_cast<short>(gi.alphaB + 10);", "gi.alphaB = static_cast<short>(gi.alphaB - 1);",
    "if (gi.alphaB <= 0)", "gi.alphaB = 0;", "if (gi.process > 30) gi.life = 0;",
    "gi.distance = 20.0f;", "gi.riseAngle = static_cast<short>(81 + 9);",
    "for (int index = 0; index < 21; ++index)",
    "gi.height[static_cast<size_t>(index)] = gi.maxHeight * g_sinTable[gi.process];",
]
check("case 0x22 source-to-implementation compares process/alpha/reset branches and post-reset height order",
      all(compact(effect) in compact(source_gi3) for effect in gi3_source_effects)
      and all(compact(effect) in compact(current_gi3) for effect in gi3_current_effects)
      and current_gi3.index("++gi.process;") < current_gi3.index("gi.distance =")
      < current_gi3.index("if (gi.process < 10)")
      < current_gi3.index("gi.alphaB = static_cast<short>(gi.alphaB - 1);")
      < current_gi3.index("if (gi.alphaB <= 0)")
      < current_gi3.index("if (gi.process > 30) gi.life = 0;")
      < current_gi3.index("gi.distance = 20.0f;")
      < current_gi3.index("for (int index = 0; index < 21; ++index)"))
source_gumgang_decompiled = body(REF_EFFECT2, "unsigned __int8 __thiscall CEffectPrim::PrimGumGang(CEffectPrim *this)\n{")
exe_gumgang = body(EXE, "unsigned __int8 __thiscall CEffectPrim::PrimGumGang(CEffectPrim *this)\n{")
source_gumgang = body(REF_DHXJ_EFFECT2, "CEffectPrim :: PrimGumGang()")
current_gumgang = body(CURRENT, "bool UpdateReferenceGumGang(CEffectPrim* prim)")
check("cases 0x23/0x63 Ref/EXE share PrimGumGang and the dispatcher ignores its result",
      compact(source_gumgang_decompiled) == compact(exe_gumgang)
      and re.search(r"case\s+0x23:\s*case\s+0x63:\s*CEffectPrim::PrimGumGang\(this\);\s*goto\s+\$L145961;", source_dispatch)
      and "m_type == PP_GUMGANG || m_type == PP_DOUBLEGUMGANG" in current_dispatch
      and "UpdateReferenceGumGang(this); // Source ids 0x23/0x63 ignore the bool result." in current_dispatch
      and len(current_explicit_ids) == 84)
gumgang_source_effects = [
    "m_GI[0].alphaT=pView->GetCurLongitude();", "m_GI[1].alphaT=360-m_GI[1].alphaT+90;",
    "m_pos = m_master->m_pos;", "range=8;", "half=range/2;", "he=8;", "tn=5;",
    "for (ec=0;ec<4;ec++)", "if (m_GI[ec].life == 1)", "m_GI[ec].process++;",
    "for (i=0;i<=12;i+=4)", "m_GI[ec].height[i]++;",
    "if (m_GI[ec].height[i]<10)", "m_GI[ec].height[i+1]+=25;",
    "m_GI[ec].height[i+1]-=12;", "if (m_GI[ec].height[i+1]<12)",
    "m_GI[ec].height[i]=0;", "m_GI[ec].height[i+1]=0;",
    "m_GI[ec].height[i+3]=random(360);", "m_GI[ec].height[i+2]=random(tn);",
    "m_GI[ec].vecB_pre.y=-random(he)-3;", "m_GI[ec].vecB_now.y=-random(he)-3;",
    "m_GI[ec].vecT_pre.y=-random(he)-3;", "m_GI[ec].vecT_now.y=-random(he)-3;",
    "return true;",
]
gumgang_current_effects = [
    "static_cast<short>(360 - longitude)", "static_cast<short>(360 - latitude + 90)",
    "if (cameraLatitude >= 360)", "prim->m_pos = prim->m_master->EffectPosition();",
    "constexpr int he = 8;", "constexpr int tn = 5;", "for (EffectGi& gi : prim->m_gi)",
    "if (gi.life != 1) continue;", "++gi.process;", "for (int index = 0; index <= 12; index += 4)",
    "gi.height[index] += 1.0f;", "if (gi.height[index] < 10.0f)",
    "gi.height[index + 1] += 25.0f;", "gi.height[index + 1] -= 12.0f;",
    "if (gi.height[index + 1] < 12.0f)", "gi.height[index] = 0.0f;",
    "gi.height[index + 1] = 0.0f;", "gi.height[index + 3] = static_cast<float>(rand() % 360);",
    "gi.height[index + 2] = static_cast<float>(rand() % tn);", "rand() % he",
    "case 0: gi.arcBaselineY = -fallingHeight - 3.0f;", "case 4: gi.sparkY = -fallingHeight - 3.0f;",
    "case 8: gi.vecTPreY = -fallingHeight - 3.0f;", "default: gi.vecTNowY = -fallingHeight - 3.0f;",
    "return true;",
]
check("cases 0x23/0x63 source-to-implementation compare four groups, reset thresholds, vector mapping and RNG order",
      all(compact(effect) in compact(source_gumgang) for effect in gumgang_source_effects)
      and all(compact(effect) in compact(current_gumgang) for effect in gumgang_current_effects)
      and current_gumgang.index("const short longitude") < current_gumgang.index("prim->m_pos =")
      < current_gumgang.index("for (EffectGi& gi")
      < current_gumgang.index("++gi.process;")
      < current_gumgang.index("for (int index = 0;")
      < current_gumgang.index("gi.height[index] +=")
      < current_gumgang.index("gi.height[index + 1] -=")
      < current_gumgang.index("gi.height[index + 3] =")
      < current_gumgang.index("gi.height[index + 2] =")
      < current_gumgang.index("const float fallingHeight")
      < current_gumgang.index("switch (index)"))
source_grandcross_decompiled = body(REF_EFFECT2, "void __thiscall CEffectPrim::PrimGrandCross2(CEffectPrim *this)\n{")
exe_grandcross = body(EXE, "void __thiscall CEffectPrim::PrimGrandCross2(CEffectPrim *this)\n{")
source_grandcross = body(REF_DHXJ_EFFECT2, "CEffectPrim :: PrimGrandCross2()")
current_grandcross = body(CURRENT, "void UpdateReferenceGrandCross2(CEffectPrim* prim)")
check("case 0x25 Ref/EXE helper bodies match and source dispatch continues to the shared tick",
      compact(source_grandcross_decompiled) == compact(exe_grandcross)
      and re.search(r"case\s+0x25:\s*CEffectPrim::PrimGrandCross2\(this\);\s*goto\s+\$L145961;", source_dispatch)
      and "if (m_type == PP_GRANDCROSS2)" in current_dispatch
      and "UpdateReferenceGrandCross2(this);" in current_dispatch
      and len(current_explicit_ids) == 84)
grandcross_source_effects = [
    "middle=(E_DIVISION-1)/2;", "m2=90/middle;", "for (;ec<4;ec++)",
    "if (m_GI[ec].life == 1)", "m_GI[ec].process+=1;", "if (m_GI[ec].process>=10)",
    "m_GI[ec].alphaB-=1;", "if (m_GI[ec].alphaB<=0)", "m_GI[ec].alphaB = 0;",
    "if (m_GI[ec].process>30)", "m_GI[ec].life=0;", "m_GI[ec].alphaB+=9;",
    "m_GI[ec].height[1]+=0.005f;", "m_GI[ec].height[0]+=0.005f;",
    "m_GI[ec].height[2]=m_GI[ec].max_height*g_sinTable[m_GI[ec].process];",
]
grandcross_current_effects = [
    "for (EffectGi& gi : prim->m_gi)", "if (gi.life != 1) continue;", "++gi.process;",
    "if (gi.process >= 10)", "gi.alphaB = static_cast<short>(gi.alphaB - 1);",
    "if (gi.alphaB <= 0)", "gi.alphaB = 0;", "if (gi.process > 30) gi.life = 0;",
    "gi.alphaB = static_cast<short>(gi.alphaB + 9);", "gi.height[1] += 0.005f;",
    "gi.height[0] += 0.005f;", "gi.height[2] = gi.maxHeight * g_sinTable[gi.process];",
]
check("case 0x25 source-to-implementation pairs alpha/fade/life and height writes in exact order",
      all(compact(effect) in compact(source_grandcross) for effect in grandcross_source_effects)
      and all(compact(effect) in compact(current_grandcross) for effect in grandcross_current_effects)
      and current_grandcross.index("++gi.process;") < current_grandcross.index("if (gi.process >= 10)")
      < current_grandcross.index("gi.alphaB = static_cast<short>(gi.alphaB - 1);")
      < current_grandcross.index("if (gi.alphaB <= 0)")
      < current_grandcross.index("if (gi.process > 30) gi.life = 0;")
      < current_grandcross.index("gi.height[1] += 0.005f;")
      < current_grandcross.index("gi.height[0] += 0.005f;")
      < current_grandcross.index("gi.height[2] = gi.maxHeight * g_sinTable[gi.process];"))
source_linelink_decompiled = body(REF_EFFECT2, "unsigned __int8 __thiscall CEffectPrim::PrimLineLink(CEffectPrim *this)\n{")
exe_linelink = body(EXE, "unsigned __int8 __thiscall CEffectPrim::PrimLineLink(CEffectPrim *this)\n{")
source_linelink = body(REF_DHXJ_EFFECT2, "CEffectPrim :: PrimLineLink()")
current_linelink = body(CURRENT, "bool UpdateReferenceLineLink(CEffectPrim* prim)")
check("case 0x26 Ref/EXE helper bodies match and source dispatch ignores helper return",
      compact(source_linelink_decompiled) == compact(exe_linelink)
      and re.search(r"case\s+0x26:\s*CEffectPrim::PrimLineLink\(this\);\s*goto\s+\$L145961;", source_dispatch)
      and "if (m_type == PP_LINELINK)" in current_dispatch
      and "UpdateReferenceLineLink(this);" in current_dispatch)
linelink_source_effects = [
    "m_pos = m_master->m_pos;", "m_GI[ec].process++;", "m_GI[ec].vecT_now=m_pos;",
    "m_GI[ec].vecT_now.y-=8.0f;", "if (m_GI[ec].height[4] == 2)",
    "if (m_GI[ec].process>40)", "m_GI[ec].alphaB-=3;", "if (m_GI[ec].alphaB<0)",
    "else if (m_GI[ec].process<20)", "m_GI[ec].alphaB+=6;",
    "else if (m_GI[ec].height[4] == 3)", "pro=m_GI[ec].process%360;",
    "if (m_GI[ec].process<=80)", "m_GI[ec].alphaB+=1;",
    "m_GI[ec].max_height+=g_sinTable[pro]*0.005f;", "if (m_GI[ec].process%10<5)",
    "m_GI[ec].alphaB+=2;", "m_GI[ec].alphaB-=2;", "if (m_GI[ec].height[0]>0)",
    "m_GI[ec].height[0] == g_session.GetAid()", "gameMode->GetWorld()->m_player;",
    "gameMode->GetWorld()->GetGameActorByAID(m_GI[ec].height[0]);",
    "m_GI[ec].life=0;", "m_GI[ec].alphaB=0;", "m_GI[ec].vecT_pre=tActor->m_pos;",
    "m_GI[ec].vecT_pre.y-=8.0f;", "return true;",
]
linelink_current_effects = [
    "prim->m_pos = prim->m_master->EffectPosition();", "++gi.process;", "gi.vecTNowX = prim->m_pos.x;",
    "gi.vecTNowY = prim->m_pos.y - 8.0f;", "if (gi.height[4] == 2.0f)",
    "if (gi.process > 40)", "gi.alphaB = static_cast<short>(gi.alphaB - 3);",
    "if (gi.alphaB < 0) gi.alphaB = 0;", "else if (gi.process < 20)",
    "gi.alphaB = static_cast<short>(gi.alphaB + 6);", "else if (gi.height[4] == 3.0f)",
    "const int phase = gi.process % 360;", "if (gi.process <= 80)",
    "gi.alphaB = static_cast<short>(gi.alphaB + 1);", "gi.maxHeight += g_sinTable[phase] * 0.005f;",
    "else if (gi.process % 10 < 5)", "gi.alphaB = static_cast<short>(gi.alphaB + 2);",
    "gi.alphaB = static_cast<short>(gi.alphaB - 2);", "if (gi.height[0] > 0.0f && world)",
    "targetAid == g_session.m_aid ? world->m_player", "world->GetGameActorByAID(targetAid);",
    "gi.life = 0;", "gi.alphaB = 0;", "gi.vecTPreX = target->m_pos.x;",
    "gi.vecTPreY = target->m_pos.y - 8.0f;", "gi.vecTPreZ = target->m_pos.z;", "return true;",
]
check("case 0x26 source-to-implementation branch mapping covers every mode, target path and return",
      all(compact(effect) in compact(source_linelink) for effect in linelink_source_effects)
      and all(compact(effect) in compact(current_linelink) for effect in linelink_current_effects)
      and current_linelink.index("prim->m_pos = prim->m_master->EffectPosition();") < current_linelink.index("++gi.process;")
      < current_linelink.index("gi.vecTNowX =") < current_linelink.index("if (gi.height[4] == 2.0f)")
      < current_linelink.index("else if (gi.height[4] == 3.0f)")
      < current_linelink.index("else if (gi.process % 10 < 5)")
      < current_linelink.index("if (gi.height[0] > 0.0f && world)")
      < current_linelink.index("if (!target)") < current_linelink.index("return true;"))
source_landprotector_decompiled = body(REF_EFFECT2, "unsigned __int8 __thiscall CEffectPrim::PrimLandProtector(CEffectPrim *this)\n{")
exe_landprotector = body(EXE, "unsigned __int8 __thiscall CEffectPrim::PrimLandProtector(CEffectPrim *this)\n{")
source_landprotector = body(REF_DHXJ_EFFECT2, "CEffectPrim :: PrimLandProtector()")
current_landprotector = body(CURRENT, "bool UpdateReferenceLandProtector(CEffectPrim* prim)")
check("case 0x27 Ref/EXE helper bodies match and source dispatch ignores helper return",
      compact(source_landprotector_decompiled) == compact(exe_landprotector)
      and re.search(r"case\s+0x27:\s*CEffectPrim::PrimLandProtector\(this\);\s*goto\s+\$L145961;", source_dispatch)
      and "if (m_type == PP_LANDPROTECTOR)" in current_dispatch
      and "UpdateReferenceLandProtector(this);" in current_dispatch)
landprotector_source_effects = [
    "m_pos = m_master->m_pos;", "for (ec=0;ec<2;ec++)", "m_GI[ec].height[4] == 2",
    "m_GI[ec].rise_angle+=1;", "m_GI[ec].rise_angle+=3;",
    "if (m_GI[ec].rise_angle>=360)", "m_GI[ec].rise_angle-=360;", "return true;",
]
landprotector_current_effects = [
    "prim->m_pos = prim->m_master->EffectPosition();", "for (size_t index = 0; index < 2; ++index)",
    "EffectGi& gi = prim->m_gi[index];", "gi.height[4] == 2.0f ? 1 : 3",
    "if (gi.riseAngle >= 360)", "gi.riseAngle = static_cast<short>(gi.riseAngle - 360);", "return true;",
]
check("case 0x27 source-to-implementation compares both unconditional slots, exact mode and single wrap order",
      all(compact(effect) in compact(source_landprotector) for effect in landprotector_source_effects)
      and all(compact(effect) in compact(current_landprotector) for effect in landprotector_current_effects)
      and current_landprotector.index("prim->m_pos =") < current_landprotector.index("for (size_t index")
      < current_landprotector.index("gi.height[4] == 2.0f ? 1 : 3")
      < current_landprotector.index("if (gi.riseAngle >= 360)")
      < current_landprotector.index("gi.riseAngle = static_cast<short>(gi.riseAngle - 360);"))
source_sakura_decompiled = body(REF_EFFECT2, "bool __thiscall CEffectPrim::PrimSakura(CEffectPrim *this)\n{")
exe_sakura = body(EXE, "bool __thiscall CEffectPrim::PrimSakura(CEffectPrim *this)\n{")
source_sakura = body(REF_DHXJ_EFFECT2, "CEffectPrim :: PrimSakura()")
current_sakura = body(CURRENT, "bool UpdateReferenceSakura(CEffectPrim* prim)")
check("case 0x28 Ref/EXE helper bodies match and source dispatch ignores helper return",
      compact(source_sakura_decompiled) == compact(exe_sakura)
      and re.search(r"case\s+0x28:\s*CEffectPrim::PrimSakura\(this\);\s*goto\s+\$L145961;", source_dispatch)
      and "if (m_type == PP_SAKURA)" in current_dispatch
      and "UpdateReferenceSakura(this);" in current_dispatch)
sakura_source_effects = [
    "m_pos = m_master->m_pos;", "if (m_GI[ec].life == 1)", "for (i=0;i<4;i+=2)",
    "m_GI[ec].height[i+1] > m_GI[ec].height[i]", "m_GI[ec].height[i]+=3;",
    "m_GI[ec].height[i] > 359", "m_GI[ec].height[i] = 359;",
    "m_GI[ec].height[i] > m_GI[ec].height[i+1]", "random(int(m_GI[ec].height[i]))",
    "m_GI[ec].height[i]-=2;", "m_GI[ec].height[i] < 0", "m_GI[ec].height[i] = 0;",
    "m_GI[ec].height[i] <= m_GI[ec].height[i+1]", "359 - random(int(m_GI[ec].height[i]))",
    "m_GI[ec].vecB_pre.y += m_GI[ec].max_height;", "m_GI[ec].flag1[0] == 0",
    "0.24f * g_sinTable[int(m_GI[ec].height[0])]", "0.30f * g_sinTable[int(m_GI[ec].height[2])]",
    "0.12f * g_sinTable[int(m_GI[ec].height[0])]", "0.15f * g_sinTable[int(m_GI[ec].height[2])]",
    "if (m_GI[ec].vecB_pre.y>m_pos.y)", "m_pos.x+random(300)-150",
    "m_pos.y-200", "m_pos.z+random(300)-150", "m_deltaPos=m_GI[ec].vecB_pre;",
    "m_pos = m_deltaPos;", "if (m_pattern & PT_ROTATESPR)",
    "m_roll = m_roty - gameMode->GetView()->GetCurLongitude();", "ProcessAlpha();", "ShiftSegment();",
    "if (!AnimMotion()) return false;", "return true;",
]
sakura_current_effects = [
    "prim->m_pos = prim->m_master->EffectPosition();", "if (gi.life == 1)",
    "for (int index = 0; index < 4; index += 2)",
    "gi.height[static_cast<size_t>(index + 1)] > gi.height[static_cast<size_t>(index)]",
    "gi.height[static_cast<size_t>(index)] += 3.0f;", "> 359.0f",
    "gi.height[static_cast<size_t>(index)] = 359.0f;",
    "gi.height[static_cast<size_t>(index)] > gi.height[static_cast<size_t>(index + 1)]",
    "const int bound = static_cast<int>(gi.height[static_cast<size_t>(index)]);",
    "bound > 0 ? static_cast<float>(rand() % bound) : 0.0f",
    "gi.height[static_cast<size_t>(index)] -= 2.0f;", "< 0.0f",
    "gi.height[static_cast<size_t>(index)] = 0.0f;",
    "gi.height[static_cast<size_t>(index)] <= gi.height[static_cast<size_t>(index + 1)]",
    "359 - randomValue", "gi.arcBaselineY = static_cast<float>(static_cast<double>(gi.arcBaselineY) + gi.maxHeight);",
    "if (gi.flag[0] == 0)", "0.23999999", "0.30000001", "0.12", "0.15000001",
    "if (gi.arcBaselineY > prim->m_pos.y)", "rand() % 300", "prim->m_pos.y - 200.0f",
    "prim->m_deltaPos = {gi.vecBPreX, gi.arcBaselineY, gi.vecBPreZ};", "prim->m_pos = prim->m_deltaPos;",
    "if ((prim->m_pattern & 0x20) != 0)", "prim->m_roll = prim->m_roty - mode->m_view->GetCameraLongitude();",
    "ProcessReferenceParticleAlpha(prim);", "ShiftReferenceSegments(prim);", "return AnimateReferenceParticle(prim);",
]
check("case 0x28 source-to-implementation compares paired height branches, bounded RNG, drift/respawn, stored position and shared calls",
      all(compact(effect) in compact(source_sakura) for effect in sakura_source_effects)
      and all(compact(effect) in compact(current_sakura) for effect in sakura_current_effects)
      and current_sakura.index("prim->m_pos = prim->m_master->EffectPosition();") < current_sakura.index("if (gi.life == 1)")
      < current_sakura.index("for (int index = 0;") < current_sakura.index("gi.arcBaselineY = static_cast<float>")
      < current_sakura.index("if (gi.arcBaselineY > prim->m_pos.y)")
      < current_sakura.index("prim->m_deltaPos = {") < current_sakura.index("prim->m_pos = prim->m_deltaPos;")
      < current_sakura.index("ProcessReferenceParticleAlpha(prim);")
      < current_sakura.index("ShiftReferenceSegments(prim);")
      < current_sakura.index("return AnimateReferenceParticle(prim);"))
source_boomerang_decompiled = body(REF_EFFECT2, "void __thiscall CEffectPrim::PrimShieldBoomerang(CEffectPrim *this)\n{")
exe_boomerang = body(EXE, "void __thiscall CEffectPrim::PrimShieldBoomerang(CEffectPrim *this)\n{")
source_boomerang = body(REF_DHXJ_EFFECT2, "CEffectPrim :: PrimShieldBoomerang()")
current_boomerang = body(CURRENT, "void UpdateReferenceShieldBoomerang(CEffectPrim* prim)")
check("case 0x2A Ref/EXE helper bodies match and source dispatch ignores helper return",
      compact(source_boomerang_decompiled) == compact(exe_boomerang)
      and re.search(r"case\s+0x2A:\s*CEffectPrim::PrimShieldBoomerang\(this\);\s*goto\s+\$L145961;", source_dispatch)
      and "if (m_type == PP_SHIELDBOOMERANG)" in current_dispatch
      and "UpdateReferenceShieldBoomerang(this);" in current_dispatch)
boomerang_source_effects = [
    "m_GI[0].alphaT=pView->GetCurLongitude();", "m_GI[0].alphaT=360-m_GI[0].alphaT;",
    "m_GI[1].alphaT=pView->GetCurLatitude();", "m_GI[1].alphaT=360-m_GI[1].alphaT+90;",
    "if (m_GI[1].alphaT>=360)", "m_pos = m_master->m_pos;", "if (m_GI[ec].life == 1)",
    "m_GI[ec].process++;", "if (m_GI[ec].process>0)", "if (m_GI[ec].flag1[2] == 1)",
    "ra=1;", "ra=3;", "if (m_GI[ec].process<=10)", "m_GI[ec].alphaB+=25;",
    "if (m_GI[ec].alphaB>=255)", "if (m_GI[ec].process%ra == 0)",
    "if (m_GI[ec].process > 3 )", "m_GI[ec+3].distance = m_GI[ec+2].distance;",
    "m_GI[ec+3].height[8] = m_GI[ec+2].height[8];", "m_GI[ec+3].alphaB = m_GI[ec+2].alphaB - 25;",
    "m_GI[ec+3].vecB_now=m_GI[ec+2].vecB_now;", "m_GI[ec+3].flag1[0] = m_GI[ec+2].flag1[0];",
    "if (m_GI[ec+3].alphaB < 0)", "if (m_GI[ec+2].alphaB < 0)", "if (m_GI[ec+1].alphaB < 0)",
    "if (m_GI[ec].process > 2 )", "if (m_GI[ec].process > 1 )", "m_GI[ec].full_display_angle-=15;",
    "if (m_GI[ec].full_display_angle<0)", "if (m_GI[ec].process == 1)",
    "Get_Angle(Rx,Rz,&m_GI[ec].RotStart);", "if ( m_GI[ec].max_height < m_GI[ec].height[1]*m_GI[ec].process )",
    "m_GI[ec].flag1[1] = 2;", "m_GI[ec].height[1]*=3;", "if (random(2)==0)",
    "m_GI[ec].RotStart+=random(45);", "m_GI[ec].RotStart-=random(45);", "if (m_GI[ec].flag1[1] == 1 )",
    "if (m_GI[ec].RotStart>=360)", "if (m_GI[ec].RotStart<0)", "if (m_GI[ec].alphaB<0)",
    "Get_Angle(Rx,Rz,&angle);", "m_GI[ec].RotStart < angle - 4 || m_GI[ec].RotStart > angle + 4",
    "m_GI[ec].RotStart+=5;", "m_GI[ec].flag1[1] = 2;", "m_GI[ec].alphaB-=12;",
    "m_GI[ec].vecB_now.y -= m_GI[ec].height[2]*1.0f;",
    "m_GI[ec].vecB_now.x+=m_GI[ec].height[1]*g_cosTable[m_GI[ec].RotStart];",
    "m_GI[ec].vecB_now.z+=m_GI[ec].height[1]*g_sinTable[m_GI[ec].RotStart];",
]
boomerang_current_effects = [
    "prim->m_gi[0].alphaT", "prim->m_gi[1].alphaT", "prim->m_pos = prim->m_master->EffectPosition();",
    "if (gi.life != 1) return;", "++gi.process;", "if (gi.process <= 0) return;",
    "const int cadence = gi.flag[2] == 1 ? 1 : 3;", "if (gi.process <= 10)",
    "gi.alphaB = static_cast<short>(gi.alphaB + 25);", "if (gi.alphaB >= 255) gi.alphaB = 255;",
    "if (gi.process % cadence == 0)", "if (gi.process > 3)", "if (gi.process > 2)", "if (gi.process > 1)",
    "destination.distance = source.distance;", "destination.height[8] = source.height[8];",
    "destination.alphaB = static_cast<short>(source.alphaB - 25);", "destination.sparkX = source.sparkX;",
    "destination.flag[0] = source.flag[0];", "if (destination.alphaB < 0) destination.alphaB = 0;",
    "gi.fullDisplayAngle = static_cast<short>(gi.fullDisplayAngle - 15);",
    "if (gi.process == 1)", "gi.rotStart = static_cast<short>(getAngle(deltaX, deltaZ));",
    "static_cast<double>(gi.process) * gi.height[1] > gi.maxHeight", "gi.flag[1] = 2;", "gi.height[1] *= 3.0f;",
    "if (rand() % 2 == 0)", "rand() % 45", "gi.sparkY = static_cast<float>(static_cast<double>(gi.sparkY) + gi.height[2]);",
    "else if (gi.flag[1] == 1)", "const int targetAngle = getAngle", "targetAngle - 4", "targetAngle + 4",
    "gi.rotStart = static_cast<short>(gi.rotStart + 5);", "gi.flag[1] = 2;",
    "gi.alphaB = static_cast<short>(gi.alphaB - 12);", "gi.sparkY = static_cast<float>(static_cast<double>(gi.sparkY) - gi.height[2] * 1.0);",
    "gi.sparkX = static_cast<float>(static_cast<double>(gi.height[1]) * g_cosTable[gi.rotStart] + gi.sparkX);",
    "gi.sparkZ = static_cast<float>(static_cast<double>(gi.height[1]) * g_sinTable[gi.rotStart] + gi.sparkZ);",
    "if (gi.rotStart >= 360)", "if (gi.rotStart < 0)", "if (gi.alphaB < 0)",
]
check("case 0x2A source-to-implementation compares camera/process gates, trail cadence and descending snapshot branches",
      all(compact(effect) in compact(source_boomerang) for effect in boomerang_source_effects[:27])
      and all(compact(effect) in compact(current_boomerang) for effect in boomerang_current_effects[:20])
      and current_boomerang.index("if (gi.process % cadence == 0)") < current_boomerang.index("if (gi.process > 3)")
      < current_boomerang.index("if (gi.process > 2)") < current_boomerang.index("if (gi.process > 1)"))
check("case 0x2A source-to-implementation compares launch, tether RNG, return tolerance, fade and final trig movement branches",
      all(compact(effect) in compact(source_boomerang) for effect in boomerang_source_effects[27:])
      and all(compact(effect) in compact(current_boomerang) for effect in boomerang_current_effects[20:])
      and current_boomerang.index("if (gi.process == 1)") < current_boomerang.index("if (gi.flag[1] == 0)")
      < current_boomerang.index("else if (gi.flag[1] == 1)") < current_boomerang.index("gi.alphaB = static_cast<short>(gi.alphaB - 12);")
      < current_boomerang.index("gi.sparkX =") < current_boomerang.index("gi.sparkZ =")
      and current_boomerang.index("if (rand() % 2 == 0)") < current_boomerang.index("gi.rotStart = static_cast<short>(gi.rotStart + rand() % 45);")
      < current_boomerang.index("} else {") < current_boomerang.index("gi.rotStart = static_cast<short>(gi.rotStart - rand() % 45);")
      and current_boomerang.index("destination.alphaB = static_cast<short>(source.alphaB - 25);") < current_boomerang.index("destination.alphaB = static_cast<short>(gi.alphaB - 150);")
      and source_boomerang.index("if (random(2)==0)") < source_boomerang.index("m_GI[ec].RotStart+=random(45);")
      < source_boomerang.index("m_GI[ec].RotStart-=random(45);"))
source_intimidate_decompiled = body(REF_EFFECT2, "void __thiscall CEffectPrim::PrimIntimidate(CEffectPrim *this)\n{")
exe_intimidate = body(EXE, "void __thiscall CEffectPrim::PrimIntimidate(CEffectPrim *this)\n{")
source_intimidate = body(REF_DHXJ_EFFECT2, "CEffectPrim :: PrimIntimidate()")
current_intimidate = body(CURRENT, "void UpdateReferenceIntimidate(CEffectPrim* prim)")
check("case 0x2B Ref/EXE helper bodies match and source dispatch ignores helper return",
      compact(source_intimidate_decompiled) == compact(exe_intimidate)
      and re.search(r"case\s+0x2B:\s*CEffectPrim::PrimIntimidate\(this\);\s*goto\s+\$L145961;", source_dispatch)
      and "if (m_type == PP_INTIMIDATE)" in current_dispatch
      and "UpdateReferenceIntimidate(this);" in current_dispatch)
intimidate_source_effects = [
    "m_GI[0].alphaT=pView->GetCurLongitude();", "m_GI[0].alphaT=360-m_GI[0].alphaT;",
    "m_GI[1].alphaT=pView->GetCurLatitude();", "m_GI[1].alphaT=360-m_GI[1].alphaT+90;",
    "if (m_GI[1].alphaT>=360)", "for (ec=0;ec<4;ec++)", "if (m_GI[ec].life == 1)",
    "m_GI[ec].process++;", "if (m_GI[ec].process>0)", "m_GI[ec].distance+=m_GI[ec].height[0];",
    "if (m_GI[ec].process<=10 && m_GI[ec].process>0)", "if (m_GI[ec].flag1[0] == 0)",
    "m_GI[ec].alphaB+=15;", "m_GI[ec].alphaB+=25;", "else if (m_GI[ec].process>25)",
    "m_GI[ec].alphaB-=7;", "if (m_GI[ec].alphaB<=0)", "if (m_GI[ec].flag1[0]==2)",
    "m_GI[ec].RotStart+=3;", "if (m_GI[ec].RotStart>=360)", "m_GI[ec].RotStart-=360;",
    "m_GI[ec].height[2]+=3;", "m_GI[ec].height[2]+=5;", "if (m_GI[ec].height[2]>=360)",
    "m_GI[ec].height[2]-=360;",
]
intimidate_current_effects = [
    "prim->m_gi[0].alphaT", "prim->m_gi[1].alphaT", "for (size_t index = 0; index < 4; ++index)",
    "if (gi.life != 1) continue;", "++gi.process;", "if (gi.process > 0) gi.distance += gi.height[0];",
    "if (gi.process <= 10 && gi.process > 0)", "gi.flag[0] == 0 ? 15 : 25",
    "else if (gi.process > 25)", "gi.alphaB = static_cast<short>(gi.alphaB - 7);", "if (gi.alphaB <= 0) gi.alphaB = 0;",
    "if (gi.flag[0] == 2) gi.rotStart = static_cast<short>(gi.rotStart + 3);",
    "if (gi.rotStart >= 360)", "gi.rotStart = static_cast<short>(gi.rotStart - 360);",
    "gi.height[2] += gi.flag[0] == 2 ? 3.0f : 5.0f;", "if (gi.height[2] >= 360.0f)",
    "gi.height[2] -= 360.0f;",
]
check("case 0x2B source-to-implementation maps all four bands and process, distance and alpha threshold branches",
      all(compact(effect) in compact(source_intimidate) for effect in intimidate_source_effects[:17])
      and all(compact(effect) in compact(current_intimidate) for effect in intimidate_current_effects[:11])
      and current_intimidate.index("++gi.process;") < current_intimidate.index("if (gi.process > 0)")
      < current_intimidate.index("if (gi.process <= 10 && gi.process > 0)")
      < current_intimidate.index("else if (gi.process > 25)"))
check("case 0x2B source-to-implementation maps rotation flag, independent one-step wraps and height2 updates",
      all(compact(effect) in compact(source_intimidate) for effect in intimidate_source_effects[17:])
      and all(compact(effect) in compact(current_intimidate) for effect in intimidate_current_effects[11:])
      and current_intimidate.index("if (gi.flag[0] == 2)") < current_intimidate.index("if (gi.rotStart >= 360)")
      < current_intimidate.index("gi.height[2] += gi.flag[0] == 2 ? 3.0f : 5.0f;")
      < current_intimidate.index("if (gi.height[2] >= 360.0f)"))
source_teihit1_decompiled = body(REF_EFFECT2, "void __thiscall CEffectPrim::PrimTeiHit1(CEffectPrim *this)\n{")
exe_teihit1 = body(EXE, "void __thiscall CEffectPrim::PrimTeiHit1(CEffectPrim *this)\n{")
source_teihit1 = body(REF_DHXJ_EFFECT2, "CEffectPrim :: PrimTeiHit1()")
current_teihit1 = body(CURRENT, "void UpdateReferenceTeiHit1(CEffectPrim* prim)")
check("case 0x2C Ref/EXE helper bodies match and source dispatch ignores helper return",
      compact(source_teihit1_decompiled) == compact(exe_teihit1)
      and re.search(r"case\s+0x2C:\s*CEffectPrim::PrimTeiHit1\(this\);\s*goto\s+\$L145961;", source_dispatch)
      and "if (m_type == PP_TEIHIT1)" in current_dispatch
      and "UpdateReferenceTeiHit1(this);" in current_dispatch)
teihit1_source_effects = [
    "for (ec=0;ec<4;ec++)", "if (m_GI[ec].life == 1)", "m_GI[ec].process++;",
    "if (m_GI[ec].process>0)", "m_GI[ec].distance+=m_GI[ec].height[0];",
    "if (m_GI[ec].process<=20 && m_GI[ec].process>10)", "m_GI[ec].alphaB+=15;",
    "else", "m_GI[ec].alphaB-=5;", "if (m_GI[ec].alphaB<=0)", "m_GI[ec].alphaB=0;",
]
teihit1_current_effects = [
    "for (size_t index = 0; index < 4; ++index)", "if (gi.life != 1) continue;", "++gi.process;",
    "if (gi.process > 0) gi.distance += gi.height[0];", "if (gi.process <= 20 && gi.process > 10)",
    "gi.alphaB = static_cast<short>(gi.alphaB + 15);", "else", "gi.alphaB = static_cast<short>(gi.alphaB - 5);",
    "if (gi.alphaB <= 0) gi.alphaB = 0;",
]
check("case 0x2C source-to-implementation maps the four life-gated bands and ordered distance/alpha branches",
      all(compact(effect) in compact(source_teihit1) for effect in teihit1_source_effects)
      and all(compact(effect) in compact(current_teihit1) for effect in teihit1_current_effects)
      and current_teihit1.index("++gi.process;") < current_teihit1.index("if (gi.process > 0) gi.distance += gi.height[0];")
      < current_teihit1.index("if (gi.process <= 20 && gi.process > 10)")
      < current_teihit1.index("else {") < current_teihit1.index("if (gi.alphaB <= 0)"))
check("current unmatched route has a generic fallback while source reports illegal primitive ids",
      "alpha > 0.0f" in current_dispatch
      and "Illegal Effect PrimitiveId" in source_dispatch)

not_explicit = sorted(source_ids - current_explicit_ids)
print(f"Source-only explicit selectors requiring branch mapping ({len(not_explicit)}): "
      + " ".join(f"0x{value:02X}" for value in not_explicit))
print("Inventory is not a parity pass: contextual UsesReference* routes and generic fallback still require case-by-case branch comparison.")
