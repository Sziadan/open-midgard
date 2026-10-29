#!/usr/bin/env python3
"""Compare canonical PP_3DLINE initialization and shared-tick behavior."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffectPrim.cpp").read_text(encoding="utf-8", errors="replace")
REF_DISPATCH = (ROOT / "Ref/dhxj/RagEffectPrim.cpp").read_text(encoding="cp1252", errors="replace")
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


signature = "void __thiscall CEffectPrim::Prim3DLine(CEffectPrim *this)"
pe = body(PE, signature, PE.index("//----- (00644DC0)"))
ref = body(REF, signature)
readable_ref = body(REF_DISPATCH, "void\tCEffectPrim :: Prim3DLine()")
pe_dispatch = body(PE, "unsigned __int8 __thiscall CEffectPrim::OnProcess(CEffectPrim *this)", PE.index("//----- (00640D30)"))
ref_dispatch = body(REF, "unsigned __int8 __thiscall CEffectPrim::OnProcess(CEffectPrim *this)")
local_dispatch = body(LOCAL, "bool CEffectPrim::ProcessPrimitiveFrame()")
local_tick = body(LOCAL, "u8 CEffectPrim::OnProcess()")

check("canonical PE and primary Ref Prim3DLine bodies normalize identically", compact(pe) == compact(ref))
check("readable Ref retains the five-unit vertical increment", "m_pos.y += 5.0f" in readable_ref)
check("PE and Ref dispatch primitive 0x14 with the source vertical increment",
      re.search(r"case\s+0x14\s*:[\s\S]{0,120}m_pos\.y\s*=\s*this->m_pos\.y\s*\+\s*5\.0", pe_dispatch) is not None
      and re.search(r"case\s+0x14\s*:[\s\S]{0,120}m_pos\.y\s*=\s*this->m_pos\.y\s*\+\s*5\.0", ref_dispatch) is not None)
check("local primitive route adds five to world Y and returns to the shared tick",
      re.search(r"m_type\s*==\s*PP_3DLINE\s*\)\s*\{\s*m_pos\.y\s*\+=\s*5\.0f;\s*return\s+true", local_dispatch) is not None
      and "ProcessPrimitiveFrame()" in local_tick and "++m_stateCnt" in local_tick)
check("existing lifecycle fixture covers unchanged X/Z and state increment after the Y step",
      "prim.m_type = PP_3DLINE" in FIXTURE
      and "prim.m_pos.x == 2 && prim.m_pos.y == 8 && prim.m_pos.z == 4" in FIXTURE
      and "prim.m_stateCnt == 1" in FIXTURE
      and "case 0x14 adds five to world Y before shared lifetime increment" in FIXTURE)
print("PP_3DLINE initializer comparator passed.")
