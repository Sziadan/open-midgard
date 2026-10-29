#!/usr/bin/env python3
"""Compare effects390/391 inline PE clip-wave callers with local timed routing."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
ROUTE = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")


def function_body(source, signature):
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


pe_process = function_body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
pe_cases = {}
for effect_id, wave_symbol in ((390, "aEffectBlackOve"), (391, "aEffectEfIncagi")):
    match = re.search(
        rf"case {effect_id}:\s*if \( !this->m_stateCnt \)[\s\S]*?"
        rf"PlayWave\({wave_symbol}, [a-z]+, 0\.0, [a-z]+, 250, 40, 1\.0\);\s*\}}\s*goto LABEL_99;",
        pe_process,
    )
    if match is None:
        raise AssertionError(f"PE effect{effect_id} frame-zero wave/common STR continuation changed")
    pe_cases[effect_id] = match.group(0)

if "case 390: return ProcessClipWaveStrEffect();" not in ROUTE or "case 391: return ProcessClipWaveStrEffect();" not in ROUTE:
    raise AssertionError("local effects390/391 generated clip-wave routes missing")
local_helper = function_body(LOCAL, "bool CRagEffect::ProcessClipWaveStrEffect()")
for effect_id, wave in ((390, "effect\\\\black_overthrust.wav"), (391, "effect\\\\EF_IncAgility.wav")):
    expected = f'case {effect_id}: wave = "{wave}"; copyMasterPosition = true; propagateStrResult = false; break;'
    if expected not in local_helper:
        raise AssertionError(f"local effect{effect_id} wave/frame/STR policy changed")
for token in (
    "if (m_stateCnt == waveFrame", "if (copyMasterPosition) m_effectPos = m_cachedPos",
    "TryPlaySingleEffectWave(m_effectPos, wave, yOffset, true)",
    "const bool active = ProcessEZ2STR(0.0f, -80.0f, 0.0f)",
    "return propagateStrResult ? active : true",
):
    if token not in local_helper:
        raise AssertionError(f"local clip-wave common behavior missing {token}")

# The primary Ref's numeric cases are Init STR-name mappings, not OnProcess callers.
for effect_id, str_name in ((390, "aMeltStr"), (391, "aCartStr")):
    if REF.count(f"case {effect_id}:") != 1 or f"case {effect_id}:\n            sprintf(StrName, {str_name});" not in REF:
        raise AssertionError(f"primary-Ref effect{effect_id} caller/Init provenance changed")

image = ExeImage(ROOT / "HighPriest.exe")
for va, expected, label in (
    (0x71A598, b"effect\\black_overthrust.wav", "effect390 wave"),
    (0x71A760, b"effect\\EF_IncAgility.wav", "effect391 wave"),
):
    resource = image.read_cstring_at(va)
    if resource is None or resource.raw != expected:
        raise AssertionError(f"{label} bytes differ at PE VA 0x{va:06X}")

print("effects390/391 clip-wave parity passed: PE callers, local frame-zero routes/common STR handling, exact waves and Ref Init provenance")
