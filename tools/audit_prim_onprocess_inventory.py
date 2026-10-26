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
      len(current_explicit_ids) == 61 and {0x04, 0x0E, 0x11, 0x14, 0x15, 0x16}.issubset(current_explicit_ids)
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
check("current unmatched route has a generic fallback while source reports illegal primitive ids",
      "alpha > 0.0f" in current_dispatch
      and "Illegal Effect PrimitiveId" in source_dispatch)

not_explicit = sorted(source_ids - current_explicit_ids)
print(f"Source-only explicit selectors requiring branch mapping ({len(not_explicit)}): "
      + " ".join(f"0x{value:02X}" for value in not_explicit))
print("Inventory is not a parity pass: contextual UsesReference* routes and generic fallback still require case-by-case branch comparison.")
