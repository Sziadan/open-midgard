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
pe_385 = re.search(
    r"case 385:\s*v92 = this->m_stateCnt;\s*"
    r"if \( v92 >= 40 && v92 <= 70 \)\s*\{\s*"
    r"if \( \(\(\*p_m_master\)->m_BodyLight & 1\) != 1 \)\s*"
    r"\+\+\(\*p_m_master\)->m_BodyLight;\s*"
    r"CRenderObject::SetArgb\(\*p_m_master, -1, 200, 150, 50\);\s*\}\s*break;",
    PE,
)
require(pe_385 is not None, "canonical PE effect385 frame window or tint order changed")
local_385 = re.search(
    r"case 385:\s*if \(master != nullptr && m_stateCnt >= 40 && m_stateCnt <= 70\)\s*\{\s*"
    r"if \(\(master->m_BodyLight & 1\) != 1\)\s*\{\s*\+\+master->m_BodyLight;\s*\}\s*"
    r"master->SetArgb\(-1, 200, 150, 50\);\s*\}\s*break;",
    helper.group(1),
)
require(local_385 is not None, "local effect385 frame window, bit gate, tint, or null-master handling changed")
require("case 385: ApplyActorLight(); return true;" in DISPATCH,
        "local effect385 dispatcher route missing")
require(actor_light_ids is not None and re.search(r"\b385\b", actor_light_ids.group(1)),
        "effect385 missing from generator actor-light group")
pe_387 = re.search(
    r"case 387:\s*if \( this->m_stateCnt == 30 \)\s*\{\s*"
    r"v49 = CModeMgr::GetGameMode\(&g_modeMgr\);\s*"
    r"v49->CMode::SendMsg\(v49, 135, 0, 0, 0\);\s*\}\s*"
    r"v50 = this->m_stateCnt;\s*if \( v50 >= 30 && v50 <= 54 \)\s*\{\s*"
    r"\(\*p_m_master\)->m_roty = \(\*p_m_master\)->m_roty \+ 30\.0;\s*"
    r"if \( \(\*p_m_master\)->m_roty > 360\.0 \)\s*"
    r"\(\*p_m_master\)->m_roty = \(\*p_m_master\)->m_roty - 360\.0;\s*\}\s*"
    r"v51 = this->m_stateCnt;\s*if \( v51 >= 30 && v51 <= 50 && v51 % 10 < 5 \)\s*\{\s*"
    r"v52 = \*p_m_master;\s*if \( \(v52->m_BodyLight & 1\) != 1 \)\s*"
    r"\+\+v52->m_BodyLight;\s*\}\s*break;",
    PE,
)
require(pe_387 is not None, "canonical PE effect387 message/yaw/body-light order or bounds changed")
local_387 = re.search(
    r"case 387:\s*if \(m_stateCnt == 30\)\s*\{\s*"
    r"if \(CGameMode\* mode = g_modeMgr.GetCurrentGameMode\(\)\)\s*\{\s*mode->SendMsg\(135, 0, 0, 0\);\s*\}\s*\}\s*"
    r"if \(master != nullptr && m_stateCnt >= 30 && m_stateCnt <= 54\)\s*\{\s*"
    r"master->m_roty \+= 30\.0f;\s*if \(master->m_roty > 360\.0f\)\s*\{\s*"
    r"master->m_roty -= 360\.0f;\s*\}\s*\}\s*"
    r"if \(master != nullptr && m_stateCnt >= 30 && m_stateCnt <= 50 && \(m_stateCnt % 10\) < 5\s*"
    r"&& \(master->m_BodyLight & 1\) != 1\)\s*\{\s*\+\+master->m_BodyLight;\s*\}\s*break;",
    helper.group(1),
)
require(local_387 is not None, "local effect387 timing, wrap, bit gate, or master guard changed")
require("case 387: ApplyActorLight(); return true;" in DISPATCH,
        "local effect387 dispatcher route missing")
require(actor_light_ids is not None and re.search(r"\b387\b", actor_light_ids.group(1)),
        "effect387 missing from generator actor-light group")
require("case 375" not in REF and "case 379" not in REF and "case 382" not in REF and "case 385" not in REF,
        "primary Ref gained a standalone actor-light case; update recorded provenance")

require("case 387" not in REF, "primary Ref gained effect387; update recorded provenance")

print("effects375/379/382/385/387 actor-light source parity passed; primary Ref has no standalone cases and local helpers guard null masters")
