#!/usr/bin/env python3
"""Compare effect442's inline PE body-light update with its local helper."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
GENERATOR = (ROOT / "tools/generate_effect_dispatch.py").read_text(encoding="utf-8")


def require(ok, message):
    if not ok:
        raise SystemExit(message)


pe_case = re.search(
    r"case 442:\s*if \( \(\(\*p_m_master\)->m_BodyLight & 0x8000\) != 0x8000 \)\s*\{\s*"
    r"\(\*p_m_master\)->m_BodyLight \+= 0x8000;\s*"
    r"\(\*p_m_master\)->m_BodyTime3 = this->m_stateCnt;\s*\}\s*break;",
    PE,
)
require(pe_case is not None, "canonical PE effect442 bit gate/body-time update changed")
require("case 442: ApplyBodyLight442(); return true;" in DISPATCH,
        "local effect442 dispatcher route missing")
require(re.search(r"442:\s*\"ApplyBodyLight442\(\); return true;\"", GENERATOR),
        "effect442 generator direct route missing")
helper = re.search(r"void CRagEffect::ApplyBodyLight442\s*\(\)\s*\{([\s\S]*?)\n\}", LOCAL)
require(helper is not None, "local ApplyBodyLight442 body missing")
for token in (
    "ResolveCurrentMaster()", "(master->m_BodyLight & 0x8000) != 0x8000",
    "master->m_BodyLight += 0x8000", "master->m_BodyTime3 = static_cast<u8>(m_stateCnt)",
):
    require(token in helper.group(1), f"local effect442 helper missing {token}")
require("case 0x1BA" not in REF and "case 442:" not in REF,
        "primary Ref gained an explicit effect442 case; update provenance")

print("effect442 body-light source parity passed; primary Ref has no standalone case")
