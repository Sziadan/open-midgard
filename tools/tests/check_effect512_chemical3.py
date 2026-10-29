#!/usr/bin/env python3
"""Compare effect512 repeated CHEMICAL3 calls and actor tint window."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
REF_HELPER = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="latin-1")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
ROUTES = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")


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
                return source[opening + 1:pos]
    raise AssertionError(f"unterminated function: {signature}")


def compact(text: str) -> str:
    return re.sub(r"\s+", "", text)


def effect512_case(body: str) -> str:
    start = body.index("case 0x200:")
    end = body.index("case 0x201:", start)
    return body[start:end]


pe_onprocess3 = function(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
ref_onprocess3 = function(REF, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
pe_case = effect512_case(pe_onprocess3)
ref_case = effect512_case(ref_onprocess3)
if compact(pe_case) != compact(ref_case):
    raise AssertionError("PE/primary-Ref effect512 caller branch differs")
for token in ("v63 = 4", "CHEMICAL3(this, 10)", "CHEMICAL3(this, 11)", "while ( v63 )",
              "m_stateCnt == 44", "SendMsg(GameMode, 135, 0, 0, 0)", "this->m_master->m_pos",
              "PlayWave(aEffect_5", "v67 < 20", "v67 > 100", "m_BodyLight & 8",
              "SetArgb(this->m_master, -1, 255, 255, 150)"):
    if token not in pe_case:
        raise AssertionError(f"PE effect512 behavior missing {token}")

if "case 512: return ProcessEffectFrameLegacy();" not in ROUTES:
    raise AssertionError("local effect512 generated route changed")
local_process = function(LOCAL, "bool CRagEffect::ProcessEffectFrameLegacy()")
local_case = re.search(r"const bool chemicalSlash = dispatchHandler && \(m_type == 439 \|\| m_type == 512 \|\| m_type == 623\);[\s\S]*?\} else if \(blastMine\)", local_process)
if local_case is None:
    raise AssertionError("local Chemical3 grouped caller branch missing")
for token in ("const short slashTime = static_cast<short>(m_type == 512 ? 10",
              "repeat < 4", "ApplyChemical3(slashTime)",
              "ApplyChemical3(static_cast<short>(slashTime + 1))", "m_type == 512 && m_stateCnt == 44",
              "mode->SendMsg(135, 0, 0, 0)", '"effect\\\\\\xBF\\xAC\\xC1\\xD6\\xBA\\xD8\\xB0\\xDD.wav"',
              "m_type == 512 && master != nullptr && m_stateCnt >= 20 && m_stateCnt <= 100",
              "master->m_BodyLight & 8", "master->SetArgb(-1, 255, 255, 150)"):
    if token not in local_case.group(0):
        raise AssertionError(f"local effect512 behavior missing {token}")

pe_emitter = function(PE, "void __thiscall CRagEffect::CHEMICAL3(CRagEffect *this, __int16 time)")
ref_emitter = function(REF_HELPER, "void __thiscall CRagEffect::CHEMICAL3(CRagEffect *this, __int16 time)")
if compact(pe_emitter) != compact(ref_emitter):
    raise AssertionError("PE/primary-Ref CHEMICAL3 emitter helper differs")
local_emitter = function(LOCAL, "void CRagEffect::ApplyChemical3(short time)")
for token in ("m_stateCnt != 0", "kPrimTypeChemical3", "time >= 10", '"effect\\\\line3y.tga"',
              '"effect\\\\line3.tga"', "band < 4", "gi.process = 0", "gi.distance = 100.0f"):
    if token not in local_emitter:
        raise AssertionError(f"local CHEMICAL3 emitter behavior missing {token}")

print("effect512 CHEMICAL3 parity passed: PE/Ref caller and helper, local repeated calls, frame44/tint window")
