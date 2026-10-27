#!/usr/bin/env python3
"""Branch-level comparison for the AnimTexture behavior in Prim3DAura."""
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


ref_readable = (ROOT / "Ref/dhxj/RagEffect2.cpp").read_text(encoding="cp1252", errors="replace")
ref_effect = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
ref_prim = (ROOT / "Ref/RagEffectPrim.cpp").read_text(encoding="utf-8")
exe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
current = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")

signature = "bool __thiscall CEffectPrim::Prim3DAura(CEffectPrim *this)"
ref_body = body(ref_effect, signature)
exe_body = body(exe, signature)
assert compact(ref_body) == compact(exe_body), "Ref Prim3DAura differs from canonical executable"
readable = body(ref_readable, "CEffectPrim :: Prim3DAura()")
current_body = body(current, "bool UpdateReference3DAura(CEffectPrim* prim)")
source = compact(exe_body)
implementation = compact(current_body)
anim_texture_signature = "unsigned __int8 __thiscall CEffectPrim::AnimTexture(CEffectPrim *this)"
ref_anim_texture = body(ref_prim, anim_texture_signature)
exe_anim_texture = body(exe, anim_texture_signature)
assert compact(ref_anim_texture) == compact(exe_anim_texture), "Ref AnimTexture differs from canonical executable"
anim_texture_source = compact(exe_anim_texture)

for token in (
    "this->m_pos=this->m_master->m_pos",
    "v1=2", "p_rise_angle=&this->m_GI[0].rise_angle", "v3=*p_rise_angle+3",
    "*p_rise_angle=v3", "p_rise_angle+=90", "--v1", "while(v1)",
    "returnCEffectPrim::AnimTexture(this)!=0",
):
    assert token in source, f"canonical AnimTexture caller branch is missing: {token}"
for token in (
    "if(!(this->m_stateCnt%this->m_animSpeed))", "v2=this->m_curMotion+1", "this->m_curMotion=v2",
    "if(m_repeatAnim)", "this->m_curMotion=v2%this->m_totalTexture",
    "elseif(v2==this->m_totalTexture)", "return0", "return1",
):
    assert token in anim_texture_source, f"canonical AnimTexture helper branch is missing: {token}"
for token in (
    "if(!AnimTexture())returnfalse", "returntrue",
):
    assert compact(token) in compact(readable), f"readable AnimTexture caller branch is missing: {token}"
for token in (
    "prim->m_pos=prim->m_master->EffectPosition()",
    "for(intindex=0;index<2;++index)",
    "prim->m_gi[index].riseAngle+3",
    "if(riseAngle>=360)",
    "prim->m_stateCnt%prim->m_animSpeed==0",
    "constintmotion=++prim->m_curMotion",
    "if(prim->m_repeatAnim)",
    "prim->m_curMotion=motion%prim->m_totalTexture",
    "elseif(motion==prim->m_totalTexture)",
    "returnfalse", "returntrue",
):
    assert token in implementation, f"current AnimTexture behavior is missing: {token}"

for source_order, label in ((source, "canonical"), (implementation, "current")):
    if label == "canonical":
        order = (
            "v1=2", "this->m_pos=this->m_master->m_pos", "p_rise_angle=&this->m_GI[0].rise_angle",
            "v3=*p_rise_angle+3", "*p_rise_angle=v3", "p_rise_angle+=90", "--v1", "while(v1)",
            "returnCEffectPrim::AnimTexture(this)!=0",
        )
    else:
        order = (
            "prim->m_pos=prim->m_master->EffectPosition()", "for(intindex=0;index<2;++index)",
            "prim->m_gi[index].riseAngle+3", "if(prim->m_stateCnt%prim->m_animSpeed==0)",
            "constintmotion=++prim->m_curMotion", "if(prim->m_repeatAnim)",
            "prim->m_curMotion=motion%prim->m_totalTexture", "elseif(motion==prim->m_totalTexture)",
            "returnfalse", "returntrue",
        )
    cursor = -1
    for token in order:
        cursor = source_order.find(token, cursor + 1)
        assert cursor >= 0, f"{label} AnimTexture branch order differs: {token}"

# Compare current inlined behavior against the exact standalone source helper.
for token in (
    "if(prim->m_stateCnt%prim->m_animSpeed==0)", "constintmotion=++prim->m_curMotion",
    "if(prim->m_repeatAnim)", "prim->m_curMotion=motion%prim->m_totalTexture",
    "elseif(motion==prim->m_totalTexture)", "returnfalse",
):
    assert token in implementation, f"current inlined AnimTexture helper effect is missing: {token}"
print("AnimTexture comparison passed: EXE/Ref Prim3DAura equality and ordered cadence, repeat modulo and completion branches")
