#!/usr/bin/env python3
"""Compare EffectTextureSet selectors 2/4/14/100 and their callers."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage, SymbolTable  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
ROUTE = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")
LIFECYCLE = (ROOT / "tools/tests/EffectLifecycleTests.cpp").read_text(encoding="utf-8")
TEST_CMAKE = (ROOT / "tools/tests/CMakeLists.txt").read_text(encoding="utf-8")


def function_body(source, signature):
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:pos]
    raise AssertionError(f"unterminated function: {signature}")


for label, source in (("PE", PE), ("primary Ref", REF)):
    body = function_body(source, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
    match = re.search(r"case 0x22A:(.*?)(?=\n\s*case |\n\s*default:|\Z)", body, re.S)
    if match is None or not re.search(
            r"EffectTextureSet\(this,\s*aEffectFreezeAB,\s*2\);\s*return 1;", match.group(1)):
        raise AssertionError(f"{label} effect554 caller changed")

    match = re.search(r"case 0x2A4:(.*?)(?=\n\s*case |\n\s*default:|\Z)", body, re.S)
    if match is None or not re.search(
            r"EffectTextureSet\(this,\s*aEffectBlastMin,\s*4\);\s*"
            r"if\s*\(\s*this->m_stateCnt\s*!=\s*10\s*\).*?"
            r"this->m_pos\.x\s*=\s*v\d+->x;.*?this->m_pos\.z\s*=\s*v\d+->z;.*?"
            r"PlayWave\(aEffectEfDecagi,.*?250,\s*40,\s*1\.0\);\s*return 1;", match.group(1), re.S):
        raise AssertionError(f"{label} effect676 selector4/state10 cue caller changed")
    for effect_id, case_id, resource, selector in (
        (693, "0x2B5", "aEffectGlow01Bm", 14),
        (694, "0x2B6", "aEffectGlow02Bm", 14),
        (701, "0x2BD", "aEffectGlow11Bm", 14),
        (702, "0x2BE", "aEffectGlow12Bm", 14),
        (703, "0x2BF", "aEffectWhite02B", 100),
    ):
        match = re.search(rf"case {case_id}:(.*?)(?=\n\s*case |\n\s*default:|\Z)", body, re.S)
        if match is None or not re.search(
                rf"EffectTextureSet\(this,\s*{resource},\s*{selector}\);\s*return 1;", match.group(1)):
            raise AssertionError(f"{label} effect{effect_id} selector/resource caller changed")

if 'case 554: ApplyEffectTexture("effect\\\\freeze_a.bmp", 2); return true;' not in ROUTE:
    raise AssertionError("local effect554 generated route changed")
if 'm_type == 554) {\n        ApplyEffectTexture("effect\\\\freeze_a.bmp", 2);' not in LOCAL:
    raise AssertionError("local effect554 selector-2 mapping changed")
if "case 676: return ProcessTarotEffect();" not in ROUTE:
    raise AssertionError("local effect676 generated route changed")
effect_texture_set = function_body(LOCAL, "bool CRagEffect::ProcessEffectTextureSet()")
selector_cases = (
    (693, 'ApplyEffectTexture("effect\\\\glow01.bmp", 14); return true;'),
    (694, 'ApplyEffectTexture("effect\\\\glow02.bmp", 14); return true;'),
    (701, 'ApplyEffectTexture("effect\\\\glow11.bmp", 14); return true;'),
    (702, 'ApplyEffectTexture("effect\\\\glow12.bmp", 14); return true;'),
    (703, 'ApplyEffectTexture("effect\\\\white02.bmp", 100); return true;'),
)
for effect_id, body in selector_cases:
    if f"case {effect_id}: {body}" not in effect_texture_set:
        raise AssertionError(f"local effect{effect_id} EffectTextureSet mapping changed")
    if f"case {effect_id}: return ProcessEffectTextureSet();" not in ROUTE:
        raise AssertionError(f"local effect{effect_id} generated route changed")

process_tarot = function_body(LOCAL, "bool CRagEffect::ProcessTarotEffect()")
effect676 = re.search(r"case 676:(.*?)(?=\n\s*case |\Z)", process_tarot, re.S)
if effect676 is None:
    raise AssertionError("local effect676 ProcessTarotEffect branch missing")
for token in (
    'ApplyEffectTexture("effect\\\\blast_mine##clock.bmp", 4)', "if (m_stateCnt == 10)",
    'TryPlayEffectWaveAt(wavePos, "effect\\\\EF_DecAgility.wav")',
    "wavePos.y = mode->m_world->m_player->m_pos.y",
):
    if token not in effect676.group(1):
        raise AssertionError(f"local effect676 texture/sound behavior missing {token}")
if effect676.group(1).find("ApplyEffectTexture") > effect676.group(1).find("TryPlayEffectWaveAt"):
    raise AssertionError("effect676 texture setup must precede its state-10 sound")

symbols = SymbolTable(ExeImage())
resource = symbols.resolve_name("aEffectFreezeAB")
if resource is None or resource.raw != b"effect\\freeze_a.bmp":
    raise AssertionError("PE freeze_a resource bytes changed")
symbols_by_name = (("aEffectBlastMin", b"effect\\blast_mine##clock.bmp"),
                   ("aEffectEfDecagi", b"effect\\EF_DecAgility.wav"))
for name, expected in symbols_by_name:
    resolved = symbols.resolve_name(name)
    if resolved is None or resolved.raw != expected:
        raise AssertionError(f"PE effect676 resource bytes for {name} changed")
for name, expected in (
    ("aEffectGlow01Bm", b"effect\\glow01.bmp"),
    ("aEffectGlow02Bm", b"effect\\glow02.bmp"),
    ("aEffectGlow11Bm", b"effect\\glow11.bmp"),
    ("aEffectGlow12Bm", b"effect\\glow12.bmp"),
    ("aEffectWhite02B", b"effect\\white02.bmp"),
):
    resolved = symbols.resolve_name(name)
    if resolved is None or resolved.raw != expected:
        raise AssertionError(f"PE texture-set resource bytes for {name} changed")

texture = function_body(LOCAL, "void CRagEffect::ApplyEffectTexture(const char* textureName, int variant)")
for token in (
    "if (m_stateCnt != 0)", "LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeEffectTexture)",
    "prim->m_renderFlag = 5", "prim->m_duration = m_duration", "gi.life = 1", "gi.process = 0",
    "if (variant == 2)", "gi.distance = 10.0f", "gi.arcBaselineY = origin.y - 10.0f",
    "gi.flag[2] = 2", "gi.height[10] = 0.0f", "gi.fullDisplayAngle = 135",
    "gi.height[8] = 0.0f", "gi.alphaB = 0", "gi.sparkX = origin.x", "gi.sparkZ = origin.z",
    "gi.rotStart = static_cast<short>(rand() % 360)", "gi.flag[4] = static_cast<u8>(variant",
):
    if token not in texture:
        raise AssertionError(f"local selector-2 EffectTextureSet profile missing {token}")
for token in (
    "} else if (variant == 4)", "gi.distance = 6.0f", "gi.arcBaselineY = origin.y - 30.0f",
    "gi.flag[2] = 0", "gi.height[10] = 0.0f", "gi.fullDisplayAngle = 135",
):
    if token not in texture:
        raise AssertionError(f"local selector-4 EffectTextureSet profile missing {token}")

for token in (
    "} else if (variant == 14)", "gi.distance = m_param[0] > 0.0f ? m_param[0] : 30.0f",
    "gi.flag[2] = 4", "gi.fullDisplayAngle = static_cast<short>(rand() % 360)",
    "} else if (variant == 100)", "prim->m_renderFlag = 257", "gi.distance = 50.0f",
    "gi.flag[2] = 100", "gi.fullDisplayAngle = 45", "origin.z + (variant == 100 ? 100.0f : 0.0f)",
    "origin.y + 100.0f",
):
    if token not in texture:
        raise AssertionError(f"local selector-14/100 EffectTextureSet profile missing {token}")

if 'extract_body(effect_texture_emitter "${effect_source}" "void CRagEffect::ApplyEffectTexture(" "bool CRagEffect::ProcessEffectTextureSet(")' not in TEST_CMAKE:
    raise AssertionError("production ApplyEffectTexture helper is not extracted into the lifecycle harness")
fixture = re.search(r"CRagEffect effect; effect\.m_type=554; effect\.m_master=&actor; effect\.m_duration=73;(.*?)(?=\n        })", LIFECYCLE, re.S)
if fixture is None or 'effect.ApplyEffectTexture("effect\\\\freeze_a.bmp",2)' not in fixture.group(1):
    raise AssertionError("effect554 selector2 production lifecycle fixture disappeared")
for token in (
    "PP_EFFECTTEXTURE", "m_renderFlag==5", "m_duration==73", "distance==10.0f",
    "arcBaselineY==24.0f", "fullDisplayAngle==135", "rotStart==73", "flag[4]==2",
    "rngDraws==draws+1", "follows master movement without relaunch",
):
    if token not in fixture.group(1):
        raise AssertionError(f"effect554 selector2 lifecycle assertion missing {token}")

print("effects554/676/693/694/701-703 parity passed: callers, local routes, exact resources and selector-2/4/14/100 profiles; effect554 runtime fixture bound")
