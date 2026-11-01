# Combat presentation fixes — 2026-11-01

## Changes and reference evidence

- Falcon: restored Ref's idle counter reset at each animation-speed boundary, so message 29 can enter the dive after a long idle. Attack movement, disappearance at counter 25, and recovery after 35 retain `Ref/Skill.cpp:1132-1180`. Movement advances on logic ticks. The spawn uses the original default zoom of 1 instead of a custom zoom of 6.
- Falcon and freeze: use `Ref/Renderer.cpp:4538`'s `oow * hpc * 0.14285715` pixel ratio, preserve ACT clip offsets, and submit projected depth. This removes screen-pixel size clamps and scales with camera perspective. Freeze inherits the owner's zoom, as in `Ref/GameActor.cpp:3538`.
- Throw Spirit Sphere: canonical `HighPriest.exe.c:460223` initializes GI `vecB_now` at the source position with Y minus 8. The local initializer mistakenly wrote `vecT_now`, leaving the projectile's actual movement origin at zero. Corrected both Tanji variants.
- Occult Impaction and spirit projectiles: canonical `RenderCloud` (`HighPriest.exe.c:462221`, `Ref/RagEffect2.cpp:12370`) places corners at GI world coordinates and calls the world-space overload with F1=1. Removed an extra addition of the primitive's position. Restored PP_CHIMTO's particle renderer route (canonical primitive render case 0x2E). Original source actor ownership and target messages remain intact.
- Fire Wall: packet-created units remain until server removal. The legacy three-second timer applies only to locally generated units. Their parent effects advance on logic ticks, synchronized with their primitives.
- Arrows: equipped ammunition (item type 10) remains visible in ETC; it was filtered out with equipped clothing/weapons. Ammo double-click supports the equipment action.

## Blast Mine finding

The gameplay log records skill 122 at level 5. Both `db/pre-re/skill_db.yml` and `db/re/skill_db.yml` in the supplied rAthena server assign Blast Mine placement durations of 25/20/15/10/5 seconds at levels 1–5. The reported five-second lifetime is expected. The client does not extend traps beyond server removal.

## Validation

- MinGW Qt preset build succeeded (`build-mingw-preset/seven-effects-build.log`).
- `CombatPresentationFixTests.cpp` compiles extracted production bodies and checks long-idle-to-dive behavior, unclamped perspective scaling, nonzero-world-origin projectile movement/rendering, packet-owned Fire Wall persistence, and equipped ammo visibility. Passed. Reverting the Tanji initializer in the generated fixture causes the regression test to fail.
- `HealTimingTests.cpp` checks Heal and ground-unit timing across multiple render frames per logic tick. Passed.
- `tools/check_cloud_render_parity.py`: 36 mechanical source comparisons passed, including outer/nested color branches and world-space translation.
- Shared test-project configuration remains blocked by its existing `ResolveSkillPacketActor` extraction failure. These are standalone focused-test results, not a full CTest-suite pass.
- Live gameplay and visual confirmation remain for the user; the game executable was not launched by the agent.
