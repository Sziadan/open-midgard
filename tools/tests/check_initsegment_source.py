#!/usr/bin/env python3
"""Branch-level source comparison for CEffectPrim::InitSegment."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def function_body(source: str, signature: str) -> str:
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 1
    cursor = opening + 1
    while depth:
        depth += (source[cursor] == "{") - (source[cursor] == "}")
        cursor += 1
    return source[start:cursor]


def compact(source: str) -> str:
    return re.sub(r"\s+", "", source)


ref = (ROOT / "Ref/RagEffectPrim.cpp").read_text(encoding="utf-8")
exe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
current = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")

signature = "void __thiscall CEffectPrim::InitSegment(CEffectPrim *this)"
ref_body = function_body(ref, signature)
exe_body = function_body(exe, signature)
assert compact(ref_body) == compact(exe_body), "Ref InitSegment differs from canonical executable"
source = compact(exe_body)
for token in (
    "if(this->m_numSegment>0)",
    "*(vector3d*)(p_z-2)=this->m_pos",
    "p_z[1]=this->m_segPos[0].x", "p_z[2]=this->m_segPos[0].y", "p_z[3]=this->m_segPos[0].z",
    "*(vector3d*)(p_z+4)=this->m_segPos[1]", "*(vector3d*)(p_z+7)=this->m_segPos[2]",
    "*(vector3d*)(p_z+10)=this->m_segPos[3]", "p_z[13]=this->m_radius", "p_z[14]=this->m_size",
    "p_z[15]=this->m_longitude", "qmemcpy(p_z+16,&this->m_matrix,0x30u)",
    "p_z[31]=this->m_alpha", "p_z[28]=*(float*)&this->m_red", "p_z[29]=*(float*)&this->m_green",
    "p_z[30]=*(float*)&this->m_blue", "++i>=this->m_numSegment",
):
    assert token in source, f"canonical InitSegment source effect is missing: {token}"

current_body = function_body(current, "void InitReferenceSegments(CEffectPrim* prim)")
implementation = compact(current_body)
for token in (
    "constintcount=(std::min)(prim->m_numSegments,static_cast<int>(prim->m_segments.size()))",
    "for(inti=0;i<count;++i)", "segment.pos=prim->m_pos",
    "prim->m_segmentQuadPositions[static_cast<size_t>(i)]=prim->m_segPos",
    "segment.radius=prim->m_radius", "segment.size=prim->m_size", "segment.longitude=prim->m_longitude",
    "segment.transform=prim->m_matrix", "segment.alpha=prim->m_alpha", "segment.argb=PackColor(",
):
    assert token in implementation, f"current InitSegment effect is missing: {token}"

ordered = (
    "segment.pos=prim->m_pos",
    "prim->m_segmentQuadPositions[static_cast<size_t>(i)]=prim->m_segPos",
    "segment.radius=prim->m_radius", "segment.size=prim->m_size", "segment.longitude=prim->m_longitude",
    "segment.transform=prim->m_matrix", "segment.alpha=prim->m_alpha", "segment.argb=PackColor(",
)
cursor = -1
for token in ordered:
    cursor = implementation.find(token, cursor + 1)
    assert cursor >= 0, f"current InitSegment payload order differs from source mapping: {token}"

print("InitSegment comparison passed: EXE/Ref equality and per-slot position, four quad points, dimensions, color, alpha and matrix copy")
