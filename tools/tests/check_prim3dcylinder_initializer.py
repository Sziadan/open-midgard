#!/usr/bin/env python3
"""Audit canonical Prim3DCylinder update against Ref and local dispatch."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffectPrim.cpp").read_text(encoding="utf-8", errors="replace")
REF_DHXJ = (ROOT / "Ref/dhxj/RagEffectPrim.cpp").read_text(encoding="utf-8", errors="replace")
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


signature = "bool __thiscall CEffectPrim::Prim3DCylinder(CEffectPrim *this)"
pe = body(PE, signature, PE.index("//----- (00644640)"))
legacy = body(REF_DHXJ, "bool\tCEffectPrim :: Prim3DCylinder()")
primary_mislabeled = body(REF, "bool __thiscall CEffectPrim::Prim3DCylinderHalf(CEffectPrim *this)", REF.index("Original Signature: private: bool __thiscall CEffectPrim::Prim3DCylinder"))
pe_process = body(PE, "unsigned __int8 __thiscall CEffectPrim::OnProcess(CEffectPrim *this)", PE.index("//----- (006405"))
ref_process = body(REF, "unsigned __int8 __thiscall CEffectPrim::OnProcess(CEffectPrim *this)")
local_process = body(LOCAL, "bool CEffectPrim::ProcessPrimitiveFrame()")
local = body(LOCAL, "bool UpdateCylinderPrimitive(CEffectPrim* prim)")

check("canonical PE and secondary Ref Prim3DCylinder bodies preserve accelerated angular motion and update order",
      "m_longSpeed += m_longAccel" in legacy and "m_latiSpeed += m_latiAccel" in legacy
      and "m_longitude += m_longSpeed" in legacy and "m_latitude += m_latiSpeed" in legacy
      and "m_longitude += m_longSpeed" in legacy[legacy.index("m_pos = m_orgPos + m_deltaPos;"):]
      and "Prim3DCylinder" in PE[PE.index(signature):PE.index(signature) + 80]
      and "m_latiSpeed" in pe and "m_longAccel" in pe)
check("primary Ref labels Prim3DCylinder but supplies the duplicate Prim3DCylinderHalf body",
      "Original Signature: private: bool __thiscall CEffectPrim::Prim3DCylinder(void)" in REF
      and "Prim3DCylinderHalf" in REF[REF.index("Original Signature: private: bool __thiscall CEffectPrim::Prim3DCylinder"):]
      and "m_latiSpeed" not in primary_mislabeled)
check("PE and primary Ref OnProcess dispatch case 0x0D to Prim3DCylinder",
      re.search(r"case\s+0xD\s*:[\s\S]{0,120}Prim3DCylinder\(this\)", pe_process) is not None
      and re.search(r"case\s+0xD\s*:[\s\S]{0,120}Prim3DCylinder\(this\)", ref_process) is not None)
check("local type 0x0D route selects the source-derived cylinder path",
      "if (UsesReferenceCylinder())" in local_process
      and "UpdateReferenceCylinderState(this)" in local_process
      and "if (m_type == PP_3DCYLINDER)" in local_process
      and "UpdateCylinderPrimitive(this)" in local_process)
check("comparator records the unresolved PE/local angular-motion gap instead of treating the generic fallback as source parity",
      "m_longSpeed += m_longAccel" in legacy and "m_latiSpeed += m_latiAccel" in legacy
      and "prim->m_longSpeed += prim->m_longAccel" not in local
      and "prim->m_latiSpeed += prim->m_latiAccel" not in local
      and "prim->m_latitude += prim->m_latiSpeed" not in local
      and "reference cylinder uses stored parent height and increments longitude after matrix" in FIXTURE)
print("Prim3DCylinder update comparator passed.")
