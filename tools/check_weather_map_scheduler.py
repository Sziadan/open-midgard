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


checks = 0


def check(label: str, condition: bool) -> None:
    global checks
    assert condition, f"FAIL {label}"
    checks += 1
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
string_defs = dict(re.findall(r'char (a\w+Rsw)\[\d+\] = "([^"]+)";', EXE))
source_weather_setup = REF_MODE[REF_MODE.index("if ( !strcmp(aGefFild07Rsw, this->m_rswName)"):REF_MODE.index("m_player = this->m_world->m_player", REF_MODE.index("if ( !strcmp(aGefFild07Rsw, this->m_rswName)"))]
source_groups = {
    229: ("aGefFild07Rsw", "aMjolnir01Rsw"),
    230: ("aYunoRsw", "aGonryunRsw", "aGonDun02Rsw", "aRaTemskyRsw", "aQueTemskyRsw", "aSchGldRsw", "aBatFild02Rsw", "aBatB01Rsw", "aBatB02Rsw"),
    233: ("aValkyrieRsw", "aRwc01Rsw", "aHiminnRsw", "aQueQsch01Rsw", "aQueQsch02Rsw", "aQueQsch03Rsw", "aQueQsch04Rsw", "aQueQsch05Rsw", "aQueQaru01Rsw", "aQueQaru02Rsw", "aQueQaru03Rsw", "aQueQaru04Rsw", "aQueQaru05Rsw"),
    515: ("aEinbrochRsw",),
    516: ("aAirplaneRsw", "aAirplane01Rsw"),
    592: ("aThanaBossRsw", "aMocFild22Rsw"),
    697: ("a6TowerRsw",),
    698: ("a5TowerRsw",),
}
source_codes = {229: 0, 230: 1, 233: 2, 515: 3, 516: 4, 592: 5, 697: 7, 698: 8}
resolver_segments = {}
last_return_end = 0
for match in re.finditer(r"return (\d+);", resolver):
    effect_id = int(match.group(1))
    resolver_segments[effect_id] = resolver[last_return_end:match.end()]
    last_return_end = match.end()
for effect_id, source_names in source_groups.items():
    expected_names = {string_defs[name] for name in source_names}
    actual_names = set(re.findall(r'"([^"\\]+\.rsw)"', resolver_segments[effect_id]))
    first_source_name = source_names[0]
    source_name_pos = source_weather_setup.index(first_source_name)
    next_source_code = re.search(r"v71\._Res = (\d+);", source_weather_setup[source_name_pos:])
    source_code_matches = (next_source_code is not None and int(next_source_code.group(1)) == source_codes[effect_id])
    if effect_id in (697, 698):
        source_code_matches = ("v71._Res = 7;" in source_weather_setup
                               and "v71._Res = 8;" in source_weather_setup
                               and "a6TowerRsw" in source_weather_setup
                               and "a5TowerRsw" in source_weather_setup)
    check(f"all source aliases map exactly to weather effect {effect_id}",
          all(name in source_weather_setup for name in source_names)
          and source_code_matches
          and expected_names == actual_names)
check("tower aliases are source literals, with no extra non-source variants",
      string_defs["a6TowerRsw"] in resolver_segments[697]
      and string_defs["a5TowerRsw"] in resolver_segments[698]
      and set(re.findall(r'"([^"\\]+\.rsw)"', resolver_segments[697])) == {string_defs["a6TowerRsw"]}
      and set(re.findall(r'"([^"\\]+\.rsw)"', resolver_segments[698])) == {string_defs["a5TowerRsw"]})
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

print(f"{checks} mechanical weather map/lifecycle comparisons passed")
