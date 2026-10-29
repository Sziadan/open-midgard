#!/usr/bin/env python3
"""Compare effect375's inline PE actor-light case with its local helper route."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
DISPATCH3 = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")
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
pe_396 = re.search(
    r"case 396:\s*if \( \(\(\*p_m_master\)->m_BodyLight & 1\) != 1 \)\s*"
    r"\+\+\(\*p_m_master\)->m_BodyLight;\s*"
    r"if \( \(\(\*p_m_master\)->m_BodyLight & 8\) != 8 \)\s*"
    r"\(\*p_m_master\)->m_BodyLight \+= 8;\s*"
    r"CRenderObject::SetArgb\(\*p_m_master, -1, 255, 89, 182\);\s*break;",
    PE,
)
require(pe_396 is not None, "canonical PE effect396 bit-setting and tint order changed")
local_396 = re.search(
    r"case 396:\s*if \(master != nullptr\)\s*\{\s*"
    r"if \(\(master->m_BodyLight & 1\) != 1\)\s*\{\s*\+\+master->m_BodyLight;\s*\}\s*"
    r"if \(\(master->m_BodyLight & 8\) != 8\)\s*\{\s*master->m_BodyLight \+= 8;\s*\}\s*"
    r"master->SetArgb\(-1, 255, 89, 182\);\s*\}\s*break;",
    helper.group(1),
)
require(local_396 is not None, "local effect396 bit gate/tint order or null-master guard changed")
require("case 396: ApplyActorLight(); return true;" in DISPATCH,
        "local effect396 dispatcher route missing")
require(actor_light_ids is not None and re.search(r"\b396\b", actor_light_ids.group(1)),
        "effect396 missing from generator actor-light group")

def process3_case(source, signature, case_id, next_case):
    body = re.search(signature + r"\s*\{([\s\S]*?)\n\}", source)
    require(body is not None, f"missing function for effect{case_id}")
    branch = re.search(rf"case 0x{case_id:X}:[\s\S]*?(?=case 0x{next_case:X}:)", body.group(1))
    require(branch is not None, f"effect{case_id} caller case missing")
    return branch.group(0)

pe_538 = process3_case(PE, r"unsigned __int8 __thiscall CRagEffect::OnProcess3\(CRagEffect \*this\)", 538, 539)
ref_538 = process3_case(REF, r"unsigned __int8 __thiscall CRagEffect::OnProcess3\(CRagEffect \*this\)", 538, 539)
require(re.sub(r"\s+", "", pe_538) == re.sub(r"\s+", "", ref_538),
        "primary Ref and PE effect538 caller branches differ")
for token in ("m_stateCnt >= 31", "m_BodyLight & 8", "m_stateCnt % 2",
              "m_BodyLight & 1", "SetArgb(this->m_master, -1, 0, 255, 0)", "goto LABEL_206"):
    require(token in pe_538, f"PE effect538 actor-light behavior missing {token}")
require("case 538: ApplyActorLight(); return true;" in DISPATCH3,
        "local effect538 dispatcher route missing")
require(actor_light_ids is not None and re.search(r"\b538\b", actor_light_ids.group(1)),
        "effect538 missing from generator actor-light group")
local_538 = re.search(
    r"case 538:\s*if \(master == nullptr \|\| m_stateCnt >= 31\)\s*\{\s*break;\s*\}\s*"
    r"if \(\(master->m_BodyLight & 8\) != 8\)\s*\{\s*master->m_BodyLight \+= 8;\s*\}\s*"
    r"if \(\(m_stateCnt % 2\) != 0\)\s*\{\s*master->SetArgb\(-1, 255, 255, 255\);\s*break;\s*\}\s*"
    r"if \(\(master->m_BodyLight & 1\) != 1\)\s*\{\s*\+\+master->m_BodyLight;\s*\}\s*"
    r"master->SetArgb\(-1, 0, 255, 0\);\s*break;",
    helper.group(1),
)
require(local_538 is not None, "local effect538 cutoff, bit/tint order, or null-master guard changed")

pe_542 = process3_case(PE, r"unsigned __int8 __thiscall CRagEffect::OnProcess3\(CRagEffect \*this\)", 542, 543)
ref_542 = process3_case(REF, r"unsigned __int8 __thiscall CRagEffect::OnProcess3\(CRagEffect \*this\)", 542, 543)
require(re.sub(r"\s+", "", pe_542) == re.sub(r"\s+", "", ref_542),
        "primary Ref and PE effect542 caller branches differ")
for token in ("m_BodyLight & 0x4000", "m_stateCnt > 150", "m_BodyLight += 0x4000",
              "m_stateCnt / 3"):
    require(token in pe_542, f"PE effect542 actor-light behavior missing {token}")
require("case 542: ApplyActorLight(); return true;" in DISPATCH3,
        "local effect542 dispatcher route missing")
require(actor_light_ids is not None and re.search(r"\b542\b", actor_light_ids.group(1)),
        "effect542 missing from generator actor-light group")
local_542 = re.search(
    r"case 542:\s*if \(master == nullptr \|\| \(master->m_BodyLight & 0x4000\) == 0x4000 \|\| m_stateCnt > 150\)\s*\{\s*break;\s*\}\s*"
    r"master->m_BodyLight \+= 0x4000;\s*master->m_BodyTime2 = static_cast<u8>\(m_stateCnt / 3\);\s*break;",
    helper.group(1),
)
require(local_542 is not None, "local effect542 bit gate, frame cutoff, body-time division or master guard changed")

pe_544 = process3_case(PE, r"unsigned __int8 __thiscall CRagEffect::OnProcess3\(CRagEffect \*this\)", 544, 545)
ref_544 = process3_case(REF, r"unsigned __int8 __thiscall CRagEffect::OnProcess3\(CRagEffect \*this\)", 544, 545)
require(re.sub(r"\s+", "", pe_544) == re.sub(r"\s+", "", ref_544),
        "primary Ref and PE effect544 caller branches differ")
for token in ("m_BodyLight & 0x40000", "m_BodyLight += 0x40000", "m_stateCnt > 200",
              "this->m_stateCnt * 0.125"):
    require(token in pe_544, f"PE effect544 actor-light behavior missing {token}")
require("case 544: ApplyActorLight(); return true;" in DISPATCH3,
        "local effect544 dispatcher route missing")
require(actor_light_ids is not None and re.search(r"\b544\b", actor_light_ids.group(1)),
        "effect544 missing from generator actor-light group")
local_544 = re.search(
    r"case 544:\s*if \(master == nullptr \|\| \(master->m_BodyLight & 0x40000\) == 0x40000\)\s*\{\s*break;\s*\}\s*"
    r"master->m_BodyLight \+= 0x40000;\s*master->m_BodyTime2 = m_stateCnt > 200\s*"
    r"\? static_cast<u8>\(25\)\s*:\s*static_cast<u8>\(static_cast<double>\(m_stateCnt\) \* 0\.125\);\s*break;",
    helper.group(1),
)
require(local_544 is not None, "local effect544 bit gate, 200/201 threshold, time conversion or master guard changed")

for effect_id, next_id, mask in ((548, 549, "0x40000"), (549, 550, "0x4000")):
    pe_case = process3_case(PE, r"unsigned __int8 __thiscall CRagEffect::OnProcess3\(CRagEffect \*this\)", effect_id, next_id)
    ref_case = process3_case(REF, r"unsigned __int8 __thiscall CRagEffect::OnProcess3\(CRagEffect \*this\)", effect_id, next_id)
    require(re.sub(r"\s+", "", pe_case) == re.sub(r"\s+", "", ref_case),
            f"primary Ref and PE effect{effect_id} caller branches differ")
    if effect_id == 548:
        require("m_stateCnt > 16" in pe_case and mask in pe_case and "goto LABEL_367" in pe_case,
                "PE effect548 mask/frame/shared BodyTime2 route changed")
    else:
        require("m_stateCnt > 16" in pe_case and "BYTE1(m_BodyLight) |= 0x40u" in pe_case
                and "3 * LOBYTE(this->m_stateCnt)" in pe_case,
                "PE effect549 mask/frame/body-time behavior changed")
    require(f"case {effect_id}: ApplyActorLight(); return true;" in DISPATCH3,
            f"local effect{effect_id} dispatcher route missing")
    require(actor_light_ids is not None and re.search(rf"\b{effect_id}\b", actor_light_ids.group(1)),
            f"effect{effect_id} missing from generator actor-light group")
    require(re.search(rf"case {effect_id}:\s*if \(master != nullptr && m_stateCnt <= 16\)\s*\{{\s*"
                      rf"master->m_BodyLight \|= {mask};\s*"
                      r"master->m_BodyTime2 = static_cast<u8>\(3 \* \(m_stateCnt & 0xFF\)\);\s*\}",
                      helper.group(1)),
            f"local effect{effect_id} mask/frame/byte conversion or master guard changed")

pe_550 = process3_case(PE, r"unsigned __int8 __thiscall CRagEffect::OnProcess3\(CRagEffect \*this\)", 550, 551)
ref_550 = process3_case(REF, r"unsigned __int8 __thiscall CRagEffect::OnProcess3\(CRagEffect \*this\)", 550, 551)
require(re.sub(r"\s+", "", pe_550) == re.sub(r"\s+", "", ref_550),
        "primary Ref and PE effect550 caller branches differ")
for token in ("v100 <= 30", "v100 > 50", "BYTE1(v102) |= 0x20u", "m_stateCnt"):
    require(token in pe_550, f"PE effect550 actor-light behavior missing {token}")
require("case 550: ApplyActorLight(); return true;" in DISPATCH3,
        "local effect550 dispatcher route missing")
require(actor_light_ids is not None and re.search(r"\b550\b", actor_light_ids.group(1)),
        "effect550 missing from generator actor-light group")
require(re.search(r"case 550:\s*if \(master != nullptr && m_stateCnt > 30 && m_stateCnt <= 50\)\s*\{\s*"
                  r"master->m_BodyLight \|= 0x2000;\s*master->m_BodyTime = frame;\s*\}", helper.group(1)),
        "local effect550 mask/frame/body-time behavior or master guard changed")

pe_500 = process3_case(PE, r"unsigned __int8 __thiscall CRagEffect::OnProcess3\(CRagEffect \*this\)", 500, 501)
ref_500 = process3_case(REF, r"unsigned __int8 __thiscall CRagEffect::OnProcess3\(CRagEffect \*this\)", 500, 501)
require(re.sub(r"\s+", "", pe_500) == re.sub(r"\s+", "", ref_500),
        "primary Ref and PE effect500 caller branches differ")
for token in ("m_stateCnt >= 31", "m_BodyLight & 8", "m_stateCnt % 2",
              "m_BodyLight & 1", "SetArgb(this->m_master, -1, 0, 0, 255)"):
    require(token in pe_500, f"PE effect500 blue/white flash behavior missing {token}")
require("case 500: ApplyActorLight(); return true;" in DISPATCH3,
        "local effect500 dispatcher route missing")
require(actor_light_ids is not None and re.search(r"\b500\b", actor_light_ids.group(1)),
        "effect500 missing from generator actor-light group")
require(re.search(r"case 500:\s*if \(m_stateCnt < 31\)\s*\{\s*flashBody\(0, 0, 255\);\s*\}", helper.group(1)),
        "local effect500 cutoff/blue flash mapping changed")

pe_521 = process3_case(PE, r"unsigned __int8 __thiscall CRagEffect::OnProcess3\(CRagEffect \*this\)", 521, 522)
ref_521 = process3_case(REF, r"unsigned __int8 __thiscall CRagEffect::OnProcess3\(CRagEffect \*this\)", 521, 522)
require(re.sub(r"\s+", "", pe_521) == re.sub(r"\s+", "", ref_521),
        "primary Ref and PE effect521 caller branches differ")
for token in ("v58 < 10", "v58 >= 51", "m_BodyLight & 8", "m_stateCnt % 2",
              "m_BodyLight & 1", "SetArgb(this->m_master, -1, 255, 0, 0)"):
    require(token in pe_521, f"PE effect521 red-flash interval/order missing {token}")
require("case 521: ApplyActorLight(); return true;" in DISPATCH3,
        "local effect521 dispatcher route missing")
require(actor_light_ids is not None and re.search(r"\b521\b", actor_light_ids.group(1)),
        "effect521 missing from generator actor-light group")
require(re.search(r"case 521:\s*if \(m_stateCnt >= 10 && m_stateCnt < 51\)\s*\{\s*"
                  r"flashBody\(255, 0, 0\);\s*\}", helper.group(1)),
        "local effect521 red-flash interval changed")

pe_625 = process3_case(PE, r"unsigned __int8 __thiscall CRagEffect::OnProcess3\(CRagEffect \*this\)", 625, 626)
ref_625 = process3_case(REF, r"unsigned __int8 __thiscall CRagEffect::OnProcess3\(CRagEffect \*this\)", 625, 626)
require(re.sub(r"\s+", "", pe_625) == re.sub(r"\s+", "", ref_625),
        "primary Ref and PE effect625 caller branches differ")
for token in ("m_stateCnt % 4", "m_BodyLight & 0x4000", "m_BodyLight + 0x4000", "goto LABEL_182"):
    require(token in pe_625, f"PE effect625 cadence/body-light behavior missing {token}")
require("case 625: ApplyActorLight(); return true;" in DISPATCH3,
        "local effect625 dispatcher route missing")
require(actor_light_ids is not None and re.search(r"\b625\b", actor_light_ids.group(1)),
        "effect625 missing from generator actor-light group")
require(re.search(r"case 625:\s*if \(master != nullptr && \(m_stateCnt % 4\) == 0 && "
                  r"\(master->m_BodyLight & 0x4000\) != 0x4000\)\s*\{\s*"
                  r"master->m_BodyLight \+= 0x4000;\s*master->m_BodyTime2 = 20;\s*\}", helper.group(1)),
        "local effect625 cadence, bit gate, BodyTime2 or master guard changed")

pe_626 = process3_case(PE, r"unsigned __int8 __thiscall CRagEffect::OnProcess3\(CRagEffect \*this\)", 626, 627)
ref_626 = process3_case(REF, r"unsigned __int8 __thiscall CRagEffect::OnProcess3\(CRagEffect \*this\)", 626, 627)
require(re.sub(r"\s+", "", pe_626) == re.sub(r"\s+", "", ref_626),
        "primary Ref and PE effect626 caller branches differ")
for token in ("m_stateCnt & 0x80000003", "m_BodyLight & 0x40000", "m_BodyLight + 0x40000", "m_BodyTime2 = 20"):
    require(token in pe_626, f"PE effect626 cadence/body-light behavior missing {token}")
require("case 626: ApplyActorLight(); return true;" in DISPATCH3,
        "local effect626 dispatcher route missing")
require(actor_light_ids is not None and re.search(r"\b626\b", actor_light_ids.group(1)),
        "effect626 missing from generator actor-light group")
require(re.search(r"case 626:\s*if \(master != nullptr && \(m_stateCnt % 4\) == 0 && "
                  r"\(master->m_BodyLight & 0x40000\) != 0x40000\)\s*\{\s*"
                  r"master->m_BodyLight \+= 0x40000;\s*master->m_BodyTime2 = 20;\s*\}", helper.group(1)),
        "local effect626 cadence, bit gate, BodyTime2 or master guard changed")

for effect_id, next_id, hex_case, mask, local_time, source_time_tokens in (
    (655, 656, 0x28F, "0x80000", "m_stateCnt >= 200 ? static_cast<u8>(-56) : static_cast<u8>(m_stateCnt)",
     ("m_BodyLight & 0x80000", "m_BodyLight += 0x80000", "m_BodySin5 = v17", "v17 >= 200", "-56")),
    (656, 665, 0x290, "0x80000", "static_cast<u8>(-56 - 5 * (m_stateCnt & 0xFF))",
     ("m_stateCnt >= 40", "m_BodyLight & 0x80000", "-56 - 5 * LOBYTE(this->m_stateCnt)")),
    (667, 668, 0x29B, "2", "m_stateCnt >= 25 ? static_cast<u8>(-56) : static_cast<u8>(8 * m_stateCnt)",
     ("m_BodyLight & 2", "m_BodyLight += 2", "v13 >= 25", "8 * v13", "-56")),
):
    pe_case = process3_case(PE, r"unsigned __int8 __thiscall CRagEffect::OnProcess3\(CRagEffect \*this\)", effect_id, next_id)
    ref_case = process3_case(REF, r"unsigned __int8 __thiscall CRagEffect::OnProcess3\(CRagEffect \*this\)", effect_id, next_id)
    require(re.sub(r"\s+", "", pe_case) == re.sub(r"\s+", "", ref_case),
            f"primary Ref and PE effect{effect_id} caller branches differ")
    require(all(token in pe_case for token in source_time_tokens),
            f"PE effect{effect_id} body-light/sine timing changed")
    require(f"case {effect_id}: ApplyActorLight(); return true;" in DISPATCH3,
            f"local effect{effect_id} dispatcher route missing")
    require(actor_light_ids is not None and re.search(rf"\b{effect_id}\b", actor_light_ids.group(1)),
            f"effect{effect_id} missing from generator actor-light group")
    require(re.search(rf"case {effect_id}:\s*if \(master != nullptr(?: && .*?)?\)\s*\{{\s*"
                      rf"master->m_BodyLight (?:\|=|\+=) {mask};\s*"
                      rf"master->m_BodySin5 = {re.escape(local_time)};\s*\}}",
                      helper.group(1)),
            f"local effect{effect_id} bit/timing profile or master guard changed")

pe_414 = re.search(r"case 414:\s*if \(\s*\(\(\*p_m_master\)->m_BodyLight & 0x40\) != 0x40 && "
                   r"this->m_stateCnt < 18\s*\)\s*\{\s*"
                   r"\(\*p_m_master\)->m_BodyLight \+= 64;\s*"
                   r"\(\*p_m_master\)->m_BodyTime = 2 \* LOWORD\(this->m_stateCnt\);\s*\}", PE)
require(pe_414 is not None, "PE effect414 body-light bit/state/time branch changed")
require("case 414" not in REF, "primary Ref added effect414; update its source provenance")
require("case 414: ApplyActorLight(); return true;" in DISPATCH,
        "local effect414 dispatcher route missing")
require(actor_light_ids is not None and re.search(r"\b414\b", actor_light_ids.group(1)),
        "effect414 missing from generator actor-light group")
require(re.search(r"case 414:\s*if \(master != nullptr && \(master->m_BodyLight & 0x40\) != 0x40 && "
                  r"m_stateCnt < 18\)\s*\{\s*master->m_BodyLight \+= 64;\s*"
                  r"master->m_BodyTime = static_cast<u16>\(2 \* frame\);\s*\}", helper.group(1)),
        "local effect414 master guard, bit gate, state cutoff or body-time conversion changed")
require("case 375" not in REF and "case 379" not in REF and "case 382" not in REF and "case 385" not in REF,
        "primary Ref gained a standalone actor-light case; update recorded provenance")

require("case 387" not in REF and "case 396" not in REF,
        "primary Ref gained an explicit actor-light case; update recorded provenance")

# These later PE cases have no primary-Ref caller branches; compare their inline
# profiles to the local consolidated helper and generated dispatch table.
late_profiles = {
    417: ("m_BodyLight & 0x100", "m_BodyLight += 256", "m_stateCnt < 15", "m_BodyTime = this->m_stateCnt"),
    419: ("m_BodyLight & 0x200", "m_BodyLight += 512"),
    420: ("m_BodyLight & 0x10", "m_BodyLight += 16", "m_stateCnt <= 45", "m_BodySin3 = this->m_stateCnt"),
    421: ("m_BodyLight & 0x10", "m_BodyLight += 16", "m_BodySin3 = 45"),
    422: ("m_BodyLight & 0x400", "m_BodyLight += 1024", "m_stateCnt <= 45", "m_BodySin4 = this->m_stateCnt"),
    423: ("m_BodyLight & 0x400", "m_BodyLight += 1024", "m_BodySin4 = 45"),
    424: ("m_BodyLight & 0x800", "m_BodyLight += 2048", "m_stateCnt >= 200", "m_BodySin5 = -56"),
    425: ("m_stateCnt >= 30", "m_BodyTime = LOWORD(this->m_stateCnt) - 30"),
    426: ("m_BodyLight & 0x2000", "m_BodyLight += 0x2000", "m_BodyTime = this->m_stateCnt"),
    427: ("m_BodyLight & 0x800", "m_BodyLight += 2048", "m_stateCnt < 150", "m_BodyTime = v96 + 50"),
    458: ("m_BodyLight & 0x2000", "m_BodyLight += 0x2000", "m_BodyTime = this->m_stateCnt"),
}
for effect_id, profile in late_profiles.items():
    require(f"case {effect_id}:" not in REF,
            f"primary Ref gained effect{effect_id}; update its source provenance")
    require(all(token in PE for token in profile),
            f"PE effect{effect_id} actor-light profile changed")
    require(f"case {effect_id}: ApplyActorLight(); return true;" in DISPATCH,
            f"local effect{effect_id} dispatcher route missing")
    require(actor_light_ids is not None and re.search(rf"\b{effect_id}\b", actor_light_ids.group(1)),
            f"effect{effect_id} missing from generator actor-light group")
    require(re.search(rf"case {effect_id}:", helper.group(1)),
            f"local ApplyActorLight effect{effect_id} profile missing")

require("case 426:\n    case 458:" in helper.group(1),
        "local effect426/458 shared profile changed")

pe_415 = re.search(
    r"case 415:\s*if \( this->m_stateCnt == 17 \)\s*\{\s*"
    r"this->m_pos = \(\*p_m_master\)->m_pos;[\s\S]*?"
    r"PlayWave\(aEffectEfHit2Wa, [^;]*\);\s*\}\s*"
    r"if \( this->m_stateCnt == 25 \)\s*\{[\s\S]*?"
    r"PlayWave\(aEffectEfHit4Wa, [^;]*\);\s*\}\s*"
    r"if \( \(\(\*p_m_master\)->m_BodyLight & 0x80\) != 0x80 && this->m_stateCnt >= 30 \)\s*\{\s*"
    r"\(\*p_m_master\)->m_BodyLight \+= 128;\s*"
    r"\(\*p_m_master\)->m_BodyTime = LOWORD\(this->m_stateCnt\) - 30;",
    PE,
)
require(pe_415 is not None, "PE effect415 cue positions, wave order, or body-light threshold changed")
require("case 415" not in REF, "primary Ref gained effect415; update its source provenance")
require("case 415: ApplyActorLight(); return true;" in DISPATCH,
        "local effect415 dispatcher route missing")
require(actor_light_ids is not None and re.search(r"\b415\b", actor_light_ids.group(1)),
        "effect415 missing from generator actor-light group")
local_415 = re.search(
    r"case 415:\s*if \(m_stateCnt == 17\)\s*\{\s*"
    r"m_effectPos = origin;\s*m_cachedPos = origin;\s*"
    r"playAtListener\(\"effect\\\\EF_hit2\.wav\"\);\s*\}\s*"
    r"if \(m_stateCnt == 25\)\s*\{\s*"
    r"m_effectPos = origin;\s*m_cachedPos = origin;\s*"
    r"playAtListener\(\"effect\\\\EF_hit4\.wav\"\);\s*\}\s*"
    r"if \(master != nullptr && \(master->m_BodyLight & 0x80\) != 0x80 && m_stateCnt >= 30\)\s*\{\s*"
    r"master->m_BodyLight \+= 128;\s*master->m_BodyTime = static_cast<u16>\(frame - 30\);",
    helper.group(1),
)
require(local_415 is not None,
        "local effect415 must refresh both origin caches before exact waves and preserve one-shot frame30 timing")

print("actor-light source parity passed for effects375/379/382/385/387/396/414/415/417/419-427/458/500/521/538/542/544/548/549/550/625/626/655/656/667")
