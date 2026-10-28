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


helper = body(RAG, "bool CRagEffect::ProcessEffectSprite(int spriteVariant)")
ref_helper = body((ROOT / "Ref/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace"),
                  "void __thiscall CRagEffect::Effect_SPR(CRagEffect *this, char F1)")
ref_dispatch = body(REF, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")

expected_routes = {
    570: 0, 571: 1, 576: 2, 577: 3, 578: 4, 579: 5, 580: 6, 583: 7,
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

asset_vas = {
    0: (0x71D338, 0x71D324), 1: (0x71D2F4, 0x71D2DC),
    2: (0x71D2C4, 0x71D2AC), 3: (0x71D294, 0x71D27C),
    4: (0x71D264, 0x71D24C), 5: (0x71D238, 0x71D224),
    6: (0x71D20C, 0x71D1F4), 7: (0x71D1C8, 0x71D1B4),
    9: (0x71D150, 0x71D13C), 10: (0x71D178, 0x71D164),
    11: (0x71D128, 0x71D114), 12: (0x71D100, 0x71D0EC),
    13: (0x71D0D8, 0x71D0C4), 15: (0x71D084, 0x71D06C),
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

print(f"Effect_SPR source check passed: {len(expected_routes)} caller routes, {len(asset_vas)} PE-backed assets, {len(wave_vas)} PE-backed caller waves")
