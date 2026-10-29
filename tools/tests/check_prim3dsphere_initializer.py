#!/usr/bin/env python3
"""Compare canonical PP_3DSPHERE initialization with the local tick and fixture."""
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


signature = "void __thiscall CEffectPrim::Prim3DSphere(CEffectPrim *this)"
pe = body(PE, signature, PE.index("//----- (00644A80)"))
ref = body(REF, signature)
pe_dispatch = body(PE, "unsigned __int8 __thiscall CEffectPrim::OnProcess(CEffectPrim *this)", PE.index("//----- (00640D30)"))
ref_dispatch = body(REF, "unsigned __int8 __thiscall CEffectPrim::OnProcess(CEffectPrim *this)")
local_dispatch = body(LOCAL, "bool CEffectPrim::ProcessPrimitiveFrame()")
local_helper = body(LOCAL, "void UpdateReferenceSphere(CEffectPrim* prim)")
local_tick = body(LOCAL, "u8 CEffectPrim::OnProcess()")

check("canonical PE and primary Ref Prim3DSphere bodies normalize identically", compact(pe) == compact(ref))
def case_body(dispatch: str) -> str:
    start = re.search(r"case\s+0xE\s*:", dispatch, re.IGNORECASE)
    assert start is not None, "missing primitive case 0x0E"
    end = re.search(r"case\s+0xF\s*:", dispatch[start.end():], re.IGNORECASE)
    assert end is not None, "missing next primitive case after 0x0E"
    return dispatch[start.start():start.end() + end.start()]


pe_case = case_body(pe_dispatch)
ref_case = case_body(ref_dispatch)
check("canonical PE and primary Ref inline dispatch cases for primitive 0x0E match",
      compact(pe_case) == compact(ref_case)
      and "this->m_pos = this->m_orgPos" in pe_case
      and "this->m_radius = v12 + this->m_radius" in pe_case
      and "this->m_longSpeed + this->m_longitude" in pe_case
      and "ProcessAlpha(this)" in pe_case)
check("local primitive route runs UpdateReferenceSphere and returns to the shared tick",
      "m_type == PP_3DSPHERE" in local_dispatch
      and "UpdateReferenceSphere(this)" in local_dispatch
      and "return true" in local_dispatch
      and "ProcessPrimitiveFrame()" in local_tick and "++m_stateCnt" in local_tick)
check("local sphere updater preserves source origin, radius speed, radius and longitude steps",
      "AddVec3(prim->m_master->EffectPosition(),prim->m_deltaPos2)" in compact(local_helper)
      and "static_cast<double>(prim->m_radiusAccel)+prim->m_radiusSpeed" in compact(local_helper)
      and "prim->m_radius = static_cast<float>(speed+prim->m_radius)" in compact(local_helper)
      and "prim->m_longitude += prim->m_longSpeed" in compact(local_helper)
      and "ProcessReferenceParticleAlpha(prim)" in local_helper)
check("existing lifecycle fixture checks origin, radius, longitude and state increment",
      "sphere.m_type = PP_3DSPHERE" in FIXTURE
      and "sphere.m_orgPos.x == 11" in FIXTURE and "sphere.m_orgPos.y == 22" in FIXTURE
      and "sphere.m_orgPos.z == 33" in FIXTURE
      and "sphere.m_radiusSpeed == 3 && sphere.m_radius == 7" in FIXTURE
      and "sphere.m_longitude == 12 && sphere.m_stateCnt == 1" in FIXTURE
      and "case 0x0E always executes source sphere origin" in FIXTURE)
print("PP_3DSPHERE initializer comparator passed.")
