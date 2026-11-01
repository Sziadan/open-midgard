# Yuno weather cloud repeat fix

Yuno launches weather effect 230 (map variant 1). Its parent lifetime is
99,999,999 effect frames, so extending the lifetime or repeatedly launching
new effects would not fix the disappearance.

The missing operation was the primitive's parent-position copy. Canonical
`HighPriest.exe.c:461331` / `Ref/RagEffect2.cpp:1082` PrimCloud updates camera
angles, copies its master effect's position, then processes its four GI bands.
Our updater left the primitive position at zero. Initial GI world coordinates
were correct, but after fading the randomized clouds respawned around world
origin, outside the player's view.

UpdateWeatherCloudPrimitive now copies `m_master->EffectPosition()` before the
band loop. The GI coordinates remain world-space, and RenderReferenceCloud
still avoids adding primitive position again. This preserves initial placement,
source fade timing, RNG calls, sway and repeat behavior.

Validation:

- `tools/check_prim_cloud_parity.py`: 10 mechanical comparisons pass, now
  checking the canonical parent-position copy instead of incorrectly requiring
  its absence.
- `tools/check_cloud_render_parity.py`: 36 renderer comparisons pass.
- `tools/tests/check_effects515_516_592_weather_cloud.py`: existing cloud caller
  mappings and payload comparisons pass.
- `WeatherCloudCycleTests.cpp` compiles the production updater and GI structure.
  It runs 5,000 updates for each of eight map variants, moving the parent far
  from the origin mid-run and checking repeated fade/respawn cycles. Yuno's
  respawns stay near the moving parent. Variant zero's distinct cumulative
  coordinate translation is preserved as in the canonical source.
- The focused regression script passes. These are standalone results, not a
  full CTest-suite result. Live visual confirmation remains for user testing.
