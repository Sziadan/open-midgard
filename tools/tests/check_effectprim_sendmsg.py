#!/usr/bin/env python3
"""Compare CEffectPrim::SendMsg against Ref and the canonical EXE."""
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

signature = "void __thiscall CEffectPrim::SendMsg(CEffectPrim *this, CGameObject *sender, int msgid, int val1, int val2, int val3)"
ref_body = function_body(ref, signature)
exe_body = function_body(exe, signature)
assert compact(ref_body) == compact(exe_body), "Ref SendMsg differs from canonical executable"
assert "(msgid==24)this->m_master=0;" in compact(exe_body)

current_body = function_body(current, "void CEffectPrim::SendMsg(CGameObject*, int msg, msgparam_t, msgparam_t, msgparam_t)")
assert "if(msg==24){m_master=nullptr;}" in compact(current_body)
assert "msg==24" in compact(current_body) and "m_master=nullptr" in compact(current_body)
current_effects = current_body[current_body.index("{") + 1:-1]
assert "sender" not in current_effects and "msgparam_t" not in current_effects

print("CEffectPrim::SendMsg comparison passed: EXE/Ref equality and the message-24 master-clear branch")
