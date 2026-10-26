#!/usr/bin/env python3
"""Inventory CEffectPrim::OnProcess source cases and current split dispatch routes."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffectPrim.cpp").read_text(encoding="utf-8", errors="replace")
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
      len(current_explicit_ids) == 67 and {0x00, 0x01, 0x02, 0x04, 0x09, 0x0A, 0x0B, 0x0E, 0x0F, 0x10, 0x11, 0x14, 0x15, 0x16}.issubset(current_explicit_ids)
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
check("current unmatched route has a generic fallback while source reports illegal primitive ids",
      "alpha > 0.0f" in current_dispatch
      and "Illegal Effect PrimitiveId" in source_dispatch)

not_explicit = sorted(source_ids - current_explicit_ids)
print(f"Source-only explicit selectors requiring branch mapping ({len(not_explicit)}): "
      + " ".join(f"0x{value:02X}" for value in not_explicit))
print("Inventory is not a parity pass: contextual UsesReference* routes and generic fallback still require case-by-case branch comparison.")
