#!/usr/bin/env python3
"""Compare animation effects689/690/691/696 and their exact frame lists."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
REF2 = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="latin-1")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
ROUTE = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")


def function_body(source, signature):
    for match in reversed(list(re.finditer(re.escape(signature), source))):
        start = match.start()
        opening = source.find("{", start + len(signature))
        if opening < 0 or source.find(";", start + len(signature), opening) >= 0:
            continue
        depth = 0
        for pos in range(opening, len(source)):
            if source[pos] == "{":
                depth += 1
            elif source[pos] == "}":
                depth -= 1
                if depth == 0:
                    return source[opening + 1:pos]
    raise AssertionError(f"function body not found: {signature}")


def compact(source):
    return re.sub(r"\s+", "", source)


signature = "void __thiscall CRagEffect::EffectTextureSet_Animation(CRagEffect *this, char F1)"
pe_helper = function_body(PE, signature)
ref_helper = function_body(REF2, signature)
if compact(pe_helper) != compact(ref_helper):
    raise AssertionError("PE and primary-Ref animation helper bodies differ")

for source_name, source in (("PE", PE), ("primary Ref", REF)):
    body = function_body(source, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
    for effect_id, variant in ((689, 0), (690, 1), (691, 2), (696, 3)):
        pattern = rf"case 0x{effect_id:X}:\s*CRagEffect::EffectTextureSet_Animation\(this, {variant}\);\s*return 1;"
        if not re.search(pattern, body):
            raise AssertionError(f"{source_name} effect{effect_id} animation caller changed")
        if f"case {effect_id}: return ProcessEffectTextureAnimation();" not in ROUTE:
            raise AssertionError(f"local generated effect{effect_id} route changed")

local_helper = function_body(LOCAL, "bool CRagEffect::ProcessEffectTextureAnimation()")
for token in (
    "case 689: variant = 0;", "case 690: variant = 1;", "case 691: variant = 2;", "case 696: variant = 3;",
    "existing->m_pos = origin", "if (m_stateCnt != 0)", "LaunchEffectPrim(PP_EFFECTTEXTURE_ANI",
    "prim->m_renderFlag = 5", "prim->m_duration = m_duration", "prim->m_size = 0.0f",
    '"effect\\\\dust0"', '"effect\\\\torch_"', "m_param[0] <= 0.0f ? 20.0f : m_param[0]",
    "gi.arcBaselineY = origin.y - 10.0f", "gi.flag[2] = 4", "gi.height[10] = 0.0f",
    "gi.fullDisplayAngle = 135", "gi.height[8] = 0.0f", "variant == 0 ? 100 : 130",
    "gi.rotStart = static_cast<short>(rand() % 360)", "gi.flag[4] = static_cast<u8>(variant)",
):
    if token not in local_helper:
        raise AssertionError(f"local animation helper missing {token}")

image = ExeImage(ROOT / "HighPriest.exe")
strings = {item.raw for item in image.iter_cstrings()}
for stem, count in (("dust", 9), ("torch_red", 13), ("torch_green", 13), ("torch_violet", 13)):
    for index in range(1, count + 1):
        expected = f"effect\\{stem}{index:02d}.bmp".encode("ascii")
        if expected not in strings:
            raise AssertionError(f"PE resource missing: {expected!r}")

print("effects689/690/691/696 animation parity passed: callers, full PE/Ref helper, local profiles and all 48 exact frame resources")
