#!/usr/bin/env python3
"""Compare effect401's timed NapalmBeat wave/Hit2 caller with local handling."""
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
pe_case = re.search(
    r"case 401:\s*v11 = this->m_stateCnt;\s*"
    r"if \( v11 != 20 && v11 != 30 && v11 != 40 && v11 != 50 && v11 != 60 \)\s*break;\s*"
    r"this->m_pos = \(\*p_m_master\)->m_pos;\s*"
    r"v12 = &CModeMgr::GetGameMode\(&g_modeMgr\)->m_world->m_player->m_pos;\s*"
    r"zc = this->m_pos.z - v12->z;\s*xc = this->m_pos.x - v12->x;\s*"
    r"PlayWave\(aEffectEfNapalm, xc, 0\.0, zc, 250, 40, 1\.0\);\s*"
    r"\$L146731:\s*CRagEffect::Hit2\(this, 0\);\s*break;",
    pe_process,
)
if pe_case is None:
    raise AssertionError("PE effect401 gates, position/wave order or Hit2(0) continuation changed")

if "case 401: return ProcessEffectFrameLegacy();" not in ROUTE:
    raise AssertionError("local effect401 generated dispatcher route missing")
legacy = function_body(LOCAL, "bool CRagEffect::ProcessEffectFrameLegacy()")
if "if (m_type == 401) {\n        ApplyNapalmHit();\n        return true;\n    }" not in legacy:
    raise AssertionError("local effect401 legacy handler dispatch changed")
local_helper = function_body(LOCAL, "void CRagEffect::ApplyNapalmHit()")
for token in (
    "m_stateCnt != 20 && m_stateCnt != 30 && m_stateCnt != 40", "m_stateCnt != 50 && m_stateCnt != 60",
    "ResolveCurrentMaster()", "m_effectPos = master->m_pos", 'PlayWave("effect\\\\EF_NapalmBeat.wav"',
    "SpawnHit2(0)",
):
    if token not in local_helper:
        raise AssertionError(f"local effect401 handling missing {token}")
if "case 401" in REF:
    raise AssertionError("primary Ref gained numeric effect401 caller; update recorded provenance")

image = ExeImage(ROOT / "HighPriest.exe")
wave = image.read_cstring_at(0x71A790)
if wave is None or wave.raw != b"effect\\EF_NapalmBeat.wav":
    raise AssertionError("effect401 NapalmBeat wave bytes differ at PE VA 0x71A790")

print("effect401 NapalmHit parity passed: PE frame gates/call order, local legacy route, exact wave and Ref provenance")
