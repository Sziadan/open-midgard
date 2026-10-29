#!/usr/bin/env python3
"""Compare Effect273's PE inline branch, exact call-site wave, STR, and local route."""
from __future__ import annotations
import re
import sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage  # noqa: E402

def require(ok: bool, message: str) -> None:
    if not ok: raise SystemExit(message)

def function(source: str, signature: str) -> str:
    start = source.rindex(signature); opening = source.index("{", start); depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{": depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0: return source[start:pos+1]
    raise AssertionError(signature)

def compact(source: str) -> str:
    return re.sub(r"\s+", "", source)

pe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
ref = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
src = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
generator = (ROOT / "tools/generate_effect_dispatch.py").read_text(encoding="utf-8")
onprocess = function(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
start = onprocess.index("case 273:")
end = onprocess.index("case 274:", start)
branch = compact(onprocess[start:end])
require("if(!this->m_stateCnt)" in branch and "this->m_pos=(*p_m_master)->m_pos" in branch
        and "PlayWave(aEffectMon,xbj,0.0,zbj,250,40,1.0)" in branch
        and "gotoLABEL_424" in branch, "PE effect273 state0 branch mismatch")
common = compact(onprocess[onprocess.index("LABEL_424:"):onprocess.index("LABEL_425:")])
require("ProcessEZ2STR(this,0.0,-80.0,0.0)" in common,
        "PE effect273 shared STR continuation mismatch")
asset = ExeImage().read_cstring_at(0x71A364)
require(asset is not None and asset.raw == b"effect\\mon_\xBF\xAC\xC8\xAF.wav",
        "exact effect273 call-site wave bytes at VA 0x71A364 mismatch")
require('case 273: return ProcessEffect273();' in dispatch,
        "effect273 explicit generated route missing")
require('273: "return ProcessEffect273();"' in generator and "FORCE_ROUTE.add(273)" in generator,
        "effect273 generator mapping/force route missing")
local = compact(function(src, "bool CRagEffect::ProcessEffect273()"))
ordered = ("if(m_stateCnt==0&&master!=nullptr)", "m_effectPos=master->m_pos",
           'TryPlaySingleEffectWave(m_effectPos,"effect\\\\mon_\\xBF\\xAC\\xC8\\xAF.wav",0.0f,true)',
           "returnProcessEZ2STR(0.0f,-80.0f,0.0f)")
pos = [local.find(token) for token in ordered]
require(all(i >= 0 for i in pos) and pos == sorted(pos),
        "local effect273 operation order/resource/STR return mismatch")
strrow = next(line for line in (ROOT / "src/world/RagEffectStrNameTable.generated.inc").read_text(encoding="utf-8").splitlines() if "{ 273," in line)
require('"\\xBF\\xAC\\xC8\\xAF.str"' in strrow,
        "Ref effect273 STR name differs from 연환.str")
print("effect273 PE branch, call-site wave bytes, Ref STR mapping, route and local operation order passed")


