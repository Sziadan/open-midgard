#!/usr/bin/env python3
"""Mechanically compare CRagEffect::Cloud producer setup with Ref and EXE."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
REF = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
REF_DISPATCH = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
IMPL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")


def body(text: str, marker: str) -> str:
    start = text.index(marker)
    opening = text.index("{", start)
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[opening + 1:pos]
    raise AssertionError(f"unclosed function body: {marker}")


ref = body(REF, "void __thiscall CRagEffect::Cloud(CRagEffect *this, char map)\n{")
exe = body(EXE, "void __thiscall CRagEffect::Cloud(CRagEffect *this, char map)\n{")
impl = body(IMPL, "void CRagEffect::SpawnWeatherCloud()")
texture = body(IMPL, "CTexture* ResolveWeatherCloudTexture(int mapVariant)")
init = body(IMPL, "void CRagEffect::Init(CRenderObject* master, int effectId, const vector3d& deltaPos)")


def check(label: str, condition: bool) -> None:
    assert condition, f"FAIL {label}"
    print(f"PASS {label}")


check("reference and executable Cloud producer bodies are identical",
      re.sub(r"\s+", " ", ref) == re.sub(r"\s+", " ", exe))
check("source position refresh, sentinel conversion, and state-zero launch gate are preserved",
      "this->m_pos = CModeMgr::GetGameMode(&g_modeMgr)->m_world->m_player->m_pos" in ref
      and "this->m_stateCnt == 99999998" in ref and "this->m_duration = 99999999" in ref
      and "m_effectPos = player->m_pos" in impl and "m_stateCnt == 99999998" in impl
      and "if (m_stateCnt != 0)" in impl)
check("each requested launch has PP_CLOUD, flags five, source duration and three texture slots",
      "LaunchEffectPrim(this, PP_CLOUD" in ref and "m_renderFlag |= 5u" in ref
      and "g_prim->m_duration = this->m_duration" in ref and "m_totalTexture = 3" in ref
      and "LaunchEffectPrim(PP_CLOUD, vector3d{})" in impl and "prim->m_renderFlag = 5u" in impl
      and "prim->m_duration = m_duration" in impl and "prim->m_totalTexture = 3" in impl
      and "prim->m_texture.resize(3, nullptr)" in impl)
check("single rand modulo three selects exact fog/cloud texture families without fallback retries",
      "v3 = rand() % 3" in ref and all(name in ref for name in ("aEffectFog1Tga", "aEffectFog2Tga", "aEffectFog3Tga", "aEffectCloud4Tg", "aEffectCloud1Tg", "aEffectCloud2Tg"))
      and "return g_texMgr.GetTexture(textureNames[rand() % 3], false);" in texture
      and "ResolveEffectTextureCandidates" not in texture)
check("four source GI bands initialize life/process/map/display angle/alpha/flags/rotation",
      "while ( v5 < 4 )" in ref and "for (EffectGi& gi : prim->m_gi)" in impl
      and all(token in impl for token in ("gi.life = 1", "gi.process = 0", "gi.height[7] = static_cast<float>(mapVariant)", "gi.fullDisplayAngle = 45", "gi.alphaB = 0", "gi.flag[0] = 0", "rand() % 200 + 300")))
check("map zero, two, three, five and default position branches preserve their random ranges",
      all(token in ref for token in ("case 2:", "case 3:", "case 5:", "rand() % 300 - 150", "rand() % 200 + 25", "v14 = rand() % 10", "(double)v14 - 125.0"))
      and all(token in impl for token in ("mapVariant == 0", "mapVariant == 2", "mapVariant == 3", "mapVariant == 5", "rand() % 300 - 150", "rand() % 200 + 25", "rand() % 10 - 125")))
check("map-zero height gate and map-three terrain query/random drop match the source",
      "height[6] = -152.0" in ref and "m_pos.y > (double)g_prim->m_GI[v5].height[6]" in ref
      and "C3dAttr::GetHeight" in ref and "v7 % 5" in ref
      and "gi.height[6] = -152.0f" in impl and "m_effectPos.y > gi.height[6]" in impl
      and "GetHeight(m_effectPos.x + gi.sparkX, m_effectPos.z + gi.sparkZ) - 20.0f" in impl
      and "const int heightRandom = rand() % 5" in impl)
check("four angle samples, map-dependent distance, phase sample and style flag values match",
      all(token in ref for token in ("height[0] = (float)(rand() % 360)", "height[1] = (float)(rand() % 360)", "height[2] = (float)v14", "height[3] = (float)(rand() % 360)", "rand() % 10) + 35.0", "rand() % 20) + 30.0", "height[8] = (float)(rand() % 360)"))
      and all(token in impl for token in ("gi.height[0] = static_cast<float>(rand() % 360)", "gi.height[1] = static_cast<float>(rand() % 360)", "gi.height[2] = static_cast<float>(rand() % 360)", "gi.height[3] = static_cast<float>(rand() % 360)", "rand() % 10 + 35", "rand() % 20 + 30", "gi.height[8] = static_cast<float>(rand() % 360)", "case 3: gi.flag[2] = 12", "case 5: gi.flag[2] = 14", "case 7: gi.flag[2] = 16", "case 8: gi.flag[2] = 17")))

route_counts = {229: 40, 230: 60, 233: 40, 515: 80, 516: 80, 592: 80, 697: 80, 698: 80}
for effect_id, count in route_counts.items():
    init_case = re.search(rf"case {effect_id}:\s*m_handler = Handler::WeatherCloud;\s*m_param\[0\] = ([0-9.]+)f;\s*m_param\[1\] = ([0-9.]+)f;", init)
    check(f"effect {effect_id} keeps map/count {init_case.groups() if init_case else None}",
          init_case is not None and int(float(init_case.group(1))) == {229: 0, 230: 1, 233: 2, 515: 3, 516: 4, 592: 5, 697: 7, 698: 8}[effect_id]
          and int(float(init_case.group(2))) == count)

def route_call(text: str, case_label: str) -> tuple[int, int] | None:
    for case in re.finditer(re.escape(case_label), text):
        snippet = text[case.start():case.start() + 500]
        match = re.search(r"=\s*(\d+)\s*;\s*do\s*\{[\s\S]{0,180}?CRagEffect::Cloud\(this,\s*(\d+)\);", snippet)
        if match:
            return int(match.group(1)), int(match.group(2))
    return None


source_routes = {515: (80, 3), 516: (80, 4), 592: (80, 5), 697: (80, 7), 698: (80, 8)}
exe_routes = {229: (40, 0), 230: (60, 1), 233: (40, 2)}
route_results = [route_call(REF_DISPATCH, f"case 0x{effect_id:X}:") for effect_id in source_routes]
route_results += [route_call(EXE, f"case {effect_id}:") for effect_id in exe_routes]
check("reference/executable dispatcher loops preserve the exact per-frame Cloud call counts and map values",
      route_results == list(source_routes.values()) + list(exe_routes.values()))
check("current launch loop preserves call count and one four-band primitive per iteration",
      "for (int cloudIndex = 0; cloudIndex < cloudCount; ++cloudIndex)" in impl
      and "LaunchEffectPrim(PP_CLOUD, vector3d{})" in impl)

print("10 mechanical Cloud producer comparisons plus 8 route/count checks passed")
