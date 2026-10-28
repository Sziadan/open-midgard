#!/usr/bin/env python3
"""Compare Effect90 Lord executable/reference control flow and routed port."""
from __future__ import annotations
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage


def require(ok, message):
    if not ok:
        raise SystemExit(message)


def function(source, signature):
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


def compact(source):
    return re.sub(r"\s+", "", source)


pe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
ref = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
local = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
require(re.search(r"case\s+90\s*:\s*result\s*=\s*CRagEffect::Lord\(this\);", pe) is not None,
        "PE OnProcess case90 Lord caller missing")
pe_helper = function(pe, "bool __thiscall CRagEffect::Lord(CRagEffect *this)")
ref_helper = function(ref, "bool __thiscall CRagEffect::Lord(CRagEffect *this)")
require(compact(pe_helper) == compact(ref_helper), "PE and Ref Lord helper bodies differ")
body = compact(pe_helper)
for token in ("aEffectWizardMe_0", "aEffectHunterBl", "case0:", "case0xA:", "case0x14:",
              "case0x32:", "case0x50:", "case0x64:", "case0x82:", "case0x8C:",
              "case0xB4:", "case0xC8:", "m_stateCnt>=50", "m_stateCnt%50", "m_stateCnt<300",
              "SendMsg(v7,135,0,0,0)", "ProcessEZ2STR(this,0.0,-80.0,0.0)"):
    require(token in body, f"Lord helper missing {token}")

image = ExeImage(ROOT / "HighPriest.exe")
for va, expected in ((0x71BCC0, b"effect\\wizard_meteo.wav"),
                     (0x71A054, b"effect\\hunter_blastmine.wav")):
    actual = image.read_cstring_at(va)
    require(actual is not None and actual.raw == expected, f"Lord audio bytes differ at 0x{va:X}")

local_helper = function(local, "bool CRagEffect::ProcessLord90()")
ordered = ("m_effectPos.x-listener->m_pos.x", "m_effectPos.z-listener->m_pos.z",
           "case0:", 'PlayWave("effect\\\\wizard_meteo.wav"',
           'PlayWave("effect\\\\hunter_blastmine.wav"', "case10:",
           "case20:case50:case80:case100:case130:case140:case180:case200:",
           "m_stateCnt>=50", "m_stateCnt%50", "m_stateCnt<300", "SendMsg(135,0,0,0)",
           "ProcessEZ2STR(0.0f,-80.0f,0.0f)")
positions = [compact(local_helper).find(token) for token in ordered]
require(all(pos >= 0 for pos in positions), "local Lord90 handler misses a source cue or completion operation")
require(positions == sorted(positions), "local Lord90 state cues or message cadence order differs")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text()
require("case 90: return ProcessLord90();" in dispatch, "effect90 route missing")
generator = (ROOT / "tools/generate_effect_dispatch.py").read_text()
require("FORCE_ROUTE.add(90)" in generator and '90: "return ProcessLord90();"' in generator,
        "effect90 generator route missing")
print("effect90 PE/Ref helper, executable audio, cadence, completion and route passed")
