#!/usr/bin/env python3
"""Compare canonical Prim3DFlash update and bind it to lifecycle coverage."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffectPrim.cpp").read_text(encoding="utf-8", errors="replace")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
FIXTURE = (ROOT / "tools/tests/EffectLifecycleTests.cpp").read_text(encoding="utf-8")


def body(text: str, marker: str, start: int = 0) -> str:
    index = text.index(marker, start)
    opening = text.index("{", index)
    depth = 0
    for cursor in range(opening, len(text)):
        if text[cursor] == "{":
            depth += 1
        elif text[cursor] == "}":
            depth -= 1
            if depth == 0:
                return text[opening + 1:cursor]
    raise AssertionError(f"unclosed function: {marker}")


def compact(text: str) -> str:
    return re.sub(r"\s+", " ", text).strip()


def check(label: str, condition: bool) -> None:
    assert condition, f"FAIL {label}"
    print(f"PASS {label}")


signature = "void __thiscall CEffectPrim::Prim3DFlash(CEffectPrim *this)"
pe = body(PE, signature, PE.index("//----- (00641C30)"))
ref = body(REF, signature)
pe_process = body(PE, "unsigned __int8 __thiscall CEffectPrim::OnProcess(CEffectPrim *this)", PE.index("//----- (006405"))
ref_process = body(REF, "unsigned __int8 __thiscall CEffectPrim::OnProcess(CEffectPrim *this)")
local_dispatch = body(LOCAL, "bool CEffectPrim::ProcessPrimitiveFrame()")
local = body(LOCAL, "void UpdateReferenceFlash(CEffectPrim* prim)")
local_3d_branch = local[local.index("// The 3D source updates arc before longitude and height."):]

check("canonical PE and primary Ref Prim3DFlash bodies normalize identically", compact(pe) == compact(ref))
check("PE and Ref OnProcess route primitive case 0x13 to Prim3DFlash",
      re.search(r"case\s+0x13\s*:[\s\S]{0,120}Prim3DFlash\(this\)", pe_process) is not None
      and re.search(r"case\s+0x13\s*:[\s\S]{0,120}Prim3DFlash\(this\)", ref_process) is not None)
check("local 3D flash route uses shared updater and resumes the common tick",
      "m_type == PP_2DFLASH || m_type == PP_3DFLASH" in local_dispatch
      and "UpdateReferenceFlash(this)" in local_dispatch
      and "++m_stateCnt" in LOCAL[LOCAL.index("u8 CEffectPrim::OnProcess()"):])
check("local 3D branch preserves Y/X anchor order, early clamp, change/reflection, arc-first update and alpha",
      "prim->m_pos.y = anchor.y + prim->m_deltaPos2.y" in local
      and "prim->m_pos.x = anchor.x + prim->m_deltaPos2.x" in local
      and "(prim->m_pattern & 1) != 0 && prim->m_heightSize <= 0.0f" in local
      and "prim->m_stateCnt == prim->m_changePoint" in local
      and "prim->m_heightSize < 0.0f" in local
      and compact(local_3d_branch).index("prim->m_arcAngle += prim->m_widthSpeed")
      < compact(local_3d_branch).index("prim->m_longitude = static_cast<float>(angularSpeed + prim->m_longitude)")
      < compact(local_3d_branch).index("prim->m_heightSize += prim->m_heightSpeed")
      and "ProcessReferenceParticleAlpha(prim)" in local)
check("lifecycle fixture covers case 0x13 anchoring, trace, change/reflection, arc, alpha and common tick",
      "OnProcess case 0x13 preserves Y/X anchoring, early height trace, change/reflection order and common tick" in FIXTURE
      and "texturePrim.m_arcAngle == 15" in FIXTURE
      and "texturePrim.m_stateCnt == 1" in FIXTURE)
print("Prim3DFlash update comparator passed.")
