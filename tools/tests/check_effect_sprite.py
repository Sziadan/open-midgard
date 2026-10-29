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
LIFECYCLE = (ROOT / "tools/tests/EffectLifecycleTests.cpp").read_text(encoding="utf-8", errors="replace")
TEST_CMAKE = (ROOT / "tools/tests/CMakeLists.txt").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
DISPATCH = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")
CATALOG = (ROOT / "src/world/RagEffectCatalog.generated.inc").read_text(encoding="utf-8")
STR_TABLE = (ROOT / "src/world/RagEffectStrNameTable.generated.inc").read_text(encoding="utf-8")
image = ExeImage()


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
if ('extract_body(effect_sprite_handler "${effect_source}" "bool CRagEffect::ProcessEffectSprite(int spriteVariant)" "bool CRagEffect::ProcessEffectSpriteAndStr()")' not in TEST_CMAKE
        or 'ProcessEffectSprite(int spriteVariant)" "bool CRagEffect::ProcessEffectSpriteProduction(int spriteVariant)' not in TEST_CMAKE):
    raise AssertionError("production Effect_SPR helper is not extracted for the lifecycle fixture")
tatami_fixture = re.search(r"CRenderObject master;master\.m_pos=\{31,41,51\};\s*CRagEffect effect;effect\.m_type=631;(.*?)(?=\n    })", LIFECYCLE, re.S)
if tatami_fixture is None or "ProcessEffectSpriteProduction(16)" not in tatami_fixture.group(1):
    raise AssertionError("effect631 direct production variant16 lifecycle fixture disappeared")
for token in (r"\xC0\xCC\xC6\xD1\xC6\xAE\\\xB4\xD9\xB4\xD9\xB9\xCC.spr",
              r"\xC0\xCC\xC6\xD1\xC6\xAE\\\xB4\xD9\xB4\xD9\xB9\xCC.act",
              "PP_3DPARTICLE", "m_deltaPos2.y==-6.0f", "m_renderFlag==1", "m_animSpeed==6",
              "m_numSegments==1", "rngDraws==draws", "waveCalls.empty()",
              "without a second particle, resource request, cue or RNG draw"):
    if token not in tatami_fixture.group(1):
        raise AssertionError(f"effect631 lifecycle assertion missing {token}")
variant19_fixture = re.search(r"CRenderObject master;master\.m_pos=\{61,71,81\};\s*CRagEffect effect;effect\.m_type=634;(.*?)(?=\n    })", LIFECYCLE, re.S)
if variant19_fixture is None or "ProcessEffectSpriteProduction(19)" not in variant19_fixture.group(1):
    raise AssertionError("effect634 direct production variant19 lifecycle fixture disappeared")
for token in (r"\xC0\xCC\xC6\xD1\xC6\xAE\\\xC8\xAD\xBF\xB0\xC1\xF8.spr",
              r"\xC0\xCC\xC6\xD1\xC6\xAE\\\xC8\xAD\xBF\xB0\xC1\xF8.act",
              "PP_3DPARTICLE", "m_deltaPos2.y==-1.0f", "m_renderFlag==1", "m_animSpeed==7",
              "m_repeatAnim", "rngDraws==draws+1", "waveCalls.empty()",
              "without duplicate particle, resource, wave or RNG"):
    if token not in variant19_fixture.group(1):
        raise AssertionError(f"effect634 lifecycle assertion missing {token}")
effect632_fixture = re.search(r"struct Effect632Case \{ int rngValue; int action; \};(.*?)(?=\n    })", LIFECYCLE, re.S)
if effect632_fixture is None or "ProcessEffectSpriteProduction(17)" not in effect632_fixture.group(1):
    raise AssertionError("effect632 direct production variant17 lifecycle fixture disappeared")
for name, expected in (("expectedSpr", image.read_cstring_at(0x71D030).raw),
                       ("expectedAct", image.read_cstring_at(0x71D01C).raw),
                       ("expectedWave", image.read_cstring_at(0x719BF4).raw)):
    literal = re.search(rf"const std::string {name}=(\"(?:\\.|[^\"\\])*\")", effect632_fixture.group(1))
    if literal is None or ast.literal_eval(literal.group(1)).encode("latin-1") != expected:
        raise AssertionError(f"effect632 fixture {name} differs from its PE-backed bytes")
for token in ("const Effect632Case cases[]={{0,2},{1,0}}", "PP_3DPARTICLE",
              "m_duration==91", "m_renderFlag==0x201", "m_animSpeed==4", "m_spriteAction==item.action",
              "m_numSegments==1", "rngDraws==draws+1", "waveCalls.size()==1",
              "waveCalls[0].name==expectedWave", "waveCalls[0].x==190", "waveCalls[0].y==0",
              "waveCalls[0].z==380", "without repeat particle, resource, wave or RNG"):
    if token not in effect632_fixture.group(1):
        raise AssertionError(f"effect632 lifecycle assertion missing {token}")
effect633_fixture = re.search(r"CRenderObject master,listener;master\.m_pos=\{310,420,530\};listener\.m_pos=\{10,80,30\};\s*CRenderObject\* oldPlayer=g_world\.m_player;g_world\.m_player=&listener;\s*CRagEffect effect;effect\.m_type=633;(.*?)(?=\n    })", LIFECYCLE, re.S)
if effect633_fixture is None or "ProcessEffectSpriteProduction(18)" not in effect633_fixture.group(1):
    raise AssertionError("effect633 direct production variant18 lifecycle fixture disappeared")
for name, expected, required_va in (("expectedSpr", image.read_cstring_at(0x71D00C).raw, 0x71D00C),
                                   ("expectedAct", image.read_cstring_at(0x71CFFC).raw, 0x71CFFC),
                                   ("expectedWave", image.read_cstring_at(0x719C08).raw, 0x719C08)):
    literal = re.search(rf"const std::string {name}=(\"(?:\\.|[^\"\\])*\")", effect633_fixture.group(1))
    if literal is None or ast.literal_eval(literal.group(1)).encode("latin-1") != expected:
        raise AssertionError(f"effect633 fixture {name} differs from PE bytes at {required_va:#x}")
for token in ("PP_3DPARTICLE", "m_duration==87", "m_deltaPos2.y==-6.0f", "m_renderFlag==0x201",
              "m_pattern==0x200", "m_animSpeed==2", "m_spriteAction==0", "m_numSegments==1",
              "rngDraws==draws&&rngCursor==0", "waveCalls.size()==1", "waveCalls[0].name==expectedWave",
              "waveCalls[0].x==300", "waveCalls[0].y==0", "waveCalls[0].z==500",
              "without duplicate particle, resource, wave or RNG"):
    if token not in effect633_fixture.group(1):
        raise AssertionError(f"effect633 lifecycle assertion missing {token}")
effect630_fixture = re.search(r"struct Effect630Case \{ int rngValue; int action; \};(.*?)(?=\n    })", LIFECYCLE, re.S)
if effect630_fixture is None or "ProcessEffectSpriteProduction(15)" not in effect630_fixture.group(1):
    raise AssertionError("effect630 direct production variant15 lifecycle fixture disappeared")
for name, expected, required_va in (("expectedSpr", image.read_cstring_at(0x71D084).raw, 0x71D084),
                                   ("expectedAct", image.read_cstring_at(0x71D06C).raw, 0x71D06C),
                                   ("expectedWave", image.read_cstring_at(0x719BDC).raw, 0x719BDC)):
    literal = re.search(rf"const std::string {name}=(\"(?:\\.|[^\"\\])*\")", effect630_fixture.group(1))
    if literal is None or ast.literal_eval(literal.group(1)).encode("latin-1") != expected:
        raise AssertionError(f"effect630 fixture {name} differs from PE bytes at {required_va:#x}")
for token in ("const Effect630Case cases[]={{0,4},{1,0}}", "PP_3DPARTICLE",
              "m_duration==83", "m_deltaPos2.y==0", "m_renderFlag==0x201", "m_animSpeed==4",
              "m_repeatAnim", "m_spriteAction==item.action", "m_numSegments==1", "rngDraws==draws+1",
              "waveCalls.size()==1", "waveCalls[0].name==expectedWave", "waveCalls[0].x==100",
              "waveCalls[0].y==0", "waveCalls[0].z==300",
              "without duplicate particle, resource, wave or RNG"):
    if token not in effect630_fixture.group(1):
        raise AssertionError(f"effect630 lifecycle assertion missing {token}")
effect637_fixture = re.search(r"CRenderObject master,listener;master\.m_pos=\{100,200,300\};listener\.m_pos=\{10,70,20\};\s*CRenderObject\* oldPlayer=g_world\.m_player;g_world\.m_player=&listener;\s*CRagEffect effect;effect\.m_type=637;(.*?)(?=\n    })", LIFECYCLE, re.S)
if effect637_fixture is None or "ProcessEffectSpriteProduction(20)" not in effect637_fixture.group(1):
    raise AssertionError("effect637 direct production variant20 lifecycle fixture disappeared")
for name, expected, required_va in (("expectedSpr", image.read_cstring_at(0x71CFBC).raw, 0x71CFBC),
                                   ("expectedAct", image.read_cstring_at(0x71CFA4).raw, 0x71CFA4),
                                   ("expectedWave", image.read_cstring_at(0x719B40).raw, 0x719B40)):
    literal = re.search(rf"const std::string {name}=(\"(?:\\.|[^\"\\])*\")", effect637_fixture.group(1))
    if literal is None or ast.literal_eval(literal.group(1)).encode("latin-1") != expected:
        raise AssertionError(f"effect637 fixture {name} differs from PE bytes at {required_va:#x}")
for token in ("PP_3DPARTICLE", "m_duration==95", "m_renderFlag==0x201", "m_pattern==0x200",
              "m_animSpeed==4", "m_numSegments==1", "rngDraws==draws", "waveCalls.empty()",
              "effect.m_stateCnt=1", "effect.m_stateCnt=10", "effect.m_stateCnt=11",
              "waveCalls[0].name==expectedWave", "waveCalls[0].x==90", "waveCalls[0].y==0",
              "waveCalls[0].z==280", "without spawning"):
    if token not in effect637_fixture.group(1):
        raise AssertionError(f"effect637 lifecycle assertion missing {token}")
effect638_fixture = re.search(r"CRenderObject master;master\.m_pos=\{55,66,77\};\s*CRagEffect effect;effect\.m_type=638;(.*?)(?=\n    })", LIFECYCLE, re.S)
if effect638_fixture is None or "ProcessEffectSpriteProduction(21)" not in effect638_fixture.group(1):
    raise AssertionError("effect638 direct production variant21 lifecycle fixture disappeared")
for name, expected, required_va in (("expectedSpr", image.read_cstring_at(0x71CF88).raw, 0x71CF88),
                                   ("expectedAct", image.read_cstring_at(0x71CF6C).raw, 0x71CF6C)):
    literal = re.search(rf"const std::string {name}=(\"(?:\\.|[^\"\\])*\")", effect638_fixture.group(1))
    if literal is None or ast.literal_eval(literal.group(1)).encode("latin-1") != expected:
        raise AssertionError(f"effect638 fixture {name} differs from PE bytes at {required_va:#x}")
for token in ("PP_3DPARTICLE", "m_duration==63", "m_deltaPos2.y==-1.0f", "m_renderFlag==1",
              "m_pattern==0x200", "m_animSpeed==2", "m_repeatAnim", "m_spriteAction==0",
              "m_numSegments==1", "rngDraws==draws", "waveCalls.empty()",
              "without duplicate particle, resources, sound or RNG"):
    if token not in effect638_fixture.group(1):
        raise AssertionError(f"effect638 lifecycle assertion missing {token}")
effect639_fixture = re.search(r"CRenderObject master;master\.m_pos=\{65,76,87\};\s*CRagEffect effect;effect\.m_type=639;(.*?)(?=\n    })", LIFECYCLE, re.S)
if effect639_fixture is None or "ProcessEffectSpriteProduction(22)" not in effect639_fixture.group(1):
    raise AssertionError("effect639 direct production variant22 lifecycle fixture disappeared")
for name, expected, required_va in (("expectedSpr", image.read_cstring_at(0x71CF50).raw, 0x71CF50),
                                   ("expectedAct", image.read_cstring_at(0x71CF34).raw, 0x71CF34)):
    literal = re.search(rf"const std::string {name}=(\"(?:\\.|[^\"\\])*\")", effect639_fixture.group(1))
    if literal is None or ast.literal_eval(literal.group(1)).encode("latin-1") != expected:
        raise AssertionError(f"effect639 fixture {name} differs from PE bytes at {required_va:#x}")
for token in ("PP_3DPARTICLE", "m_duration==64", "m_deltaPos2.y==-1.0f", "m_renderFlag==1",
              "m_pattern==0x200", "m_animSpeed==2", "m_repeatAnim", "m_spriteAction==0",
              "m_numSegments==1", "rngDraws==draws", "waveCalls.empty()",
              "effect.m_stateCnt=1", "without duplicate particle, resources, sound or RNG"):
    if token not in effect639_fixture.group(1):
        raise AssertionError(f"effect639 lifecycle assertion missing {token}")
effect640_642_fixture = re.search(r"EffectSpriteCase cases\[\]=\{(.*?)\n        };(.*?)(?=\n    })", LIFECYCLE, re.S)
if effect640_642_fixture is None or "ProcessEffectSpriteProduction(item.variant)" not in effect640_642_fixture.group(2):
    raise AssertionError("effects640-642 table-driven production Effect_SPR lifecycle fixture disappeared")
asset_vas_by_effect = {
    640: (0x71CF1C, 0x71CF04),
    641: (0x71CEEC, 0x71CED4),
    642: (0x71CEBC, 0x71CEA4),
}
fixture_rows = {}
for match in re.finditer(r'\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*("(?:\\.|[^"\\])*")\s*,\s*("(?:\\.|[^"\\])*")\s*\}', effect640_642_fixture.group(1)):
    effect_id, variant = int(match.group(1)), int(match.group(2))
    fixture_rows[effect_id] = (variant, ast.literal_eval(match.group(4)).encode("latin-1"),
                               ast.literal_eval(match.group(5)).encode("latin-1"))
if set(fixture_rows) != set(asset_vas_by_effect) or {e: fixture_rows[e][0] for e in fixture_rows} != {640: 23, 641: 24, 642: 25}:
    raise AssertionError("effects640-642 lifecycle table must cover routes 640→23, 641→24, and 642→25")
for effect_id, (spr_va, act_va) in asset_vas_by_effect.items():
    _, spr, act = fixture_rows[effect_id]
    if (spr, act) != (image.read_cstring_at(spr_va).raw, image.read_cstring_at(act_va).raw):
        raise AssertionError(f"effect{effect_id} fixture resources differ from PE bytes at {spr_va:#x}/{act_va:#x}")
for token in ("item.effect", "m_deltaPos2.y==-1.0f", "m_renderFlag==1", "m_pattern==0x200",
              "m_animSpeed==2", "m_repeatAnim", "m_numSegments==1", "rngDraws==draws",
              "waveCalls.empty()", "effect.m_stateCnt=1", "without repeated output"):
    if token not in effect640_642_fixture.group(2):
        raise AssertionError(f"effects640-642 lifecycle assertion missing {token}")
effect643_fixture = re.search(r"CRenderObject master,listener;master\.m_pos=\{300,400,500\};listener\.m_pos=\{100,900,200\};(.*?)(?=\n    })", LIFECYCLE, re.S)
if effect643_fixture is None or "effect.m_type=643" not in effect643_fixture.group(1) or "ProcessEffectSpriteProduction(26)" not in effect643_fixture.group(1):
    raise AssertionError("effect643 direct production variant26 lifecycle fixture disappeared")
for name, expected, required_va in (("expectedSpr", image.read_cstring_at(0x71CE8C).raw, 0x71CE8C),
                                   ("expectedAct", image.read_cstring_at(0x71CE74).raw, 0x71CE74),
                                   ("expectedWave", image.read_cstring_at(0x719BB0).raw, 0x719BB0)):
    literal = re.search(rf"const std::string {name}=(\"(?:\\.|[^\"\\])*\")", effect643_fixture.group(1))
    if literal is None or ast.literal_eval(literal.group(1)).encode("latin-1") != expected:
        raise AssertionError(f"effect643 fixture {name} differs from PE bytes at {required_va:#x}")
for token in ("PP_3DPARTICLE", "m_duration==75", "m_deltaPos2.y==0.0f", "m_renderFlag==0x201",
              "m_pattern==0x200", "m_animSpeed==4", "!prim->m_repeatAnim", "m_numSegments==1",
              "effect.m_stateCnt=0", "effect.m_stateCnt=1", "effect.m_stateCnt=20", "effect.m_stateCnt=21",
              "waveCalls.empty()", "waveCalls.size()==1", "waveCalls[0].x==200", "waveCalls[0].y==0",
              "waveCalls[0].z==300", "waveCalls[0].volume==250", "waveCalls[0].range==40",
              "waveCalls[0].frequency==1", "rngDraws==draws", "without spawning"):
    if token not in effect643_fixture.group(1):
        raise AssertionError(f"effect643 lifecycle assertion missing {token}")
for label, dispatch in (("PE", pe_dispatch), ("Ref", ref_dispatch)):
    caller = re.search(r"case\s+0x283:(.*?)(?=case\s+0x[0-9A-F]+:|$)", dispatch, re.S)
    if caller is None:
        raise AssertionError(f"{label} effect643 caller branch is missing")
    branch = caller.group(1)
    ordered = ["this->m_stateCnt == 20", "this->m_pos = this->m_master->m_pos",
               "PlayWave(aEffect_146", "Effect_SPR(this, 26)", "return 1"]
    offsets = [branch.find(token) for token in ordered]
    if any(offset < 0 for offset in offsets) or offsets != sorted(offsets):
        raise AssertionError(f"{label} effect643 delayed state20 position/wave/variant order changed")
    if not re.search(r"PlayWave\([^,]+,\s*\w+,\s*0\.0,\s*\w+,\s*250,\s*40,\s*1\.0\)", branch):
        raise AssertionError(f"{label} effect643 wave coordinates/parameters changed")
effect647_fixture = re.search(r"CRenderObject master;master\.m_pos=\{74,85,96\};\s*CRagEffect effect;effect\.m_type=647;(.*?)(?=\n    })", LIFECYCLE, re.S)
if effect647_fixture is None or "ProcessEffectSpriteProduction(29)" not in effect647_fixture.group(1):
    raise AssertionError("effect647 direct production variant29 lifecycle fixture disappeared")
for name, expected, required_va in (("expectedSpr", image.read_cstring_at(0x71CE08).raw, 0x71CE08),
                                   ("expectedAct", image.read_cstring_at(0x71CDF4).raw, 0x71CDF4)):
    literal = re.search(rf"const std::string {name}=(\"(?:\\.|[^\"\\])*\")", effect647_fixture.group(1))
    if literal is None or ast.literal_eval(literal.group(1)).encode("latin-1") != expected:
        raise AssertionError(f"effect647 fixture {name} differs from PE bytes at {required_va:#x}")
for token in ("PP_3DPARTICLE", "m_duration==73", "m_deltaPos2.y==0.0f", "m_renderFlag==0x201",
              "m_pattern==0x200", "m_animSpeed==2", "!prim->m_repeatAnim", "m_spriteAction==0",
              "m_numSegments==1", "rngDraws==draws", "waveCalls.empty()", "effect.m_stateCnt=1",
              "without duplicate particle, resources, sound or RNG"):
    if token not in effect647_fixture.group(1):
        raise AssertionError(f"effect647 lifecycle assertion missing {token}")
effect644_fixture = re.search(r"CRagEffect effect;effect\.m_type=644;(.*?)(?=\n    })", LIFECYCLE, re.S)
if effect644_fixture is None or "ProcessEffectSpriteProduction(27)" not in effect644_fixture.group(1):
    raise AssertionError("effect644 direct production variant27 lifecycle fixture disappeared")
for name, expected, required_va in (("expectedSpr", image.read_cstring_at(0x71CE5C).raw, 0x71CE5C),
                                   ("expectedAct", image.read_cstring_at(0x71CE44).raw, 0x71CE44),
                                   ("expectedWave", image.read_cstring_at(0x719B98).raw, 0x719B98)):
    literal = re.search(rf"const std::string {name}=(\"(?:\\.|[^\"\\])*\")", effect644_fixture.group(1))
    if literal is None or ast.literal_eval(literal.group(1)).encode("latin-1") != expected:
        raise AssertionError(f"effect644 fixture {name} differs from PE bytes at {required_va:#x}")
for token in ("PP_3DPARTICLE", "m_duration==74", "m_deltaPos2.y==0.0f", "m_renderFlag==0x201",
              "m_pattern==0x200", "m_animSpeed==2", "!prim->m_repeatAnim", "m_numSegments==1",
              "waveCalls.size()==1", "waveCalls[0].x==200", "waveCalls[0].y==0", "waveCalls[0].z==300",
              "waveCalls[0].volume==250", "waveCalls[0].range==40", "waveCalls[0].frequency==1",
              "rngDraws==draws", "effect.m_stateCnt=1", "without repeated particle, resources, wave or RNG"):
    if token not in effect644_fixture.group(1):
        raise AssertionError(f"effect644 lifecycle assertion missing {token}")
for label, dispatch in (("PE", pe_dispatch), ("Ref", ref_dispatch)):
    caller = re.search(r"case\s+0x284:(.*?)(?=case\s+0x[0-9A-F]+:|$)", dispatch, re.S)
    if caller is None:
        raise AssertionError(f"{label} effect644 caller branch is missing")
    branch = caller.group(1)
    ordered = ["if ( !this->m_stateCnt )", "this->m_pos = this->m_master->m_pos",
               "PlayWave(aEffect_15", "Effect_SPR(this, 27)", "return 1"]
    offsets = [branch.find(token) for token in ordered]
    if any(offset < 0 for offset in offsets) or offsets != sorted(offsets):
        raise AssertionError(f"{label} effect644 state0 position/wave/variant order changed")
effect645_fixture = re.search(r"const DirectionCase directions\[\]=\{(.*?)\};(.*?)(?=\n    })", LIFECYCLE, re.S)
if effect645_fixture is None or "effect.m_type=645" not in effect645_fixture.group(2) or "ProcessEffectSpriteProduction(28)" not in effect645_fixture.group(2):
    raise AssertionError("effect645 direct production variant28 lifecycle fixture disappeared")
direction_rows = [(float(dx[:-1]), float(dz[:-1]), int(action))
                  for dx, dz, action in re.findall(r"\{\s*(-?\d+\.\d+f)\s*,\s*(-?\d+\.\d+f)\s*,\s*(\d+)\s*\}", effect645_fixture.group(1))]
if direction_rows != [(1.0, 0.0, 0), (0.0, 1.0, 6), (-1.0, 0.0, 4), (0.0, -1.0, 2)]:
    raise AssertionError("effect645 direction fixture must cover +X/+Z/-X/-Z action mapping at camera longitude0")
for name, expected, required_va in (("expectedSpr", image.read_cstring_at(0x71CE30).raw, 0x71CE30),
                                   ("expectedAct", image.read_cstring_at(0x71CE1C).raw, 0x71CE1C)):
    literal = re.search(rf"const std::string {name}=(\"(?:\\.|[^\"\\])*\")", effect645_fixture.group(2))
    if literal is None or ast.literal_eval(literal.group(1)).encode("latin-1") != expected:
        raise AssertionError(f"effect645 fixture {name} differs from PE bytes at {required_va:#x}")
for token in ("testView.longitude=0.0f", "prim->m_spriteAction==direction.action", "PP_3DPARTICLE",
              "m_duration==76", "m_deltaPos2.y==0.0f", "m_renderFlag==0x201", "m_pattern==0x200",
              "m_animSpeed==4", "!prim->m_repeatAnim", "m_numSegments==1", "rngDraws==draws",
              "waveCalls.empty()", "effect.m_stateCnt=1", "without duplicate particle, resources, wave or RNG"):
    if token not in effect645_fixture.group(2):
        raise AssertionError(f"effect645 lifecycle assertion missing {token}")
for label, dispatch in (("PE", pe_dispatch), ("Ref", ref_dispatch)):
    caller = re.search(r"case\s+0x285:(.*?)(?=case\s+0x[0-9A-F]+:|$)", dispatch, re.S)
    if caller is None or not re.search(r"Effect_SPR\(this,\s*28\).*?return\s+1", caller.group(1), re.S):
        raise AssertionError(f"{label} effect645 must call variant28 and return")
    if "PlayWave" in caller.group(1):
        raise AssertionError(f"{label} effect645 has no caller wave")

# Priority 5P effect-sprite callers 630/632/633/634 and their RNG variants.
for effect_id, variant, wave_symbol in ((630, 15, "aEffect_155"),
                                        (632, 17, "aEffect_142"),
                                        (633, 18, "aEffect_6")):
    case_value = effect_id
    for label, dispatch in (("PE", pe_dispatch), ("Ref", ref_dispatch)):
        caller = re.search(rf"case\s+0x{case_value:X}:(.*?)(?=case\s+0x[0-9A-F]+:|$)", dispatch, re.S)
        if caller is None:
            raise AssertionError(f"{label} effect{effect_id} branch is missing")
        branch = caller.group(1)
        ordered = ["if ( !this->m_stateCnt )", "this->m_pos = this->m_master->m_pos",
                   f"PlayWave({wave_symbol}", f"Effect_SPR(this, {variant})", "return 1"]
        offsets = [branch.find(token) for token in ordered]
        if any(offset < 0 for offset in offsets) or offsets != sorted(offsets):
            raise AssertionError(f"{label} effect{effect_id} position/wave/variant order changed")
        if not re.search(r"PlayWave\([^,]+,\s*\w+,\s*0\.0,\s*\w+,\s*250,\s*40,\s*1\.0\)", branch):
            raise AssertionError(f"{label} effect{effect_id} wave coordinates/parameters changed")
for label, dispatch in (("PE", pe_dispatch), ("Ref", ref_dispatch)):
    caller = re.search(r"case\s+0x27A:(.*?)(?=case\s+0x[0-9A-F]+:|$)", dispatch, re.S)
    if caller is None or not re.search(r"Effect_SPR\(this,\s*19\).*?return\s+1", caller.group(1), re.S):
        raise AssertionError(f"{label} effect634 must call variant19 and return")
    if "PlayWave" in caller.group(1):
        raise AssertionError(f"{label} effect634 has no caller wave")

for variant, next_variant, action_label, action_value in ((15, 16, "LABEL_39", 4),
                                                           (17, 18, "LABEL_44", 2)):
    branch_match = re.search(rf"case\s+{variant}:(.*?)(?=case\s+{next_variant}:)", pe_sprite, re.S)
    if branch_match is None:
        raise AssertionError(f"PE Effect_SPR({variant}) branch is missing")
    branch = branch_match.group(1)
    for token in ("m_animSpeed = 4", "rand() & 0x80000001", "== 0",
                  "< 0 )", "goto LABEL_74", f"goto {action_label}"):
        if token not in branch:
            raise AssertionError(f"PE Effect_SPR({variant}) RNG/action branch lacks {token}")
    label_branch = re.search(rf"{action_label}:.*?SetAction\(g_prim,\s*{action_value},\s*16,\s*1\)", pe_sprite, re.S)
    zero_branch = re.search(r"LABEL_74:.*?SetAction\(g_prim,\s*0,\s*16,\s*1\)", pe_sprite, re.S)
    if label_branch is None or zero_branch is None:
        raise AssertionError(f"PE Effect_SPR({variant}) action destinations changed")
for variant, next_variant in ((18, 19), (19, 20)):
    branch_match = re.search(rf"case\s+{variant}:(.*?)(?=case\s+{next_variant}:)", pe_sprite, re.S)
    if branch_match is None:
        raise AssertionError(f"PE Effect_SPR({variant}) branch is missing")
    branch = branch_match.group(1)
    if variant == 18:
        for token in ("m_deltaPos2.y = -6.0", "m_animSpeed = 2", "byte_71D00C",
                      "byte_71CFFC", "goto LABEL_69"):
            if token not in branch:
                raise AssertionError(f"PE Effect_SPR(18) lacks {token}")
        if "rand()" in branch:
            raise AssertionError("PE Effect_SPR(18) has no RNG")
    else:
        for token in ("m_deltaPos2.y = -1.0", "m_renderFlag = 1", "rand() % 5 + 3",
                      "m_repeatAnim = 1", "byte_71CFE8", "byte_71CFD4",
                      "SetAction(g_prim, 0, 16, 1)", "InitSegment(g_prim)"):
            if token not in branch:
                raise AssertionError(f"PE Effect_SPR(19) lacks {token}")

for variant in (15, 17):
    if "rand() & 0x80000001" not in helper or "alternate = value == 0" not in helper:
        raise AssertionError(f"local Effect_SPR({variant}) RNG predicate changed")
if "prim->m_spriteAction = spriteVariant == 15 ? 4 : 2" not in helper:
    raise AssertionError("local Effect_SPR(15/17) random action mapping changed")
if "if (spriteVariant == 19) prim->m_animSpeed = rand() % 5 + 3" not in helper:
    raise AssertionError("local Effect_SPR(19) one-draw random animation speed changed")
if "if (spriteVariant == 19) prim->m_deltaPos2.y = -1.0f" not in helper:
    raise AssertionError("local Effect_SPR(19) canonical Y offset changed")
local_action_rng = re.search(r"if \(spriteVariant == 15 \|\| spriteVariant == 17\) \{(.*?)\n\s*\}", helper, re.S)
if local_action_rng is None or local_action_rng.group(1).count("rand()") != 1:
    raise AssertionError("local Effect_SPR(15/17) must consume one random action draw per emission")
local_variant18 = re.search(r"\{\s*18\s*,[^\n]*,\s*2,\s*false,\s*nullptr\s*\}", helper)
local_variant19 = re.search(r"\{\s*19\s*,[^\n]*,\s*2,\s*true,\s*nullptr\s*\}", helper)
if local_variant18 is None or local_variant19 is None:
    raise AssertionError("local Effect_SPR(18/19) speed/repeat/sound table changed")
local_wave_gate = helper.find("if (callerWave != nullptr && m_stateCnt == callerWaveFrame)")
local_state_gate = helper.find("if (m_stateCnt != spawnState)", local_wave_gate)
local_prim_launch = helper.find("LaunchEffectPrim(PP_3DPARTICLE", local_state_gate)
if not (0 <= local_wave_gate < local_state_gate < local_prim_launch):
    raise AssertionError("local Effect_SPR caller wave must precede state gate and primitive launch")
if not re.search(r"case\s+612:\s+return ProcessEffectSpriteAndStr\(\);", DISPATCH):
    raise AssertionError("effect 612 must route through Effect_SPR(14) and its shared STR continuation")
ref_612 = re.search(r"case\s+0x264:(.*?)(?=case\s+0x265:)", ref_dispatch, re.S)
if ref_612 is None or not re.search(r"Effect_SPR\(this,\s*14\).*?goto\s+\$L146219", ref_612.group(1), re.S):
    raise AssertionError("Ref effect 612 no longer calls Effect_SPR(14) before the shared STR label")
pe_612 = re.search(r"case\s+0x264:(.*?)(?=case\s+0x265:)", pe_dispatch, re.S)
if pe_612 is None or not re.search(r"Effect_SPR\(this,\s*14\).*?goto\s+\$L146219", pe_612.group(1), re.S):
    raise AssertionError("PE effect 612 caller ordering or shared-tail target changed")
pe_variant14 = re.search(r"case\s+14:(.*?)(?=case\s+15:)", pe_sprite, re.S)
if pe_variant14 is None or not all(token in pe_variant14.group(1) for token in
        ("SetSprName(g_prim, &byte_71D0B0)", "SetActName(g_prim, &byte_71D09C)", "InitSegment(g_prim)")):
    raise AssertionError("PE Effect_SPR variant14 resource/action setup changed")
if "PlayWave" in pe_variant14.group(1):
    raise AssertionError("PE Effect_SPR variant14 has no wave cue")
if not re.search(r"\{\s*14\s*,[^\n]*,\s*2,\s*false,\s*nullptr\s*\}", helper):
    raise AssertionError("local Effect_SPR(14) must use speed2, one-shot animation and no wave")
str_label = re.search(r"\$L146219:\s*CRagEffect::ProcessEZ2STR\(this,\s*0\.0,\s*-80\.0,\s*0\.0\)", REF)
if str_label is None:
    raise AssertionError("Ref effect 612 shared STR continuation changed")
if not re.search(r"\$L146219:\s*CRagEffect::ProcessEZ2STR\(this,\s*0\.0,\s*-80\.0,\s*0\.0\);\s*return 1", REF):
    raise AssertionError("Ref effect 612 must ignore STR completion and return true")
if not re.search(r"ProcessEZ2STR\(this,\s*0\.0,\s*-80\.0,\s*0\.0\);\s*return 1", pe_dispatch):
    raise AssertionError("PE effect 612 shared STR continuation must ignore completion and return true")
pe_init = last_body(PE, r"__thiscall CRagEffect::Init\(CRagEffect \*this, CRenderObject \*master, int effectId, vector3d fDeltaPos\)")
ref_init = body(REF, "void __thiscall CRagEffect::Init(CRagEffect *this, CRenderObject *master, int effectId, vector3d fDeltaPos)")
for label, init in (("PE", pe_init), ("Ref", ref_init)):
    if not re.search(r"case\s+612:\s*\$L147927_0:\s*sprintf\(StrName,\s*aItempokjukStr\)", init):
        raise AssertionError(f"{label} Init effect 612 no longer selects itempokjuk.str")
image_name = image.read_cstring_at(0x71A804)
if image_name is None or image_name.raw != b"itempokjuk.str":
    raise AssertionError("PE STR bytes at VA 0x71A804 no longer resolve to itempokjuk.str")
if not re.search(r'\{\s*612,\s*"itempokjuk\.str"', CATALOG):
    raise AssertionError("local effect catalog lost effect612 itempokjuk.str mapping")
if not re.search(r'\{\s*612,\s*"itempokjuk\.str"', STR_TABLE):
    raise AssertionError("local STR name table lost effect612 itempokjuk.str mapping")
sprite_and_str = body(RAG, "bool CRagEffect::ProcessEffectSpriteAndStr()")
if not re.search(r"ProcessEffectSprite\(14\);\s*ProcessEZ2STR\(0\.0f,\s*-80\.0f,\s*0\.0f\);\s*return true;", sprite_and_str):
    raise AssertionError("local effect 612 must emit variant14, ignore STR completion, and return true")

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

print(f"Effect_SPR source check passed: {len(expected_routes)} caller routes, {len(asset_vas)} PE-backed assets, {len(wave_vas)} caller waves; effects 584/612, 630/632-634, 631 and 682-686 verified")
