#!/usr/bin/env python3
"""Keep the generated Ref dispatcher complete and its gaps observable."""
from __future__ import annotations

import csv
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "src/world"
INDEX = ROOT / "docs/skill/generated/effect_handler_index.tsv"


def fail(message: str) -> None:
    raise SystemExit(message)


with INDEX.open(encoding="utf-8", newline="") as stream:
    index = list(csv.DictReader(stream, delimiter="\t"))
if len(index) != 693:
    fail(f"expected 693 effect index rows, got {len(index)}")

expected = {
    tier: {int(row["case"]) for row in index if row["tier"] == tier}
    for tier in ("OnProcess", "OnProcess2", "OnProcess3")
}
files = {
    "OnProcess": "RagEffectOnProcessCases.generated.inc",
    "OnProcess2": "RagEffectOnProcess2Cases.generated.inc",
    "OnProcess3": "RagEffectOnProcess3Cases.generated.inc",
}
unported: set[int] = set()
case_lines: dict[str, dict[int, str]] = {}
for tier, filename in files.items():
    text = (SRC / filename).read_text(encoding="utf-8")
    case_lines[tier] = {
        int(match.group(1)): match.group(2)
        for line in text.splitlines()
        if (match := re.match(r"^case\s+(\d+):\s*(.*)", line))
    }
    found = set(case_lines[tier])
    if found != expected[tier]:
        fail(f"{tier} cases differ from effect index: missing={sorted(expected[tier] - found)} extra={sorted(found - expected[tier])}")
    if "UnportedEffect()" not in text:
        fail(f"{tier} has no explicit unported route")
    unported.update(int(value) for value in re.findall(r"^case\s+(\d+):\s*return UnportedEffect\(\);", text, re.M))
if tuple(map(len, (expected["OnProcess"], expected["OnProcess2"], expected["OnProcess3"]))) != (416, 73, 204):
    fail("dispatcher tier counts changed from 416 / 73 / 204")

rag = (SRC / "RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
process = re.search(r"bool CRagEffect::ProcessEffectFrame\(\)\s*\{(.*?)\n\}", rag, re.S)
if not process:
    fail("ProcessEffectFrame implementation not found")
body = process.group(1)
if body.find("ProcessReferenceStrOnly()") > body.find("m_type >= 300 && m_type <= 373"):
    fail("reference STR-only route must precede tier dispatch")
if "if (m_type >= 300 && m_type <= 373) return OnProcess2();" not in body or "if (m_type >= 491) return OnProcess3();" not in body:
    fail("ProcessEffectFrame tier boundaries changed")
if "Invalid Effect Id %d\\n" not in body:
    fail("ProcessEffectFrame default must log invalid ids")
if "UNPORTED effect %d (Ref %s)\\n" not in rag:
    fail("UnportedEffect diagnostic must include the Ref handler")
required_routes = {
    ("OnProcess", 41): "ProcessAngelus",
    ("OnProcess", 49): "ProcessFireHit",
    ("OnProcess", 67): "ProcessProvoke",
    ("OnProcess", 87): "ProcessLexDivina",
    ("OnProcess", 133): "ProcessStrTerminalEffect",
    ("OnProcess", 134): "ProcessStrTerminalEffect",
    ("OnProcess", 136): "ProcessEZ2STR",
    ("OnProcess", 251): "ProcessEZ2STR",
    ("OnProcess", 406): "ProcessEZ2STR",
    ("OnProcess", 407): "ProcessEZ2STR",
    ("OnProcess", 159): "SpawnTopRank",
    ("OnProcess", 164): "SpawnStatusState",
    ("OnProcess", 170): "ApplyCartRevolution",
    ("OnProcess", 183): "ProcessHit2Blue",
    ("OnProcess", 196): "ApplyCurseAttack",
    ("OnProcess", 212): "SpawnDarkBreath",
    ("OnProcess", 217): "SpawnEnergyDrain",
    ("OnProcess", 218): "ProcessPotionConcentration",
    ("OnProcess", 219): "ProcessPotionConcentration",
    ("OnProcess", 244): "UnportedEffect",
    ("OnProcess", 248): "ApplyProvidence",
    ("OnProcess2", 305): "OnProcess2Legacy",
    ("OnProcess2", 306): "OnProcess2Legacy",
    ("OnProcess2", 311): "OnProcess2Legacy",
    ("OnProcess2", 315): "ApplyGlassWall2",
    ("OnProcess2", 366): "SpawnBash3D",
    ("OnProcess2", 369): "OnProcess2Legacy",
    ("OnProcess3", 537): "UnportedEffect",
    ("OnProcess3", 612): "UnportedEffect",
    ("OnProcess3", 565): "ProcessTier3StrEffect",
    ("OnProcess3", 569): "ProcessTier3StrEffect",
    ("OnProcess3", 593): "ProcessTier3StrEffect",
    ("OnProcess3", 603): "ProcessTier3StrEffect",
    ("OnProcess3", 608): "ProcessTier3StrEffect",
    ("OnProcess3", 609): "ProcessTier3StrEffect",
    ("OnProcess3", 668): "ProcessTier3StrEffect",
    ("OnProcess3", 669): "ProcessTier3StrEffect",
    ("OnProcess3", 677): "ProcessTier3StrEffect",
    ("OnProcess3", 635): "UnportedEffect",
    ("OnProcess3", 636): "UnportedEffect",
}
for effect_id in range(475, 485):
    required_routes[("OnProcess", effect_id)] = "ApplyBlind"
for (tier, effect_id), destination in required_routes.items():
    if destination not in case_lines[tier][effect_id]:
        fail(f"{tier} effect {effect_id} must route to {destination}")
clip_wave_routes = (65, 66, 69, 75, 76, 77, 78, 83, 84, 85, 88, 91, 96, 97, 103,
                    106, 107, 108, 111, 113, 124, 127, 129, 131, 153, 154, 155,
                    130, 250, 204, 205, 206, 207, 210, 234, 245, 255, 256, 257, 258, 390, 391, 95, 102)
for effect_id in clip_wave_routes:
    if "ProcessClipWaveStrEffect" not in case_lines["OnProcess"][effect_id]:
        fail(f"OnProcess effect {effect_id} must use ProcessClipWaveStrEffect")
clip_wave = re.search(r"bool CRagEffect::ProcessClipWaveStrEffect\(\)\s*\{(.*?)\n\}", rag, re.S)
if not clip_wave:
    fail("ProcessClipWaveStrEffect implementation not found")
clip_body = clip_wave.group(1)
for effect_id, frame, wave in (
    (65, 0, "thief_invenom.wav"), (66, 0, "Acolyte_cure.wav"),
    (69, 0, "hunter_skidtrap.wav"), (75, 0, "priest_gloria.wav"),
    (84, 60, "priest_impositio.wav"), (85, 15, "priest_lexaeterna.wav"),
    (96, 75, "wizard_fire_pillar_a.wav"), (106, 20, "hunter_blastmine.wav"),
    (113, 0, "priest_magnus.wav"), (129, 10, "assasin_venomsplasher.wav"),
    (131, 0, "knight_autocounter.wav"), (154, 15, "bs_refinesuccess.wav"),
    (155, 5, "bs_refinefailed.wav"), (130, 0, "knight_twohandquicken.wav"),
    (250, 0, "knight_twohandquicken.wav"), (204, 0, "_heal_effect.wav"),
    (210, 0, "_heal_effect.wav"), (234, 0, "sage_spell breake.wav"),
    (245, 0, "cru_holy cross.wav"), (255, 10, "_enemy_hit_wind1.wav"),
    (258, 10, "_enemy_hit_wind1.wav"), (390, 0, "black_overthrust.wav"),
    (391, 0, "EF_IncAgility.wav"), (95, 0, "wizard_quagmire.wav"),
    (102, 30, "wizard_fire_pillar_b.wav"),
):
    if f"case {effect_id}:" not in clip_body or f"waveFrame = {frame};" not in clip_body or wave not in clip_body:
        fail(f"ProcessClipWaveStrEffect must preserve effect {effect_id}'s timed clip wave")
if "TryPlaySingleEffectWave(m_effectPos, wave, yOffset, true)" not in clip_body or "const bool active = ProcessEZ2STR(0.0f, -80.0f, 0.0f);" not in clip_body:
    fail("ProcessClipWaveStrEffect must preserve effect position, literal Y offset, and STR result")
for required in ("m_effectPos = m_cachedPos;", "m_duration = 9999;", "PlayWave(wave, 0.0f, 0.0f, 0.0f, 250, 40, 1.0f)", "return propagateStrResult ? active : true;", "rand() % 12 == 0", "mode->SendMsg(135, 0, 0, 0)"):
    if required not in clip_body:
        fail(f"ProcessClipWaveStrEffect must preserve Ref side effect: {required}")
tier3_str = re.search(r"bool CRagEffect::ProcessTier3StrEffect\(\)\s*\{(.*?)\n\}", rag, re.S)
if not tier3_str:
    fail("ProcessTier3StrEffect implementation not found")
tier3_body = tier3_str.group(1)
for before, after in (("TryPlayEffectWaveAt(wavePos, wave)", "mode->SendMsg(135, 0, 0, 0)"),
                      ("mode->SendMsg(135, 0, 0, 0)", "ProcessEZ2STR(0.0f, -80.0f, 0.0f)"),
                      ("ApplyPoison(1)", "ProcessEZ2STR(0.0f, -80.0f, 0.0f)")):
    if before not in tier3_body or after not in tier3_body or tier3_body.index(before) >= tier3_body.index(after):
        fail(f"ProcessTier3StrEffect order must preserve {before} before {after}")
for helper, frames, wave in (("ProcessProvoke", ("5", "30"), "swordman_provoke.wav"),
                             ("ProcessLexDivina", ("0", "20", "35", "60", "70"), "priest_lexdivina.wav")):
    match = re.search(rf"bool CRagEffect::{helper}\(\)\s*\{{(.*?)\n\}}", rag, re.S)
    if not match or wave not in match.group(1) or "TryPlaySingleEffectWave(m_effectPos" not in match.group(1) or "0.0f, true" not in match.group(1):
        fail(f"{helper} must preserve its Ref wave and effect position")
    body = match.group(1)
    if any(f"m_stateCnt == {frame}" not in body for frame in frames) or "ProcessEZ2STR(0.0f, -80.0f, 0.0f)" not in body:
        fail(f"{helper} must preserve its Ref state gates and STR result")
terminal_str = re.search(r"bool CRagEffect::ProcessStrTerminalEffect\(\)\s*\{(.*?)\n\}", rag, re.S)
if not terminal_str or "m_stateCnt = 9999;" not in terminal_str.group(1) or "return false;" not in terminal_str.group(1):
    fail("effects 133/134 must set terminal state 9999 when STR processing completes")
if "propagateStrResult = false" not in tier3_body or "return propagateStrResult ? active : true;" not in tier3_body:
    fail("ProcessTier3StrEffect must preserve both STR return contracts")
blind_update = re.search(r"void UpdateBlindPrimitive\(CEffectPrim\* prim\)\s*\{(.*?)\n\}", rag, re.S)
blind_apply = re.search(r"void CRagEffect::ApplyBlind\(int level\)\s*\{(.*?)\n\}", rag, re.S)
if not blind_update or "prim->m_master->EffectPosition()" not in blind_update.group(1):
    fail("UpdateBlindPrimitive must follow the parent effect position")
if not blind_apply:
    fail("ApplyBlind implementation not found")
blind_body = blind_apply.group(1)
for before, after in (("m_effectPos = parentPosition;", "if (m_stateCnt != 0)"),
                      ("LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeBlind)", "m_effectPos.y -= 9.0f;")):
    if before not in blind_body or after not in blind_body or blind_body.index(before) >= blind_body.index(after):
        fail(f"ApplyBlind must preserve Ref order: {before} before {after}")
init_start = rag.find("void CRagEffect::Init(")
world_start = rag.find("void CRagEffect::InitWorld(", init_start)
if init_start < 0 or world_start < 0 or "m_handler = Handler::EzStr" in rag[init_start:world_start]:
    fail("skill Init must not assign the generic EzStr handler")
print(f"dispatcher source check passed: 416 / 73 / 204 cases; {len(unported)} explicit unported routes")
