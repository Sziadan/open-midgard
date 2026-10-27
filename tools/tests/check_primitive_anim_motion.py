#!/usr/bin/env python3
"""Branch-level comparison for CEffectPrim::AnimMotion."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def body(text: str, signature: str) -> str:
    start = text.rindex(signature)
    opening = text.index("{", start)
    depth = 1
    cursor = opening + 1
    while depth:
        depth += (text[cursor] == "{") - (text[cursor] == "}")
        cursor += 1
    return text[start:cursor]


def compact(text: str) -> str:
    return re.sub(r"\s+", "", text)


ref = (ROOT / "Ref/RagEffectPrim.cpp").read_text(encoding="utf-8")
exe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
current = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")

signature = "unsigned __int8 __thiscall CEffectPrim::AnimMotion(CEffectPrim *this)"
ref_body = body(ref, signature)
exe_body = body(exe, signature)
assert compact(ref_body) == compact(exe_body), "Ref AnimMotion differs from canonical executable"
source = compact(exe_body)
for token in (
    "ActRes=CRenderObject::GetActRes(this)",
    "if(!(this->m_stateCnt%this->m_animSpeed))",
    "v4=this->m_curMotion+1", "this->m_curMotion=v4", "if(m_repeatAnim)",
    "this->m_curMotion%=CActRes::GetMotionCount(ActRes,this->m_curAction)",
    "else if((this->m_pattern&0x800)!=0)",
    "if(v4==CActRes::GetMotionCount(ActRes,this->m_curAction))",
    "this->m_curMotion=v5-1", "else if(this->m_curMotion==CActRes::GetMotionCount(ActRes,this->m_curAction))",
    "return 0", "return 1",
):
    assert compact(token) in source, f"canonical AnimMotion effect is missing: {token}"

current_body = body(current, "bool AnimateReferenceMotion(CEffectPrim* prim)")
implementation = compact(current_body)
for token in (
    "if(!prim->m_particleActRes)returntrue", "if(prim->m_stateCnt%prim->m_animSpeed!=0)returntrue",
    "++prim->m_curMotion", "constintmotionCount=prim->m_particleActRes->GetMotionCount(prim->m_spriteAction)",
    "if(prim->m_repeatAnim)", "prim->m_curMotion%=motionCount",
    "elseif((prim->m_pattern&0x800)!=0)", "if(prim->m_curMotion==motionCount)--prim->m_curMotion",
    "else if (prim->m_curMotion == motionCount)", "return false;", "return true;",
):
    assert compact(token) in implementation, f"current AnimMotion effect is missing: {token}"

ordered = (
    "if(!prim->m_particleActRes)returntrue",
    "if(prim->m_stateCnt%prim->m_animSpeed!=0)returntrue",
    "++prim->m_curMotion", "constintmotionCount=",
    "if(prim->m_repeatAnim)", "prim->m_curMotion%=motionCount",
    "elseif((prim->m_pattern&0x800)!=0)", "if(prim->m_curMotion==motionCount)--prim->m_curMotion",
    "else if (prim->m_curMotion == motionCount)", "return false;", "return true;",
)
cursor = -1
for token in ordered:
    cursor = implementation.find(compact(token), cursor + 1)
    assert cursor >= 0, f"current AnimMotion branch order differs: {token}"

# These are intentional safety guards outside the valid-resource contract.
assert "if (!prim->m_particleActRes) return true;" in current_body
print("AnimMotion comparison passed: EXE/Ref equality and ordered cadence, repeat, stop-at-last and completion branches")
