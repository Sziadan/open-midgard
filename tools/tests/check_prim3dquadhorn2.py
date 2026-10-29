#!/usr/bin/env python3
"""Compare canonical PP_3DQUADHORN2 initialization with the local tick and fixture."""
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


signature = "void __thiscall CEffectPrim::Prim3DQuadHorn2(CEffectPrim *this)"
pe = body(PE, signature, PE.index("//----- (006430A0)"))
ref = body(REF, signature)
readable_ref = body(REF_DISPATCH, "void\tCEffectPrim :: Prim3DQuadHorn2()")
local = body(LOCAL, "void UpdateReferenceQuadHorn2(CEffectPrim* prim)")
local_dispatch = body(LOCAL, "bool CEffectPrim::ProcessPrimitiveFrame()")
pe_dispatch = body(PE, "unsigned __int8 __thiscall CEffectPrim::OnProcess(CEffectPrim *this)", PE.index("//----- (00640D30)"))
ref_dispatch = body(REF_DISPATCH, "bool\tCEffectPrim :: OnProcess()")
fixture_marker = "void RunQuadHorn2LifecycleChecks(CEffectPrim& prim, int& checks, int& failures)"
fixture = body(FIXTURE, fixture_marker, FIXTURE.rindex(fixture_marker))

check("canonical PE and primary Ref Prim3DQuadHorn2 bodies normalize identically", compact(pe) == compact(ref))
check("readable Ref preserves the 550 threshold and modular speed branches",
      re.search(r"m_stateCnt\s*>=\s*550", readable_ref) is not None
      and re.search(r"m_stateCnt\s*%\s*6\s*==\s*2", readable_ref) is not None
      and re.search(r"m_stateCnt\s*%\s*6\s*==\s*5", readable_ref) is not None
      and "1.18f" in readable_ref and "0.01f" in readable_ref
      and re.search(r"m_stateCnt\s*==\s*550", readable_ref) is not None)
check("PE and Ref dispatch route primitive 0x75 to Prim3DQuadHorn2",
      re.search(r"case\s+0x75\s*:[\s\S]{0,300}Prim3DQuadHorn2\(this\)", pe_dispatch) is not None
      and re.search(r"case\s+PP_3DQUADHORN2\s*:[\s\S]{0,100}Prim3DQuadHorn2\(\)", ref_dispatch) is not None)
check("local case 0x75 uses the source tick helper and continues shared processing",
      "m_type == PP_3DQUADHORN2" in local_dispatch
      and "UpdateReferenceQuadHorn2(this)" in local_dispatch
      and "return true" in local_dispatch)
check("local helper retains origin, exact threshold branches, vector integration and alpha processing",
      "AddVec3(prim->m_master->EffectPosition(), prim->m_deltaPos2)" in compact(local)
      and "prim->m_stateCnt >= 550" in compact(local)
      and "prim->m_stateCnt % 6 == 2" in compact(local)
      and "prim->m_stateCnt % 6 == 5" in compact(local)
      and "1.18f" in local and "0.01f" in local
      and "prim->m_matrix.m[2][0]" in local and "prim->m_matrix.m[3][2]" in local
      and "prim->m_deltaPos = AddVec3(prim->m_deltaPos, prim->m_speed3d)" in compact(local)
      and "prim->m_pos = AddVec3(prim->m_orgPos, prim->m_deltaPos)" in compact(local)
      and "ProcessReferenceParticleAlpha(prim)" in local)
check("focused fixture exercises states 2, 5, 549, 550 and 551 with motion/alpha checks",
      "prim.m_stateCnt=2" in fixture and "prim.m_stateCnt=5" in fixture
      and "prim.m_stateCnt=549" in fixture and "prim.m_stateCnt=550" in fixture
      and "prim.m_stateCnt=551" in fixture
      and "state-two change values feed translated matrix motion" in fixture
      and "state-five alternate speed branch" in fixture
      and "state 550 loads change values exactly" in fixture
      and "states above 550 retain speed and acceleration" in fixture)
print("PP_3DQUADHORN2 comparator passed.")
