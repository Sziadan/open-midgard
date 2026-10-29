#!/usr/bin/env python3
"""Compare canonical PP_2DCIRCLE initialization with the local tick and fixture."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffectPrim.cpp").read_text(encoding="utf-8", errors="replace")
REF_READABLE = (ROOT / "Ref/dhxj/RagEffectPrim.cpp").read_text(encoding="cp1252", errors="replace")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
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
    start = re.search(r"case\s+(?:0x)?4\s*:", dispatch, re.IGNORECASE)
    assert start is not None, "missing primitive case 0x04"
    end = re.search(r"case\s+(?:0x)?5\s*:", dispatch[start.end():], re.IGNORECASE)
    assert end is not None, "missing next primitive case after 0x04"
    return dispatch[start.start():start.end() + end.start()]


signature = "void __thiscall CEffectPrim::Prim2DCircle(CEffectPrim *this)"
pe = body(PE, signature, PE.index("//----- (00641D70)"))
ref = body(REF, signature)
readable_ref = body(REF_READABLE, "void\tCEffectPrim :: Prim2DCircle()")
pe_dispatch = body(PE, "unsigned __int8 __thiscall CEffectPrim::OnProcess(CEffectPrim *this)", PE.index("//----- (00640D30)"))
ref_dispatch = body(REF, "unsigned __int8 __thiscall CEffectPrim::OnProcess(CEffectPrim *this)")
local_dispatch = body(LOCAL, "bool CEffectPrim::ProcessPrimitiveFrame()")
local_helper = body(LOCAL, "void UpdateReferenceScreenCircle(CEffectPrim* prim)")
local_tick = body(LOCAL, "u8 CEffectPrim::OnProcess()")
pe_case, ref_case = case_body(pe_dispatch), case_body(ref_dispatch)

check("canonical PE and primary Ref Prim2DCircle bodies normalize identically", compact(pe) == compact(ref))
check("readable Ref expresses fill-pattern and inner-radius branches equivalently",
      "PT_FILLCIRCLE" in readable_ref and "m_radius < m_innerSize" in readable_ref
      and "m_size = m_radius" in readable_ref and "m_size = m_innerSize" in readable_ref)
check("canonical PE and primary Ref inline dispatcher branches for primitive 0x04 match",
      compact(pe_case) == compact(ref_case)
      and "this->m_size = this->m_innerSize" in pe_case
      and "this->m_size = v7" in pe_case
      and "ProcessAlpha(this)" in pe_case)
check("local PP_2DCIRCLE route uses screen origin, radius integration, fill/clamp and alpha",
      "m_type == PP_2DCIRCLE" in LOCAL
      and "UsesReferenceScreenCircle()" in local_dispatch
      and "UpdateReferenceScreenCircle(this)" in local_dispatch
      and "return true" in local_dispatch
      and "ScreenPosition()" in local_helper
      and "prim->m_pattern & 1" in local_helper
      and "radius >= prim->m_innerSize" in local_helper
      and "ProcessReferenceParticleAlpha(prim)" in local_helper
      and "ProcessPrimitiveFrame()" in local_tick and "++m_stateCnt" in local_tick
      and "vector3d ScreenPosition() const { return {m_tlvertX,m_tlvertY,m_tlvertZ}; }" in HEADER)
check("fixture checks source inner clamp, fill-pattern bypass, below-inner behavior and shared tick",
      "OnProcess case 0x04 always executes source circle anchor" in FIXTURE
      and "fill pattern keeps full radius even above inner size" in FIXTURE
      and "radius below inner size stays at radius" in FIXTURE)
print("PP_2DCIRCLE initializer comparator passed.")
