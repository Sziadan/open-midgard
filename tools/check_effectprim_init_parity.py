#!/usr/bin/env python3
"""Mechanically compare CEffectPrim::Init with the canonical executable body."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
CURRENT = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
CURRENT_H_SOURCE = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
EXE_H = (ROOT / "HighPriest.exe.h").read_text(encoding="utf-8", errors="replace")
CURRENT_H = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
REF = (ROOT / "Ref/RagEffectPrim.cpp").read_text(encoding="utf-8", errors="replace")


def body(text: str, marker: str, end_marker: str) -> str:
    start = text.index(marker)
    opening = text.index("{", start)
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[opening + 1:pos]
    raise AssertionError(f"unclosed body: {marker} before {end_marker}")


exe_body = body(EXE, "void __thiscall CEffectPrim::Init(CEffectPrim *this, CRagEffect *master, EFFECTPRIMID effectPrimId, vector3d fDeltaPos)\n{", "//----- (00649220)")
current_body = body(CURRENT, "void CEffectPrim::Init(CRagEffect* master, EFFECTPRIMID effectPrimId, const vector3d& deltaPos)\n{", "static_assert(offsetof(EffectGi, height)")

checks = 0


def check(label: str, condition: bool) -> None:
    global checks
    assert condition, f"FAIL {label}"
    checks += 1
    print(f"PASS {label}")


def enum_values(text: str) -> list[tuple[str, int]]:
    start = text.index("enum EFFECTPRIMID")
    end = text.index("};", start)
    return [(name, int(value, 16)) for name, value in re.findall(r"\b(PP_[A-Z0-9_]+)\s*=\s*0x([0-9A-Fa-f]+)", text[start:end])]


check("canonical current primitive enum exactly matches EXE names and numeric values",
      enum_values(CURRENT_H) == enum_values(EXE_H)
      and len(enum_values(EXE_H)) == 129
      and len({value for _, value in enum_values(EXE_H)}) == 129)
check("Init has zero branches in both source and implementation",
      not re.search(r"\b(if|else|switch|for|while|case)\b", exe_body)
      and not re.search(r"\b(if|else|switch|for|while|case)\b", current_body))
check("master assignment occurs first",
      re.search(r"this->m_master\s*=\s*master;", exe_body) is not None
      and re.search(r"m_master\s*=\s*master;", current_body) is not None)
check("existing delta is incremented componentwise before type assignment",
      all(f"this->m_deltaPos.{axis} = fDeltaPos.{axis} + this->m_deltaPos.{axis};" in exe_body for axis in "xyz")
      and "m_deltaPos = AddVec3(m_deltaPos, deltaPos);" in current_body
      and exe_body.index("this->m_deltaPos.z =") < exe_body.index("this->m_type =")
      and current_body.index("m_deltaPos =") < current_body.index("m_type ="))
check("effect type is assigned after delta accumulation",
      "this->m_type = effectPrimId;" in exe_body and "m_type = effectPrimId;" in current_body)
check("origin is copied from current canonical master-position abstraction",
      all(f"master->m_pos.{axis}" in exe_body for axis in "xyz")
      and "m_orgPos = master->EffectPosition();" in current_body
      and "const vector3d& EffectPosition() const { return m_effectPos; }" in CURRENT_H_SOURCE
      and "m_effectPos = m_cachedPos;" in CURRENT[CURRENT.index("void CRagEffect::Init("):CURRENT.index("void CRagEffect::InitAtWorldPosition(")])
check("origin is copied into primitive position",
      "x = this->m_orgPos.x;" in exe_body and "this->m_pos.x = x;" in exe_body
      and all(f"this->m_pos.{axis} = this->m_orgPos.{axis};" in exe_body for axis in "yz")
      and "m_pos = m_orgPos;" in current_body)
check("current Init writes no fields outside the five source write groups",
      set(re.findall(r"\b(m_[A-Za-z0-9_]+)\s*=", current_body)) == {"m_master", "m_deltaPos", "m_type", "m_orgPos", "m_pos"})
check("misleading Ref Init annotation is identified as InitSegment body",
      "Original Signature: public: void __thiscall CEffectPrim::Init(class CRagEffect *,enum EFFECTPRIMID,struct vector3d)" in REF
      and "void __thiscall CEffectPrim::InitSegment(CEffectPrim *this)" in REF)

print(f"{checks} CEffectPrim::Init source-comparison assertions passed")
