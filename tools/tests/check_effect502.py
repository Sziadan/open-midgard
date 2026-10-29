#!/usr/bin/env python3
"""Compare the inline effect502 Suffragium branch and wave against PE/Ref."""
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
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[start:pos + 1]
    raise AssertionError(signature)


def compact(source: str) -> str:
    return re.sub(r"\s+", "", source)


pe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
ref = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
local = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
dispatch = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")

sig = "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)"
pe_fn = function(pe, sig)
ref_fn = function(ref, sig)
pe_start, ref_start = pe_fn.index("case 0x1F6:"), ref_fn.index("case 0x1F6:")
pe_case, ref_case = pe_fn[pe_start:pe_fn.index("case 0x1F7:", pe_start)], ref_fn[ref_start:ref_fn.index("case 0x1F7:", ref_start)]
require(compact(pe_case) == compact(ref_case), "PE and Ref effect502 inline case differ")
for token in (
    "if(!this->m_stateCnt)", "this->m_pos=this->m_master->m_pos",
    "PlayWave(aEffectEfBegins", "0x8Cu", "m_BodyLight&8", "m_BodyLight+=8",
    "m_stateCnt%2==0", "gotoLABEL_306",
): require(token in compact(pe_case), f"effect502 PE branch token missing: {token}")
require("case 502: return ProcessEffect502();" in dispatch, "effect502 route missing")
require('502: "return ProcessEffect502();"' in (ROOT / "tools/generate_effect_dispatch.py").read_text(encoding="utf-8"),
        "effect502 generator route missing")

labels = pe_fn[pe_fn.index("LABEL_306:"):]
for token in ("if(!v74)gotoLABEL_206", "m_BodyLight&1", "++v71->m_BodyLight",
              "SetArgb(this->m_master,-1,250,250,100)", "SetArgb(this->m_master,-1,255,255,255)"):
    require(token in compact(labels), f"effect502 shared flash branch changed: {token}")
body = function(local, "bool CRagEffect::ProcessEffect502()")
ordered = (
    "if(m_stateCnt==0&&master!=nullptr)", "m_effectPos=master->m_pos",
    'PlayWave("effect\\\\EF_BeginSpell.wav"', "if(m_stateCnt>140||master==nullptr)returntrue",
    "master->m_BodyLight&8", "master->m_BodyLight+=8", "(m_stateCnt%2)==0",
    "master->m_BodyLight&1", "++master->m_BodyLight", "SetArgb(-1,250,250,100)",
    "SetArgb(-1,255,255,255)", "returntrue",
)
cursor = 0
for token in ordered:
    pos = compact(body).find(token, cursor)
    require(pos >= 0, f"effect502 local branch missing or reordered: {token}")
    cursor = pos + len(token)

asset = ExeImage().read_cstring_at(0x7193CC)
require(asset is not None and asset.text == "effect\\EF_BeginSpell.wav", "PE effect502 wave bytes mismatch")
print("effect502 PE/Ref inline branch, route, flash gates, and wave bytes passed")
