#!/usr/bin/env python3
"""Compare effect375's inline PE actor-light case with its local helper route."""
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
    r"case 375:\s*v81 = \*p_m_master;\s*"
    r"if \( \(v81->m_BodyLight & 8\) != 8 \)\s*"
    r"v81->m_BodyLight \+= 8;\s*break;",
    PE,
)
require(pe_case is not None, "canonical PE effect375 actor-light bit gate changed")
require("case 375: ApplyActorLight(); return true;" in DISPATCH,
        "local effect375 dispatcher route missing")
actor_light_ids = re.search(r"ACTOR_LIGHT_IDS\s*=\s*\{([^}]*)\}", GENERATOR)
require(actor_light_ids is not None and re.search(r"\b375\b", actor_light_ids.group(1)),
        "effect375 missing from generator actor-light group")
require(re.search(r"CRagEffect\s*::\s*ApplyActorLight\s*\(", LOCAL),
        "local ApplyActorLight helper missing")
helper = re.search(r"void CRagEffect::ApplyActorLight\s*\(\)\s*\{([\s\S]*?)\n\}", LOCAL)
require(helper is not None, "local ApplyActorLight body missing")
local_case = re.search(r"case 375:\s*if \(master != nullptr && \(master->m_BodyLight & 8\) != 8\)\s*\{\s*"
                       r"master->m_BodyLight \+= 8;\s*\}\s*break;", helper.group(1))
require(local_case is not None, "local effect375 bit gate or null-master behavior changed")
pe_379 = re.search(
    r"case 379:\s*v110 = this->m_stateCnt;\s*"
    r"if \( v110 <= 0xC8 \)\s*"
    r"CRenderObject::SetArgb\(\*p_m_master, -1, 205 - v110, 205 - v110, 255\);\s*break;",
    PE,
)
require(pe_379 is not None, "canonical PE effect379 tint interval or RGB arithmetic changed")
local_379 = re.search(
    r"case 379:\s*if \(master != nullptr && m_stateCnt <= 200\)\s*\{\s*"
    r"master->SetArgb\(-1, 205 - m_stateCnt, 205 - m_stateCnt, 255\);\s*\}\s*break;",
    helper.group(1),
)
require(local_379 is not None, "local effect379 tint or null-master guard changed")
require("case 379: ApplyActorLight(); return true;" in DISPATCH,
        "local effect379 dispatcher route missing")
require(actor_light_ids is not None and re.search(r"\b379\b", actor_light_ids.group(1)),
        "effect379 missing from generator actor-light group")
pe_382 = re.search(
    r"case 382:\s*v58 = \*p_m_master;\s*"
    r"if \( \(v58->m_BodyLight & 4\) != 4 \)\s*"
    r"v58->m_BodyLight \+= 4;\s*break;",
    PE,
)
require(pe_382 is not None, "canonical PE effect382 bit-2 gate changed")
local_382 = re.search(
    r"case 382:\s*if \(master != nullptr && \(master->m_BodyLight & 4\) != 4\)\s*\{\s*"
    r"master->m_BodyLight \+= 4;\s*\}\s*break;",
    helper.group(1),
)
require(local_382 is not None, "local effect382 bit gate or null-master guard changed")
require("case 382: ApplyActorLight(); return true;" in DISPATCH,
        "local effect382 dispatcher route missing")
require(actor_light_ids is not None and re.search(r"\b382\b", actor_light_ids.group(1)),
        "effect382 missing from generator actor-light group")
require("case 375" not in REF and "case 379" not in REF and "case 382" not in REF,
        "primary Ref gained a standalone actor-light case; update recorded provenance")

print("effects375/379/382 actor-light source parity passed; primary Ref has no standalone cases and local helpers guard null masters")
