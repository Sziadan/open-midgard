#!/usr/bin/env python3
"""Compare canonical Prim3DCylinderHalf update and bind it to lifecycle coverage."""
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


signature = "bool __thiscall CEffectPrim::Prim3DCylinderHalf(CEffectPrim *this)"
pe = body(PE, signature, PE.index("//----- (006443E0)"))
ref = body(REF, signature, REF.index("// Original Signature: private: bool __thiscall CEffectPrim::Prim3DCylinderHalf"))
pe_process = body(PE, "unsigned __int8 __thiscall CEffectPrim::OnProcess(CEffectPrim *this)", PE.index("//----- (006405"))
ref_process = body(REF, "unsigned __int8 __thiscall CEffectPrim::OnProcess(CEffectPrim *this)")
local_dispatch = body(LOCAL, "bool CEffectPrim::ProcessPrimitiveFrame()")
local = body(LOCAL, "bool UpdateReferenceCylinderState(CEffectPrim* prim)")

check("canonical PE and primary Ref Prim3DCylinderHalf bodies normalize identically", compact(pe) == compact(ref))
check("PE and Ref OnProcess route primitive case 0x12 to Prim3DCylinderHalf",
      re.search(r"case\s+0x12\s*:[\s\S]{0,150}Prim3DCylinderHalf\(this\)", pe_process) is not None
      and re.search(r"case\s+0x12\s*:[\s\S]{0,150}Prim3DCylinderHalf\(this\)", ref_process) is not None)
check("local cylinder-half route uses the source updater and common tick",
      "m_type == PP_3DCYLINDERHALF" in local_dispatch
      and "UpdateReferenceCylinderState(this)" in local_dispatch
      and "++m_stateCnt" in LOCAL[LOCAL.index("u8 CEffectPrim::OnProcess()"):])
check("local updater preserves source movement, size/height cap, fade-out, alpha and animation order",
      compact(local).index("prim->m_speed += prim->m_accel")
      < compact(local).index("prim->m_speed3d =")
      < compact(local).index("prim->m_deltaPos = AddVec3(prim->m_deltaPos, prim->m_speed3d)")
      < compact(local).index("prim->m_pos = AddVec3(prim->m_orgPos, prim->m_deltaPos)")
      < compact(local).index("prim->m_longitude += prim->m_longSpeed")
      and "prim->m_heightSize > prim->m_maxHeightSize" in local
      and "prim->m_pattern & 1" in local
      and "prim->m_stateCnt == prim->m_fadeOutCnt" in local
      and "ProcessReferenceParticleAlpha(prim)" in local
      and "prim->m_stateCnt % prim->m_animSpeed" in local
      and "prim->m_repeatAnim" in local)
check("lifecycle fixture covers anchored movement, integrated sizes, height cap, fade-out, alpha and ignored completion",
      "OnProcess case 0x12 preserves anchored movement and negative speed translation" in FIXTURE
      and "OnProcess case 0x12 integrates cylinder sizes and clamps height" in FIXTURE
      and "OnProcess case 0x12 applies outer fade and alpha, ignores animation completion, then ticks" in FIXTURE
      and "texturePrim.m_pos.x == 12 && texturePrim.m_pos.y == 21 && texturePrim.m_pos.z == 36" in FIXTURE
      and "texturePrim.m_heightSize == 100" in FIXTURE
      and "texturePrim.m_curMotion == 1 && texturePrim.m_stateCnt == 1" in FIXTURE)
print("Prim3DCylinderHalf update comparator passed.")
