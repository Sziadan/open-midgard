#!/usr/bin/env python3
"""Compare Cloud callers 515/516/592 with local weather-cloud aggregation."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
REF2 = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
REF_DHXJ = (ROOT / "Ref/dhxj/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
DISPATCH = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")
DISPATCH2 = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
GENERATOR = (ROOT / "tools/generate_effect_dispatch.py").read_text(encoding="utf-8")
TESTS = (ROOT / "tools/tests/EffectLifecycleTests.cpp").read_text(encoding="utf-8")


def require(ok: bool, message: str) -> None:
    if not ok:
        raise SystemExit(message)


def helper(source: str, signature: str) -> str:
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[start:pos + 1]
    raise SystemExit(f"unterminated helper: {signature}")


def caller_case(source: str, case_label: str, expected_variant: int) -> str:
    matches = list(re.finditer(rf"case {case_label}:\s*(.*?)(?=\n\s*case |\n\s*default:)", source, re.S))
    for match in matches:
        body = match.group(1)
        if f"CRagEffect::Cloud(this, {expected_variant})" in body:
            return body
    raise SystemExit(f"Cloud caller {case_label} variant {expected_variant} missing")


for effect_id, case_label, map_variant, effect_route in (
    (515, "0x203", 3, "case 515: SpawnWeatherCloud(); return true;"),
    (516, "0x204", 4, "case 516: SpawnWeatherCloud(); return true;"),
    (592, "0x250", 5, "case 592: SpawnWeatherCloud(); return true;"),
):
    pe_body = caller_case(PE, case_label, map_variant)
    ref_body = caller_case(REF, case_label, map_variant)
    for label, body in (("PE", pe_body), ("primary Ref", ref_body)):
        require(re.search(r"=\s*80\s*;", body) is not None
                and re.search(r"while\s*\([^)]*\)\s*;", body) is not None,
                f"{label} effect{effect_id} no longer invokes Cloud in an 80-call loop")
    require(effect_route in DISPATCH, f"effect{effect_id} generated local route changed")
    require(re.search(rf"{effect_id}:\s*\"SpawnWeatherCloud\(\); return true;\"", GENERATOR),
            f"effect{effect_id} generator route changed")

    config_case = re.search(rf"case {effect_id}:\s*m_param\[0\]\s*=\s*{map_variant}\.0f;\s*m_param\[1\]\s*=\s*80\.0f;", LOCAL)
    require(config_case is not None, f"effect{effect_id} local map/count configuration changed")

pe_onprocess = helper(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
for effect_id, case_label, map_variant, cloud_count, enum_name in (
    (229, "229", 0, 40, "EF_CLOUD"),
    (230, "230", 1, 60, "EF_CLOUD2"),
    (233, "233", 2, 40, "EF_CLOUD3"),
):
    pe_body_match = re.search(rf"case {case_label}:(.*?)(?=\n\s*case |\n\s*default:|\Z)", pe_onprocess, re.S)
    require(pe_body_match is not None
            and re.search(rf"=\s*{cloud_count}\s*;", pe_body_match.group(1))
            and f"CRagEffect::Cloud(this, {map_variant})" in pe_body_match.group(1),
            f"PE effect{effect_id} Cloud variant/count changed")
    ref_case = re.search(rf"case\s+{enum_name}\s*:(.*?)(?=\n\s*case\s+|\Z)", REF_DHXJ, re.S)
    require(ref_case is not None
            and re.search(rf"i\s*<\s*{cloud_count}", ref_case.group(1))
            and re.search(rf"Cloud\(\s*{map_variant}\s*\)", ref_case.group(1)),
            f"secondary Ref {enum_name} Cloud variant/count changed")
    require(f"Cloud(this, {map_variant})" not in REF,
            f"primary Ref unexpectedly gained the absent low-variant effect{effect_id} caller")
    local_route = f"case {effect_id}: SpawnWeatherCloud(); return true;"
    require(local_route in DISPATCH2, f"effect{effect_id} local OnProcess route changed")
    local_config = re.search(
        rf"case {effect_id}:\s*m_param\[0\]\s*=\s*{map_variant}\.0f;\s*m_param\[1\]\s*=\s*{cloud_count}\.0f;",
        LOCAL)
    require(local_config is not None, f"effect{effect_id} local Cloud variant/count setup changed")

pe_cloud = helper(PE, "void __thiscall CRagEffect::Cloud(CRagEffect *this, char map)")
ref_cloud = helper(REF2, "void __thiscall CRagEffect::Cloud(CRagEffect *this, char map)")
require(re.sub(r"\s+", "", pe_cloud) == re.sub(r"\s+", "", ref_cloud),
        "PE and secondary-Ref shared Cloud helpers diverged")
for token in ("m_world->m_player->m_pos", "m_stateCnt == 99999998", "m_duration = 99999999",
              "PP_CLOUD", "m_renderFlag |= 1u", "m_totalTexture = 3", "rand() % 3",
              "height[7]", "full_display_angle = 45", "m_GI[v5].life = 1"):
    require(token in pe_cloud, f"canonical Cloud helper no longer contains {token}")

weather_emitter = re.search(r"void CRagEffect::SpawnWeatherCloud\(\)\s*\{(.*?)\n\}", LOCAL, re.S)
require(weather_emitter is not None, "local SpawnWeatherCloud emitter missing")
body = weather_emitter.group(1)
for token in ("m_param[0]", "m_param[1]", "cloudIndex < cloudCount", "LaunchEffectPrim(PP_CLOUD",
              "m_totalTexture = 3", "ResolveWeatherCloudTexture(mapVariant)", "mapVariant == 3",
              "mapVariant == 5"):
    require(token in body, f"local weather-cloud emitter lacks {token}")
require("mapVariant==3?" in TESTS and "mapVariant==5?" in TESTS
        and "for(int mapVariant:{1,2,3,4,5,7,8})" in TESTS
        and "Cloud maps 1/2/3/4/5/default" in TESTS,
        "weather-cloud lifecycle fixtures no longer cover mapped variants 0-5")
print("effects229/230/233/515/516/592 Cloud parity passed: PE/Ref callers, local mappings and generic lifecycle payloads")
