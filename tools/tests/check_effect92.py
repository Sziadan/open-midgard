#!/usr/bin/env python3
"""Check Effect92 MeteorStorm against the executable, Ref, assets and local route."""
from __future__ import annotations
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage


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
dhxj = (ROOT / "Ref/dhxj/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
local = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
pe_helper = function(pe, "bool __thiscall CRagEffect::MeteorStorm(CRagEffect *this)")
ref_helper = function(ref, "bool __thiscall CRagEffect::MeteorStorm(CRagEffect *this)")
require(compact(pe_helper) == compact(ref_helper), "PE and primary Ref MeteorStorm helpers differ")
helper = compact(pe_helper)
for token in ("if(!this->m_stateCnt)", "aEffectWizardMe", "this->m_stateCnt==40",
              "SendMsg(GameMode,135,0,0,0)",
              "ProcessEZ2STR(this,0.0,-80.0,0.0)!=0"):
    require(token in helper, f"MeteorStorm helper misses {token}")

# The canonical dispatcher has an inline case rather than a helper call.
on_process = function(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
case = re.search(r"case\s+92\s*:(.*?)(?=case\s+93\s*:)", on_process, re.S)
require(case is not None, "PE OnProcess case92 missing")
case_body = compact(case.group(1))
for token in ("if(!this->m_stateCnt)", "PlayWave(aEffectWizardMe", "this->m_stateCnt==40",
              "gotoLABEL_600", "gotoLABEL_424"):
    require(token in case_body, f"PE inline case92 misses {token}")

image = ExeImage(ROOT / "HighPriest.exe")
wave = image.read_cstring_at(0x71A0D0)
require(wave is not None and wave.raw == b"effect\\wizard_meteor.wav",
        "MeteorStorm executable wave bytes differ")
str_table = (ROOT / "src/world/RagEffectStrNameTable.generated.inc").read_text(encoding="utf-8")
require(re.search(r'\{\s*92,\s*nullptr,\s*nullptr,\s*"Meteor",\s*1,\s*3\s*\}', str_table),
        "effect92 Meteor1-3 STR selector table entry missing")

# Primary Ref keeps the helper but omits its OnProcess caller; dhxj has a
# caller but comments out the quake. Preserve the mismatch as measured evidence.
require("CRagEffect::MeteorStorm(this)" not in ref, "primary Ref dispatcher caller discrepancy changed")
dhxj_helper = function(dhxj, "bool CRagEffect :: MeteorStorm()")
require("case\tEF_METEORSTORM:" in dhxj and "//if (m_stateCnt == 40)" in dhxj_helper,
        "secondary Ref caller/commented quake discrepancy changed")

local_helper = function(local, "bool CRagEffect::ProcessMeteorStorm92()")
ordered = ("m_stateCnt==0", 'PlayWave("effect\\\\wizard_meteor.wav"',
           "m_stateCnt==40", "SendMsg(135,0,0,0)",
           "ProcessEZ2STR(0.0f,-80.0f,0.0f)")
positions = [compact(local_helper).find(token) for token in ordered]
require(all(pos >= 0 for pos in positions), "local MeteorStorm92 handler misses a source operation")
require(positions == sorted(positions), "local MeteorStorm92 operation order differs")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text()
require("case 92: return ProcessMeteorStorm92();" in dispatch, "effect92 route missing")
generator = (ROOT / "tools/generate_effect_dispatch.py").read_text()
require("FORCE_ROUTE.add(92)" in generator and '92: "return ProcessMeteorStorm92();"' in generator,
        "effect92 generator route missing")
print("effect92 PE/primary-Ref helper, caller discrepancy, audio/STR resources and route passed")
