#!/usr/bin/env python3
"""Compare effect556 ParticlePath and its PP_PARTICLE_UP backing behavior."""
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
prim_ref = ref2
src = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
dispatch = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")
generator = (ROOT / "tools/generate_effect_dispatch.py").read_text(encoding="utf-8")

pe_on = function(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
ref_on = function(ref, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
branches = []
for body, label in ((pe_on, "PE"), (ref_on, "Ref")):
    start = body.index("case 0x22C:")
    end = body.index("case 0x22D:", start)
    branch = compact(body[start:end])
    require("CRagEffect::ParticlePath(this,aEffectThunderB_1,1)" in branch and "return1;" in branch,
            f"{label} effect556 caller mismatch")
    branches.append(branch)
require(branches[0] == branches[1], "PE and Ref effect556 caller branches differ")

signature = "void __thiscall CRagEffect::ParticlePath(CRagEffect *this, char *tName, char F1)"
pe_helper = function(pe, signature)
ref_helper = function(ref2, signature)
require(compact(pe_helper) == compact(ref_helper), "PE and Ref ParticlePath helper bodies differ")
for token in ("if(!this->m_stateCnt)", "PP_PARTICLE_UP", "m_renderFlag=5", "m_duration=this->m_duration",
              "m_totalTexture=2", "m_size=0.0", "m_pos=this->m_pos", "rand()%10",
              "rand()%9", "rand()%20", "m_GI[v5].flag1[2]=1", "v5<4"):
    require(token in compact(pe_helper), f"ParticlePath PE/Ref behavior missing: {token}")

image = ExeImage()
symbols = SymbolTable(image)
assets = [("aEffectThunderB", "effect\\thunder_ball_0002.bmp"),
          ("aEffectThunderB_0", "effect\\thunder_ball_0003.bmp")]
for name, expected in assets:
    resolved = symbols.resolve_name(name)
    require(resolved is not None and resolved.text == expected, f"PE exact texture bytes mismatch for {name}")

require("case 556: return ProcessParticlePath556();" in dispatch, "effect556 generated route missing")
require('556: "return ProcessParticlePath556();"' in generator, "effect556 generator route missing")
local = compact(function(src, "void CRagEffect::SpawnParticlePath556()"))
for token in ('LaunchEffectPrim(PP_PARTICLE_UP,vector3d{})', '"effect\\\\thunder_ball_0002.bmp"',
              '"effect\\\\thunder_ball_0003.bmp"', "m_totalTexture", "m_pos=m_effectPos",
              "gi.distance=static_cast<float>(rand()%10)*0.079999998f+1.2f",
              "gi.sparkX=static_cast<float>(rand()%9)+m_deltaPos.x-4.0f",
              "gi.sparkZ=static_cast<float>(rand()%9)+m_deltaPos.z-4.0f",
              "gi.sparkY=m_deltaPos.y-6.0f", "rand()%20", "gi.flag[2]=1", "gi.height[7]=0.0f"):
    require(token in local, f"local effect556 initialization missing: {token}")
handler = compact(function(src, "bool CRagEffect::ProcessParticlePath556()"))
require(handler == "boolCRagEffect::ProcessParticlePath556(){SpawnParticlePath556();returntrue;}",
        "local effect556 handler is not a direct wrapper")

pe_prim = function(pe, "void __thiscall CEffectPrim::PrimParticleUp(CEffectPrim *this)")
ref_prim = function(prim_ref, "void __thiscall CEffectPrim::PrimParticleUp(CEffectPrim *this)")
require(compact(pe_prim) == compact(ref_prim), "PE and Ref PrimParticleUp bodies differ")
local_update = compact(function(src, "void UpdateParticleUpPrimitive(CEffectPrim* prim)"))
master_sync = "prim->m_pos=prim->m_master->EffectPosition();"
require(master_sync in local_update and local_update.index("prim->m_gi[1].alphaT=pitch;") < local_update.index(master_sync)
        < local_update.index("for(intband=0;band<4"),
        "local PP_PARTICLE_UP updater does not copy master position at the source point")
print("effect556 PE/Ref caller and helper, exact textures, route, GI initialization, and particle updater position sync passed")
