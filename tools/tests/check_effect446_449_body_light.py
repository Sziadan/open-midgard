#!/usr/bin/env python3
"""Compare inline PE body-light effects446/449 with local helpers."""
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


pe_patterns = {
    446: r"case 446:\s*if \( \(\(\*p_m_master\)->m_BodyLight & 0x20000\) != 0x20000 \)\s*\{\s*"
         r"\(\*p_m_master\)->m_BodyLight \+= 0x20000;\s*"
         r"\(\*p_m_master\)->m_BodyTime2 = this->m_stateCnt;\s*\}\s*"
         r"if \( \(\(\*p_m_master\)->m_BodyLight & 0x40\) != 0x40 && this->m_stateCnt < 25 \)\s*\{\s*"
         r"\(\*p_m_master\)->m_BodyLight \+= 64;\s*"
         r"\(\*p_m_master\)->m_BodyTime = LOWORD\(this->m_stateCnt\) \+ 11;\s*\}\s*break;",
    449: r"case 449:\s*if \( \(\(\*p_m_master\)->m_BodyLight & 0x4000\) != 0x4000 \)\s*\{\s*"
         r"\(\*p_m_master\)->m_BodyLight \+= 0x4000;\s*"
         r"if \( this->m_stateCnt <= 50 \)\s*"
         r"\(\*p_m_master\)->m_BodyTime2 = this->m_stateCnt;\s*\}\s*break;",
}
for effect_id, pattern in pe_patterns.items():
    require(re.search(pattern, PE), f"canonical PE effect{effect_id} body-light logic changed")

local_cases = {
    446: ("ApplyBodyLight446", "master->m_BodyLight += 0x20000", "master->m_BodyTime2 = static_cast<u8>(m_stateCnt)",
          "(master->m_BodyLight & 0x40) != 0x40", "m_stateCnt < 25", "master->m_BodyLight += 64",
          "master->m_BodyTime = static_cast<u16>(static_cast<unsigned short>(m_stateCnt) + 11)"),
    449: ("ApplyBodyLight449", "(master->m_BodyLight & 0x4000) != 0x4000", "master->m_BodyLight += 0x4000",
          "m_stateCnt <= 50", "master->m_BodyTime2 = static_cast<u8>(m_stateCnt)"),
}
for effect_id, tokens in local_cases.items():
    helper_name = tokens[0]
    helper = re.search(rf"void CRagEffect::{helper_name}\s*\(\)\s*\{{([\s\S]*?)\n\}}", LOCAL)
    require(helper is not None, f"local {helper_name} body missing")
    for token in tokens[1:]:
        require(token in helper.group(1), f"local effect{effect_id} helper missing {token}")
    require(f"case {effect_id}: {helper_name}(); return true;" in DISPATCH,
            f"local effect{effect_id} dispatch route missing")
    require(re.search(rf"{effect_id}:\s*\"{helper_name}\(\); return true;\"", GENERATOR),
            f"effect{effect_id} generator route missing")

require("case 446" not in REF and "case 0x1BE" not in REF
        and "case 449" not in REF and "case 0x1C1" not in REF,
        "primary Ref gained an explicit body-light case; update provenance")
print("effects446/449 body-light source parity passed; primary Ref has no standalone cases")
