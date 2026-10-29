#!/usr/bin/env python3
"""Compare effect301 PokJuk_Sound behavior and its exact PE wave bytes."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage  # noqa: E402

pe = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
ref = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="latin-1")
local = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
second_dispatch = (ROOT / "src/world/RagEffectOnProcess2Cases.generated.inc").read_text(encoding="utf-8")


def function_body(source, signature):
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 0
    for position in range(opening, len(source)):
        if source[position] == "{":
            depth += 1
        elif source[position] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:position]
    raise AssertionError(f"unterminated function {signature}")


def compact(source):
    return re.sub(r"\s+", "", source)


signature = "void __thiscall CRagEffect::PokJuk_Sound(CRagEffect *this)"
pe_helper = function_body(pe, signature)
ref_helper = function_body(ref, signature)
if compact(pe_helper) != compact(ref_helper):
    raise AssertionError("PE and primary-Ref PokJuk_Sound helpers differ")
for token in ("m_stateCnt == 300", "m_stateCnt > 2300", "m_stateCnt = 299"):
    if token not in pe_helper or token not in ref_helper:
        raise AssertionError(f"canonical source helper missing {token}")
if "PlayWave(aEffect_1, 0.0, 0.0, 0.0, 250, 40, 1.0)" not in pe_helper:
    raise AssertionError("PE frame-300 PokJuk wave call changed")

if "CRagEffect::PokJuk_Sound(this);" not in pe:
    raise AssertionError("PE OnProcess2 no longer calls PokJuk_Sound")
if "case 301: return OnProcess2Legacy();" not in second_dispatch:
    raise AssertionError("local effect301 OnProcess2 route missing")
local_switch = function_body(local, "bool CRagEffect::OnProcess2Legacy()")
match = re.search(r"case 301:[\s\S]*?return true;", local_switch)
if match is None:
    raise AssertionError("local effect301 branch missing")
local_case = match.group(0)
for token in ("m_stateCnt == 300", "m_stateCnt > 2300", "m_stateCnt = 299", "return true"):
    if token not in local_case:
        raise AssertionError(f"local effect301 branch missing {token}")
if "TryPlayEffectWaveAt(mode->m_world->m_player->m_pos" not in local_case or r"effect\\\xC6\xF8\xC1\xD7.wav" not in local_case:
    raise AssertionError("local frame-300 effect wave or origin mapping changed")

resource = ExeImage(ROOT / "HighPriest.exe").read_cstring_at(0x71C254)
if resource is None or resource.raw != b"effect\\\xC6\xF8\xC1\xD7.wav":
    raise AssertionError("PokJuk_Sound PE wave bytes differ at VA 0x71C254")

print("effect301 PokJuk_Sound parity passed: PE/Ref helper, local OnProcess2 branch and exact PE wave bytes")
