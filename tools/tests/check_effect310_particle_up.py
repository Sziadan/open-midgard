#!/usr/bin/env python3
"""Compare effect310's twenty-call PARTICLE_UP burst and parent-position refresh."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage, SymbolTable  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
REF2 = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
GENERATOR = (ROOT / "tools/generate_effect_dispatch.py").read_text(encoding="utf-8")
TEST = (ROOT / "tools/tests/EffectLifecycleTests.cpp").read_text(encoding="utf-8")
symbols = SymbolTable(ExeImage())


def require(ok: bool, message: str) -> None:
    if not ok:
        raise SystemExit(message)


def extract_case(source: str, case_label: str) -> str:
    matches = list(re.finditer(rf"case {case_label}:\s*(.*?)(?=\n\s*case |\n\s*default:)", source, re.S))
    for match in matches:
        if "CRagEffect::PARTICLE_UP(this)" in match.group(1):
            return match.group(1)
    require(False, f"case {case_label} PARTICLE_UP handler missing")
    return ""


pe_handler = extract_case(PE, "0x136")
ref_handler = extract_case(REF, "0x136")
for label, body in (("PE", pe_handler), ("primary Ref", ref_handler)):
    require("v13 = 20" in body or "v13=20" in body, f"{label} burst count is not 20")
    require("CRagEffect::PARTICLE_UP(this)" in body, f"{label} handler no longer calls PARTICLE_UP")
    require("while ( v13 )" in body or "while(v13)" in body, f"{label} loop termination changed")
    require(body.index("PARTICLE_UP(this)") < body.index("return 1"), f"{label} return precedes burst")

pe_helper = re.search(r"void __thiscall CRagEffect::PARTICLE_UP\([^)]*\)\s*\{(.*?)\n\}", PE, re.S)
ref_helper = re.search(r"void __thiscall CRagEffect::PARTICLE_UP\([^)]*\)\s*\{(.*?)\n\}", REF2, re.S)
require(pe_helper is not None and ref_helper is not None, "PE/primary-Ref PARTICLE_UP helper missing")
for label, body in (("PE", pe_helper.group(1)), ("primary Ref", ref_helper.group(1))):
    for token in ("m_master->m_pos", "m_stateCnt", "PP_PARTICLE_UP", "m_duration",
                  "m_renderFlag = 5", "m_size = 0.0", "m_GI", "rand() % 10 - 10",
                  "rand() % 16", "rand() % 9", "rand() % 40", "height[8]", "height[1]"):
        require(token in body, f"{label} helper lacks {token}")
    require(body.index("m_master->m_pos") < body.index("m_stateCnt") < body.index("PP_PARTICLE_UP"),
            f"{label} parent refresh/state/launch order changed")
    require("aEffectThunderC" in body, f"{label} thunder texture argument changed")
resolved = symbols.resolve_name("aEffectThunderC")
require(resolved is not None and resolved.raw == b"effect\\thunder_center.bmp",
        "canonical PE thunder_center bitmap bytes changed")

local = re.search(r"void CRagEffect::ApplyParticleUpBurst\(\)\s*\{(.*?)\n\}", LOCAL, re.S)
require(local is not None, "local effect310 emitter missing")
body = local.group(1)
ordered = ("CRenderObject* master = ResolveCurrentMaster()", "const vector3d origin =",
           "m_effectPos = origin", "m_cachedPos = origin", "m_stateCnt != 0",
           "burst < 20", "LaunchEffectPrim", "kPrimTypeParticleUp", "m_renderFlag = 5",
           "m_duration = m_duration", '"effect\\\\thunder_center.bmp"', "band < 4",
           "rand() % 10 - 10", "rand() % 16", "rand() % 10", "rand() % 40",
           "rand() % 9", "gi.flag[2] = 10")
position = -1
for token in ordered:
    found = body.find(token, position + 1)
    require(found >= 0, f"local emitter missing or reordering: {token}")
    position = found
require(re.search(r"case 310:\s*ApplyParticleUpBurst\(\);\s*return true;", LOCAL),
        "local effect310 OnProcess route changed")
require(re.search(r"310:\s*\"ApplyParticleUpBurst\(\); return true;\"", GENERATOR),
        "effect310 generator route changed")
require("ApplyParticleUpBurst" in LOCAL and "effect310 emits twenty PP_PARTICLE_UP" in TEST,
        "effect310 lifecycle fixture or local route is missing")
print("effect310 PARTICLE_UP parity passed: 20 PE/Ref helper calls, exact resource, local refresh/order and lifecycle payload")
