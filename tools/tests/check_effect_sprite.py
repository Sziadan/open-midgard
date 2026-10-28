#!/usr/bin/env python3
"""Check routed Effect_SPR callers and byte-backed Ref resources."""
from __future__ import annotations

import ast
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
RAG = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
DISPATCH = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")


def body(source: str, signature: str) -> str:
    start = source.find(signature)
    if start < 0:
        raise AssertionError(f"missing function {signature}")
    opening = source.find("{", start + len(signature))
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:index]
    raise AssertionError(f"unterminated function {signature}")


def last_body(source: str, signature: str) -> str:
    matches = list(re.finditer(signature, source))
    if not matches:
        raise AssertionError(f"missing function {signature}")
    opening = source.find("{", matches[-1].end())
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:index]
    raise AssertionError(f"unterminated function {signature}")


helper = body(RAG, "bool CRagEffect::ProcessEffectSprite(int spriteVariant)")
ref_helper = body((ROOT / "Ref/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace"),
                  "void __thiscall CRagEffect::Effect_SPR(CRagEffect *this, char F1)")
ref_dispatch = body(REF, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
pe_dispatch = last_body(PE, r"__thiscall CRagEffect::OnProcess3\(CRagEffect \*this\)")

expected_routes = {
    570: 0, 571: 1, 576: 2, 577: 3, 578: 4, 579: 5, 580: 6, 583: 7,
    584: 8,
    585: 9, 586: 10, 587: 11, 588: 12, 589: 13,
    630: 15, 631: 16, 632: 17, 633: 18, 634: 19, 637: 20,
    638: 21, 639: 22, 640: 23, 641: 24, 642: 25, 645: 28, 647: 29,
    643: 26, 644: 27, 648: 30, 666: 31, 682: 32, 683: 33, 684: 34, 685: 35, 686: 36,
}
for effect_id, variant in expected_routes.items():
    if not re.search(rf"case\s+{effect_id}:\s+return ProcessEffectSprite\({variant}\);", DISPATCH):
        raise AssertionError(f"effect {effect_id} does not route to Effect_SPR({variant})")
    case_value = effect_id
    if not re.search(rf"case\s+0x{case_value:X}:.*?Effect_SPR\(this,\s*{variant}\)", ref_dispatch, re.S):
        raise AssertionError(f"Ref OnProcess3 does not call Effect_SPR({variant}) for {effect_id}")

# Priority 5P source comparison: each fireworks effect reaches the shared
# Effect_SPR case, whose canonical tail initializes the sprite before sound.
pe_sprite = last_body(PE, r"void __thiscall CRagEffect::Effect_SPR\(CRagEffect \*this, char F1\)")
for effect_id, variant in ((682, 32), (683, 33), (684, 34), (685, 35), (686, 36)):
    case_value = effect_id
    pe_case = re.search(rf"case\s+0x{case_value:X}:(.*?)(?=case\s+0x[0-9A-F]+:|$)", pe_dispatch, re.S)
    ref_case = re.search(rf"case\s+0x{case_value:X}:(.*?)(?=case\s+0x[0-9A-F]+:|$)", ref_dispatch, re.S)
    if pe_case is None or ref_case is None:
        raise AssertionError(f"effect {effect_id} caller branch is missing from PE or Ref")
    for label, branch in (("PE", pe_case.group(1)), ("Ref", ref_case.group(1))):
        if not re.search(rf"Effect_SPR\(this,\s*{variant}\).*?return\s+1", branch, re.S):
            raise AssertionError(f"{label} effect {effect_id} caller must call variant {variant}, then return")
    variant_case = re.search(rf"case\s+{variant}:(.*?)(?=case\s+\d+:|LABEL_81:)", pe_sprite, re.S)
    if variant_case is None:
        raise AssertionError(f"PE Effect_SPR variant {variant} branch is missing")
    branch = variant_case.group(1)
    if ("m_animSpeed = 4" not in branch or "SetSprName" not in branch
            or "SetActName" not in branch
            or (variant != 36 and "goto LABEL_81" not in branch)):
        raise AssertionError(f"PE Effect_SPR variant {variant} setup changed")
shared_fireworks = re.search(r"LABEL_81:(.*?)(?=LABEL_82:)", pe_sprite, re.S)
if shared_fireworks is None:
    raise AssertionError("canonical Effect_SPR shared fireworks tail is missing")
fireworks_ops = ["SetAction(g_prim, 0, 16, 1)", "PlayWave(aEffectItempokj, 0.0, 0.0, 0.0, 250, 40, 1.0)", "goto LABEL_82"]
fireworks_positions = [shared_fireworks.group(1).find(token) for token in fireworks_ops]
if any(position < 0 for position in fireworks_positions) or fireworks_positions != sorted(fireworks_positions):
    raise AssertionError("PE fireworks action, sound, and segment-init order changed")

# Priority 5P Tatami route: no caller cue or random branch, just variant16.
for label, dispatch in (("PE", pe_dispatch), ("Ref", ref_dispatch)):
    tatami_caller = re.search(r"case\s+0x277:(.*?)(?=case\s+0x[0-9A-F]+:|$)", dispatch, re.S)
    if tatami_caller is None or not re.search(r"Effect_SPR\(this,\s*16\).*?return\s+1", tatami_caller.group(1), re.S):
        raise AssertionError(f"{label} effect631 caller must invoke Effect_SPR(16) and return")
tatami_variant = re.search(r"case\s+16:(.*?)(?=case\s+17:)", pe_sprite, re.S)
if tatami_variant is None:
    raise AssertionError("canonical Effect_SPR(16) branch is missing")
tatami_ops = ["m_deltaPos2.y = -6.0", "m_renderFlag = 1", "m_animSpeed = 6",
              "SetSprName(g_prim, &byte_71D058)", "SetActName(g_prim, &byte_71D044)",
              "SetAction(g_prim, 0, 16, 1)"]
tatami_positions = [tatami_variant.group(1).find(token) for token in tatami_ops]
if any(position < 0 for position in tatami_positions) or tatami_positions != sorted(tatami_positions):
    raise AssertionError("PE Effect_SPR(16) delta, render, speed, resource and action order changed")
if ("spriteVariant == 16 || spriteVariant == 18" not in helper
        or "if (spriteVariant == 16 || spriteVariant == 19" not in helper):
    raise AssertionError("local Effect_SPR(16) offset/render branches are missing")
tatami_asset = re.search(r"\{\s*16\s*,[^\n]*,\s*6,\s*false,\s*nullptr\s*\}", helper)
if tatami_asset is None:
    raise AssertionError("local Effect_SPR(16) must use speed6 one-shot animation with no sound")
if not re.search(r"case\s+612:\s+return ProcessEffectSpriteAndStr\(\);", DISPATCH):
    raise AssertionError("effect 612 must route through Effect_SPR(14) and its shared STR continuation")
ref_612 = re.search(r"case\s+0x264:(.*?)(?=case\s+0x265:)", ref_dispatch, re.S)
if ref_612 is None or not re.search(r"Effect_SPR\(this,\s*14\).*?goto\s+\$L146219", ref_612.group(1), re.S):
    raise AssertionError("Ref effect 612 no longer calls Effect_SPR(14) before the shared STR label")
str_label = re.search(r"\$L146219:\s*CRagEffect::ProcessEZ2STR\(this,\s*0\.0,\s*-80\.0,\s*0\.0\)", REF)
if str_label is None:
    raise AssertionError("Ref effect 612 shared STR continuation changed")
sprite_and_str = body(RAG, "bool CRagEffect::ProcessEffectSpriteAndStr()")
if not re.search(r"ProcessEffectSprite\(14\);\s*return ProcessEZ2STR\(0\.0f,\s*-80\.0f,\s*0\.0f\);", sprite_and_str):
    raise AssertionError("local effect 612 continuation does not match Ref sprite then STR ordering")

asset_vas = {
    0: (0x71D338, 0x71D324), 1: (0x71D2F4, 0x71D2DC),
    2: (0x71D2C4, 0x71D2AC), 3: (0x71D294, 0x71D27C),
    4: (0x71D264, 0x71D24C), 5: (0x71D238, 0x71D224),
    6: (0x71D20C, 0x71D1F4), 7: (0x71D1C8, 0x71D1B4),
    9: (0x71D150, 0x71D13C), 10: (0x71D178, 0x71D164),
    11: (0x71D128, 0x71D114), 12: (0x71D100, 0x71D0EC),
    13: (0x71D0D8, 0x71D0C4), 14: (0x71D0B0, 0x71D09C),
    15: (0x71D084, 0x71D06C),
    8: (0x71D1A0, 0x71D18C),
    16: (0x71D058, 0x71D044), 17: (0x71D030, 0x71D01C),
    18: (0x71D00C, 0x71CFFC), 19: (0x71CFE8, 0x71CFD4),
    20: (0x71CFBC, 0x71CFA4), 21: (0x71CF88, 0x71CF6C),
    22: (0x71CF50, 0x71CF34), 23: (0x71CF1C, 0x71CF04),
    24: (0x71CEEC, 0x71CED4), 25: (0x71CEBC, 0x71CEA4),
    26: (0x71CE8C, 0x71CE74), 27: (0x71CE5C, 0x71CE44),
    28: (0x71CE30, 0x71CE1C), 29: (0x71CE08, 0x71CDF4),
    30: (0x71CDDC, 0x71CDC4),
    31: (0x71CDAC, 0x71CD94), 32: (0x71CD7C, 0x71CD64),
    33: (0x71CD48, 0x71CD2C), 34: (0x71CD10, 0x71CCF4),
    35: (0x71CCDC, 0x71CCC4), 36: (0x71CCA8, 0x71CC8C),
}
assets = {}
for match in re.finditer(r'^\s*\{\s*(\d+)\s*,\s*("(?:\\.|[^"\\])*")\s*,\s*("(?:\\.|[^"\\])*")', helper, re.M):
    assets[int(match.group(1))] = (ast.literal_eval(match.group(2)).encode("latin-1"),
                                   ast.literal_eval(match.group(3)).encode("latin-1"))
image = ExeImage()
fireworks_wave = image.read_cstring_at(0x7127E8)
if fireworks_wave is None or fireworks_wave.raw != b"effect\\itempokjuk.wav":
    raise AssertionError("Effect_SPR fireworks wave bytes at VA 0x7127E8 changed")
for variant in range(32, 37):
    row = re.search(rf"\{{\s*{variant}\s*,[^\n]*,\s*4,\s*false,\s*nullptr\s*\}}", helper)
    if row is None:
        raise AssertionError(f"local Effect_SPR({variant}) must use speed4, one-shot animation and shared sound")
resources = helper.find("prim->SetParticleResources(asset.spr, asset.act)")
sound_branch = helper.find("if (spriteVariant >= 32 && spriteVariant <= 36)")
sound_call = helper.find('PlayWave("effect\\\\itempokjuk.wav"', sound_branch)
segment_init = helper.find("InitReferenceSegments(prim)", sound_call)
if not (0 <= resources < sound_branch < sound_call < segment_init):
    raise AssertionError("local fireworks must bind resources, play the exact sound, then initialize segments")
for variant, (spr_va, act_va) in asset_vas.items():
    if variant not in assets:
        raise AssertionError(f"Effect_SPR({variant}) has no local asset entry")
    expected = tuple(image.read_cstring_at(va).raw for va in (spr_va, act_va))
    if assets[variant] != expected:
        raise AssertionError(f"Effect_SPR({variant}) assets differ from PE VAs {spr_va:#x}/{act_va:#x}")

wave_vas = {630: 0x719BDC, 632: 0x719BF4, 633: 0x719C08, 637: 0x719B40,
            643: 0x719BB0, 644: 0x719B98, 648: 0x719B80}
caller_wave_literals = {
    int(match.group(1)): ast.literal_eval(match.group(2)).encode("latin-1")
    for match in re.finditer(r'case\s+(\d+):\s+callerWave\s*=\s*("(?:\\.|[^"\\])*")', helper)
}
for effect_id, wave_va in wave_vas.items():
    if effect_id not in caller_wave_literals or caller_wave_literals[effect_id] != image.read_cstring_at(wave_va).raw:
        raise AssertionError(f"effect {effect_id} caller wave differs from resolved PE callsite VA {wave_va:#x}")

for token in ("m_stateCnt == 99999998", "m_renderFlag |= 0x200", "m_pattern |= 0x200",
              "m_numSegments = 1", "InitReferenceSegments(prim)", "effect\\\\itempokjuk.wav",
              "effect\\\\earth_quake.wav", "mode->SendMsg(135, 0, 0, 0)"):
    if token not in helper:
        raise AssertionError(f"missing shared Effect_SPR setup: {token}")
if "case 15:" not in ref_helper or "case 17:" not in ref_helper or "case 28:" not in ref_helper:
    raise AssertionError("Ref RNG/directional variants were not included in the comparison")
if "rand() & 0x80000001" not in helper or "rand() % 5 + 3" not in helper:
    raise AssertionError("RNG action/speed branches are missing")
asset_setup = helper.find("prim->SetParticleResources(asset.spr, asset.act)")
variant8_action = helper.find("if (spriteVariant == 8)", asset_setup)
if asset_setup < 0 or variant8_action < asset_setup:
    raise AssertionError("Effect_SPR(8) must bind PE-backed resources before choosing its camera-relative action")
for token in ("GetCameraLongitude()", "viewDirection >= 180 && viewDirection < 270", "action = 2",
              "viewDirection < 90", "action = 0", "viewDirection < 180", "action = 3",
              "prim->m_spriteAction = action"):
    if token not in helper[variant8_action:]:
        raise AssertionError(f"Effect_SPR(8) directional action mapping is incomplete: {token}")
ref_584 = re.search(r"case\s+0x248:(.*?)(?=case\s+0x249:)", ref_dispatch, re.S)
if ref_584 is None or not re.search(r"m_stateCnt\s*==\s*5.*?PlayWave\(aEffect_16", ref_584.group(1), re.S):
    raise AssertionError("Ref effect 584 caller wave is no longer verified at its inline callsite")
effect584_wave = image.read_cstring_at(0x6F1FF0).raw
if effect584_wave != b"effect\\":
    raise AssertionError("effect 584 aEffect_16 prefix bytes changed; full wave remains unresolved")
if not re.search(r"case\s+584:\s+callerWave\s*=\s*\"effect\\\\\";\s*callerWaveFrame\s*=\s*5", helper):
    raise AssertionError("effect 584 unresolved aEffect_16 prefix/frame5 cue is missing")

print(f"Effect_SPR source check passed: {len(expected_routes)} caller routes, {len(asset_vas)} PE-backed assets, {len(wave_vas)} caller waves; effects 584/612, 631 and 682-686 verified")
