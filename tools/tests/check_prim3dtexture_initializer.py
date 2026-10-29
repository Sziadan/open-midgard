#!/usr/bin/env python3
"""Compare canonical PP_3DTEXTURE initialization and source-pattern fixtures."""
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


signature = "bool __thiscall CEffectPrim::Prim3DTexture(CEffectPrim *this)"
pe = body(PE, signature, PE.index("//----- (00643340)"))
ref = body(REF, signature)
pe_dispatch = body(PE, "unsigned __int8 __thiscall CEffectPrim::OnProcess(CEffectPrim *this)", PE.index("//----- (00640D30)"))
ref_dispatch = body(REF, "unsigned __int8 __thiscall CEffectPrim::OnProcess(CEffectPrim *this)")
local_dispatch = body(LOCAL, "bool CEffectPrim::ProcessPrimitiveFrame()")
local = body(LOCAL, "bool UpdateReference3DTexture(CEffectPrim* prim)")
local_tick = body(LOCAL, "u8 CEffectPrim::OnProcess()")

check("canonical PE and primary Ref Prim3DTexture bodies normalize identically", compact(pe) == compact(ref))
check("PE and Ref dispatch ids 0x0F/0x10 to the common texture handler",
      re.search(r"case\s+0xF\s*:[\s\S]{0,180}case\s+0x10\s*:[\s\S]{0,180}Prim3DTexture\(this\)", pe_dispatch) is not None
      and re.search(r"case\s+0xF\s*:[\s\S]{0,180}case\s+0x10\s*:[\s\S]{0,180}Prim3DTexture\(this\)", ref_dispatch) is not None)
check("local PP_3DTEXTURE and PP_3DCROSSTEXTURE route to the same source updater and shared tick",
      "m_type == PP_3DTEXTURE || m_type == PP_3DCROSSTEXTURE" in local_dispatch
      and "UpdateReference3DTexture(this)" in local_dispatch
      and "ProcessPrimitiveFrame()" in local_tick and "++m_stateCnt" in local_tick)
check("local updater retains change-point sentinel controls and source movement/size/alpha order",
      "prim->m_changePoint == prim->m_stateCnt" in compact(local)
      and "prim->m_changeSpeed3d.x != sentinel" in local
      and compact(local).index("prim->m_speed3d = AddVec3(prim->m_speed3d, prim->m_deltaPosAccel)")
      < compact(local).index("prim->m_widthSpeed += prim->m_widthAccel")
      < compact(local).index("prim->m_deltaPos = AddVec3(prim->m_deltaPos, prim->m_speed3d)")
      < compact(local).index("prim->m_pos = AddVec3(prim->m_orgPos, prim->m_deltaPos)")
      < compact(local).index("ProcessReferenceParticleAlpha(prim)"))
check("local updater retains source 0x100/0x400 motion controls, other pattern branches, and animation gate",
      "(pattern & 0x400)" in local and "(pattern & 0x100)" in local
      and "(pattern & 0x200)" in local and "(pattern & 0x40)" in local
      and "(pattern & 2)" in local and "(pattern & 0x10)" in local
      and "(pattern & 4)" in local and "(pattern & 0x20)" in local
      and "(pattern & 0x800)" in local
      and "prim->m_stateCnt % prim->m_animSpeed" in local
      and "if (prim->m_repeatAnim)" in local
      and "return motion != prim->m_totalTexture" in local)
check("existing lifecycle fixture covers change controls, 0x100 velocity, 0x400 collision, patterns 2/4/10/20, RGB cycle and clamp branches",
      "case 0x0F applies change-point controls, pattern-0x100 velocity" in FIXTURE
      and "case 0x10 preserves the source 0x400 collision early return" in FIXTURE
      and "Prim3DTexture pattern 0x400 rebuilds translated matrix velocity" in FIXTURE
      and "Prim3DTexture pattern 2 wraps V animation, pattern 0x10 absorbs parent delta, and pattern 0x20 reverses height" in FIXTURE
      and "Prim3DTexture pattern 4 stops above the source terrain height" in FIXTURE
      and "Prim3DTexture RGB-cycle delay preserves source 254 wrap" in FIXTURE
      and "Prim3DTexture regular RGB branch preserves its three asymmetric clamps" in FIXTURE)
print("PP_3DTEXTURE initializer comparator passed.")
