#!/usr/bin/env python3
"""Compare inline effect505 Suffragium caller, flash window, and exact wave."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage  # noqa: E402


def require(ok: bool, message: str) -> None:
    if not ok:
        raise SystemExit(message)


def function(source: str, signature: str) -> str:
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{": depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0: return source[start:pos + 1]
    raise AssertionError(signature)


def compact(source: str) -> str:
    return re.sub(r"\s+", "", source)


pe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
ref = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
src = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
dispatch = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")
sig = "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)"
pe_fn, ref_fn = function(pe, sig), function(ref, sig)
pe_start, ref_start = pe_fn.index("case 0x1F9:"), ref_fn.index("case 0x1F9:")
pe_case = pe_fn[pe_start:pe_fn.index("case 0x1FA:", pe_start)]
ref_case = ref_fn[ref_start:ref_fn.index("case 0x1FA:", ref_start)]
require(compact(pe_case) == compact(ref_case), "PE and Ref effect505 inline caller branches differ")
for token in ("if(!this->m_stateCnt)", "PlayWave(aEffectPriestSu", "v70<10||v70>=36",
              "(v70&1)==0", "gotoLABEL_268", "gotoLABEL_206"):
    require(token in compact(pe_case), f"effect505 PE case token missing: {token}")

require("case 505: return ProcessEffect505();" in dispatch, "effect505 route missing")
require('505: "return ProcessEffect505();"' in (ROOT / "tools/generate_effect_dispatch.py").read_text(encoding="utf-8"),
        "effect505 generator route missing")
labels = pe_fn[pe_fn.index("LABEL_268:"):]
for token in ("m_BodyLight&1", "++v71->m_BodyLight", "SetArgb(this->m_master,-1,250,250,100)",
              "SetArgb(this->m_master,-1,255,255,255)"):
    require(token in compact(labels), f"effect505 shared flash label mismatch: {token}")

body = compact(function(src, "bool CRagEffect::ProcessEffect505()"))
ordered = (
    "if(m_stateCnt==0)", 'PlayWave("effect\\\\priest_suffragium.wav"',
    "if(m_stateCnt<10||m_stateCnt>=36)returntrue", "(m_stateCnt&1)==0",
    "master->m_BodyLight&1", "++master->m_BodyLight", "SetArgb(-1,250,250,100)",
    "SetArgb(-1,255,255,255)", "returntrue",
)
cursor = 0
for token in ordered:
    pos = body.find(token, cursor)
    require(pos >= 0, f"effect505 local behavior missing or reordered: {token}")
    cursor = pos + len(token)

asset = ExeImage().read_cstring_at(0x719760)
require(asset is not None and asset.text == "effect\\priest_suffragium.wav", "PE effect505 wave mismatch")
print("effect505 PE/Ref inline branch, route, tint parity, and exact wave bytes passed")
