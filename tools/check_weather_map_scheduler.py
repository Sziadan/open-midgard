#!/usr/bin/env python3
"""Compare source weather launch routes with the current map scheduler."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
REF_EFFECT = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
REF_MODE = (ROOT / "Ref/GameMode.cpp").read_text(encoding="utf-8", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
MODE = (ROOT / "src/gamemode/GameMode.cpp").read_text(encoding="utf-8")


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


ref_cloud = body(REF_EFFECT, "void __thiscall CWeather::LaunchCloud(CWeather *this, char map)\n{")
exe_cloud = body(EXE, "void __thiscall CWeather::LaunchCloud(CWeather *this, char map)\n{")
resolver = body(MODE, "int ResolveRefWeatherCloudEffectId(const char* rswName)")
ensure = body(MODE, "void EnsureMapWeatherEffects(CGameMode& mode)")
on_update = body(MODE, "void CGameMode::OnUpdate()")


def check(label: str, condition: bool) -> None:
    assert condition, f"FAIL {label}"
    print(f"PASS {label}")


check("reference and executable LaunchCloud bodies are identical",
      re.sub(r"\s+", " ", ref_cloud) == re.sub(r"\s+", " ", exe_cloud))
check("source refresh gate and player assignment are preserved in reference evidence",
      "!m_Effect || m_Effect->m_duration - m_Effect->m_stateCnt > 300" in ref_cloud
      and "this->m_pPlayer = m_player" in ref_cloud
      and "this->m_Effect = CAbleToMakeEffect::LaunchEffect" in ref_cloud)
expected = {0: 229, 1: 230, 2: 233, 3: 515, 4: 516, 5: 592, 7: 697, 8: 698}
for map_id, effect_id in expected.items():
    source_route = ("this->m_Effect = CAbleToMakeEffect::LaunchEffect(m_player, 229" in ref_cloud if map_id == 0
                    else re.search(rf"case {map_id}:\s*memset\(&v5, 0, sizeof\(v5\)\);\s*this->m_Effect = CAbleToMakeEffect::LaunchEffect\(m_player, {effect_id}", ref_cloud) is not None)
    check(f"Cloud map {map_id} resolves to effect {effect_id}", source_route)
check("map-zero world aliases include both gef_fild07 and mjolnir_01 in source and implementation",
      "aGefFild07Rsw" in REF_MODE and "aMjolnir01Rsw" in REF_MODE
      and '"gef_fild07.rsw"' in resolver and '"mjolnir_01.rsw"' in resolver
      and "return 229;" in resolver[:resolver.find("return 230;")])
check("remaining map groups are mapped to the same effect IDs in the current resolver",
      all(f"return {effect_id};" in resolver for effect_id in (230, 233, 515, 516, 592, 697, 698))
      and all(name in resolver for name in ("yuno.rsw", "valkyrie.rsw", "einbroch.rsw", "airplane.rsw", "thana_boss.rsw", "6@tower.rsw", "5@tower.rsw")))
check("current scheduler is called after player processing and gates duplicate per-frame launches above 300",
      "EnsureMapWeatherEffects(*this);" in on_update
      and "remainingFrames > kWeatherCloudRefreshThresholdFrames" in ensure
      and "return;" in ensure[ensure.index("remainingFrames > kWeatherCloudRefreshThresholdFrames"):])
check("map setup invokes LaunchCloud after weather reset and script processing",
      "CWeather::Reset(&g_Weather);" in REF_MODE
      and REF_MODE.index("CWeather::Reset(&g_Weather);") < REF_MODE.index("CWeather::ScriptProcess(&g_Weather, v71);")
      and "CWeather::LaunchCloud(&g_Weather, v71._Res);" in REF_MODE)

print("8 mechanical weather map/launch comparisons passed")
