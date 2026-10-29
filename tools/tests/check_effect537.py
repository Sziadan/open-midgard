#!/usr/bin/env python3
"""Compare effect537 BeginCasting2 route, helper, resources, and quake receiver."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage, SymbolTable  # noqa: E402


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
            if depth == 0:
                return source[start:pos + 1]
    raise AssertionError(signature)


def compact(source: str) -> str:
    return re.sub(r"\s+", "", source)


pe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
ref = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
ref2 = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
mode_ref = (ROOT / "Ref/GameMode2.cpp").read_text(encoding="utf-8", errors="replace")
mode_local = (ROOT / "src/gamemode/GameMode.cpp").read_text(encoding="utf-8", errors="replace")
src = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
dispatch = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")
generator = (ROOT / "tools/generate_effect_dispatch.py").read_text(encoding="utf-8")

pe_on = function(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
ref_on = function(ref, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
branches = []
for body, label in ((pe_on, "PE"), (ref_on, "Ref")):
    start = body.index("case 0x219:")
    end = body.index("case 0x21A:", start)
    branch = compact(body[start:end])
    require("m_stateCnt==5" in branch and "SendMsg(v54,135,0,0,0)" in branch
            and "CRagEffect::BeginCasting2(this,aEffectRingBlac,33)" in branch and "return1;" in branch,
            f"{label} effect537 caller branch mismatch")
    branches.append(branch)
require(branches[0] == branches[1], "PE and Ref effect537 caller branches differ")

sig = "void __thiscall CRagEffect::BeginCasting2(CRagEffect *this, char *tName, char option)"
pe_helper = function(pe, sig)
ref_helper = function(ref2, sig)
require(compact(pe_helper) == compact(ref_helper), "PE and Ref BeginCasting2 helper bodies differ")
for token in ("if(!this->m_stateCnt)", "PlayWave(aEffectEfFirewa,x,0.0,z,250,40,1.0)",
              "this->m_pos.x=v5->x", "this->m_pos.y=v5->y", "this->m_pos.z=v5->z",
              "PP_ACIDDEMON", "m_renderFlag=5", "m_size=13.0", "m_GI[0].max_height=25.0",
              "m_GI[1].max_height=22.0", "m_GI[2].max_height=19.0", "m_GI[3].max_height=30.0",
              "m_GI[3].life=1"):
    require(token in compact(pe_helper), f"BeginCasting2 PE/Ref behavior missing: {token}")

symbols = SymbolTable(ExeImage())
for name, expected in (("aEffectEfFirewa", "effect\\EF_FireWall.wav"),
                       ("aEffectRingBlac", "effect\\ring_black.tga")):
    value = symbols.resolve_name(name)
    require(value is not None and value.text == expected, f"effect537 PE resource bytes mismatch for {name}")

require("case 537: return ProcessEffect537();" in dispatch, "effect537 generated route missing")
require('537: "return ProcessEffect537();"' in generator, "effect537 generator route missing")
local = compact(function(src, "bool CRagEffect::ProcessEffect537()"))
ordered = ("if(m_stateCnt==5)", "mode->SendMsg(135,0,0,0)", "if(m_stateCnt==0)",
           'PlayWave("effect\\\\EF_FireWall.wav"', "m_effectPos.x-listener.x", "m_effectPos.z-listener.z",
           "m_effectPos=master->m_pos", "LaunchEffectPrim(PP_ACIDDEMON,vector3d{})",
           'GetTexture("effect\\\\ring_black.tga",false)', "prim->m_size=13.0f",
           "gi.height.fill(0.0f)", "gi.flag.fill(0)", "returntrue")
cursor = 0
for token in ordered:
    pos = local.find(token, cursor)
    require(pos >= 0, f"local effect537 branch or operation order missing: {token}")
    cursor = pos + len(token)

msg_body = mode_ref[mode_ref.index("case 0x87u:"):mode_ref.index("case 0x88u:", mode_ref.index("case 0x87u:"))]
local_msg = mode_local[mode_local.index("case 135:"):mode_local.index("case 179:", mode_local.index("case 135:"))]
for body, label in ((compact(msg_body), "Ref"), (compact(local_msg), "local")):
    for token in ("SetQuake", "1.0", "0.23999999" if label == "Ref" else "0.24", "0.2"):
        require(token in body, f"GameMode message135 {label} quake receiver missing {token}")
print("effect537 PE/Ref caller/helper, exact resources, active route, and message135 receiver passed")
