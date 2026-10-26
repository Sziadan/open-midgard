#!/usr/bin/env python3
"""Compare source weather launch routes and lifecycle cadence with the port."""
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
launcher = body(MODE, "void LaunchMapWeatherEffect(CGameMode& mode)")
finish = body(MODE, "void FinishMapLoading(CGameMode& mode)")
on_update = body(MODE, "void CGameMode::OnUpdate()")


def check(label: str, condition: bool) -> None:
    assert condition, f"FAIL {label}"
    print(f"PASS {label}")


check("reference and executable LaunchCloud bodies are identical",
      re.sub(r"\s+", " ", ref_cloud) == re.sub(r"\s+", " ", exe_cloud))
check("source Reset-cleared pointer makes first map launch eligible and binds player",
      "!m_Effect || m_Effect->m_duration - m_Effect->m_stateCnt > 300" in ref_cloud
      and "this->m_pPlayer = m_player" in ref_cloud
      and "this->m_Effect = CAbleToMakeEffect::LaunchEffect" in ref_cloud)
expected = {0: 229, 1: 230, 2: 233, 3: 515, 4: 516, 5: 592, 7: 697, 8: 698}
for map_id, effect_id in expected.items():
    source_route = ("this->m_Effect = CAbleToMakeEffect::LaunchEffect(m_player, 229" in ref_cloud if map_id == 0
                    else re.search(rf"case {map_id}:\s*memset\(&v5, 0, sizeof\(v5\)\);\s*this->m_Effect = CAbleToMakeEffect::LaunchEffect\(m_player, {effect_id}", ref_cloud) is not None)
    check(f"Cloud map {map_id} resolves to effect {effect_id}", source_route)
check("source map-zero aliases and port resolver agree",
      "aGefFild07Rsw" in REF_MODE and "aMjolnir01Rsw" in REF_MODE
      and '"gef_fild07.rsw"' in resolver and '"mjolnir_01.rsw"' in resolver
      and "return 229;" in resolver[:resolver.find("return 230;")])
check("remaining source map groups map to the port's same effect ids",
      all(f"return {effect_id};" in resolver for effect_id in (230, 233, 515, 516, 592, 697, 698))
      and all(name in resolver for name in ("yuno.rsw", "valkyrie.rsw", "einbroch.rsw", "airplane.rsw", "thana_boss.rsw", "6@tower.rsw", "5@tower.rsw")))
check("source map setup resets weather before ScriptProcess and one-shot launch branches",
      "CWeather::Reset(&g_Weather);" in REF_MODE
      and REF_MODE.index("CWeather::Reset(&g_Weather);") < REF_MODE.index("CWeather::ScriptProcess(&g_Weather, v71);")
      and "CWeather::LaunchCloud(&g_Weather, v71._Res);" in REF_MODE)
check("source map setup launches only with effects enabled",
      "if ( g_session.m_isEffectOn )" in REF_MODE[REF_MODE.index("CWeather::ScriptProcess(&g_Weather, v71);"):REF_MODE.index("m_player = this->m_world->m_player", REF_MODE.index("CWeather::ScriptProcess(&g_Weather, v71);"))])
check("current launch helper maps effects-off and unsupported-map branches to no launch",
      "if (!g_session.m_isEffectOn)" in launcher and "if (effectId == 0)" in launcher
      and "LaunchEffect(effectId" in launcher)
check("current map-completion hook launches once after actor sync",
      finish.count("LaunchMapWeatherEffect(mode);") == 1
      and finish.index("SyncBootstrapRuntimeActorWorldPos(mode);") < finish.index("LaunchMapWeatherEffect(mode);")
      and finish.index("LaunchMapWeatherEffect(mode);") < finish.index("mode.m_mapLoadingStage = CGameMode::MapLoading_None;"))
check("current OnUpdate no longer runs a weather reconciler every frame",
      "EnsureMapWeatherEffects(*this);" not in on_update
      and "LaunchMapWeatherEffect(*this);" not in on_update)
check("source actor rebuild explicitly resets weather state",
      "CWeather::Reset(&g_Weather);" in body(REF_MODE, "void __thiscall CGameMode::RemoveGameActorAllAndMake(CGameMode *this)\n{"))

print("18 mechanical weather map/lifecycle comparisons passed")
