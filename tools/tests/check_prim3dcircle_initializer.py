#!/usr/bin/env python3
"""Compare canonical PP_3DCIRCLE initialization and local change-point behavior."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffectPrim.cpp").read_text(encoding="utf-8", errors="replace")
REF_READABLE = (ROOT / "Ref/dhxj/RagEffectPrim.cpp").read_text(encoding="cp1252", errors="replace")
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


def case_body(dispatch: str) -> str:
    start = re.search(r"case\s+0xC\s*:", dispatch, re.IGNORECASE)
    assert start is not None, "missing primitive case 0x0C"
    end = re.search(r"case\s+0xD\s*:", dispatch[start.end():], re.IGNORECASE)
    assert end is not None, "missing next primitive case after 0x0C"
    return dispatch[start.start():start.end() + end.start()]


signature = "void __thiscall CEffectPrim::Prim3DCircle(CEffectPrim *this)"
pe = body(PE, signature, PE.index("//----- (006441C0)"))
ref = body(REF, signature)
readable_ref = body(REF_READABLE, "void\tCEffectPrim :: Prim3DCircle()")
pe_dispatch = body(PE, "unsigned __int8 __thiscall CEffectPrim::OnProcess(CEffectPrim *this)", PE.index("//----- (00640D30)"))
ref_dispatch = body(REF, "unsigned __int8 __thiscall CEffectPrim::OnProcess(CEffectPrim *this)")
local_dispatch = body(LOCAL, "bool CEffectPrim::ProcessPrimitiveFrame()")
local_reference = body(LOCAL, "bool UpdateReferenceCircleState(CEffectPrim* prim)")
local_generic = body(LOCAL, "bool UpdateCirclePrimitive(CEffectPrim* prim)")
local_tick = body(LOCAL, "u8 CEffectPrim::OnProcess()")
pe_case, ref_case = case_body(pe_dispatch), case_body(ref_dispatch)

check("canonical PE and primary Ref Prim3DCircle bodies normalize identically", compact(pe) == compact(ref))
check("readable Ref loads change speed and acceleration at the exact state boundary",
      re.search(r"m_ChangePoint\s*==\s*m_stateCnt", readable_ref) is not None
      and "m_radiusSpeed = m_ChangeSpeed" in readable_ref
      and "m_radiusAccel = m_ChangeAccel" in readable_ref)
check("canonical PE and primary Ref dispatcher route case 0x0C through Prim3DCircle",
      compact(pe_case) == compact(ref_case)
      and "CEffectPrim::Prim3DCircle(this)" in pe_case
      and "goto $L145961" in pe_case)
check("both local circle paths apply the change-point values before radius integration",
      "if (prim->m_stateCnt == prim->m_changePoint)" in compact(local_reference)
      and "prim->m_radiusSpeed = prim->m_changeSpeed" in compact(local_reference)
      and "prim->m_radiusAccel = prim->m_changeAccel" in compact(local_reference)
      and compact(local_reference).index("prim->m_radiusSpeed = prim->m_changeSpeed")
      < compact(local_reference).index("prim->m_radiusSpeed += prim->m_radiusAccel")
      and "if (prim->m_stateCnt == prim->m_changePoint)" in compact(local_generic)
      and "prim->m_radiusSpeed = prim->m_changeSpeed" in compact(local_generic)
      and "prim->m_radiusAccel = prim->m_changeAccel" in compact(local_generic)
      and compact(local_generic).index("prim->m_radiusSpeed = prim->m_changeSpeed")
      < compact(local_generic).index("prim->m_radiusSpeed += prim->m_radiusAccel"))
check("local dispatcher preserves source-selected and generic circle routes before the common tick",
      "if (UsesReferenceCircle())" in local_dispatch
      and "UpdateReferenceCircleState(this)" in local_dispatch
      and "if (m_type == PP_3DCIRCLE)" in local_dispatch
      and "UpdateCirclePrimitive(this)" in local_dispatch
      and "ProcessPrimitiveFrame()" in local_tick and "++m_stateCnt" in local_tick)
check("lifecycle fixture exercises the change-point frame, replacement values, integration and tick",
      "circleMaster.m_type = 8" in FIXTURE
      and "circle.m_changePoint = 2" in FIXTURE
      and "circle.m_changeSpeed = 0.08f" in FIXTURE
      and "circle.m_changeAccel = 0.0f" in FIXTURE
      and "circle.m_radiusSpeed == 0.08f && circle.m_radiusAccel == 0.0f" in FIXTURE
      and "std::fabs(circle.m_radius - 4.08f)" in FIXTURE
      and "OnProcess case 0x0C applies change-point radius speed" in FIXTURE)
print("PP_3DCIRCLE initializer comparator passed.")
