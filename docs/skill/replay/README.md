# Skill packet replay fixtures

Each scenario is UTF-8 TSV with three columns: packet opcode (integer, `0x` prefix accepted), raw packet bytes as hex, and a human-readable comment. Blank lines are ignored. Packet rows whose first character is `#` are comments. Optional actor setup comments use `# actor<TAB>role<TAB>aid<TAB>job<TAB>cellX<TAB>cellY`; role is `source`, `target`, or `actor`. The source initializes the player AID, target configures the primary target, and each `actor` row adds another registered world actor. The opcode is repeated in each packet row so the runner can check that the metadata matches the little-endian packet header.

`actor_action.tsv` is a 29-byte `0x008A` framing/router fixture used by the standalone smoke test to exercise production `TryReadPacket`, `CGameModePacketRouter`, and `HandleActorActionNotify` logic against lightweight doubles, including malformed and unknown packet rejection. `blessing34.tsv` is a 15-byte `0x011A` successful Blessing34 notify at level3; it checks Ref target effect42.

`cast_cancel01b9.tsv` is a 6-byte `0x01B9` cast-cancel packet. The runner seeds begin-spell and magic-target effects on the player, routes the packet through the default router, and checks Ref actor-message order84,87,83 and detachment of both effects.

`firewall_unit_lifecycle.tsv` contains a 16-byte `0x011F` Fire Wall unit entry (job127, unit AID100, cell10,20) followed by a 6-byte `0x0120` removal. It checks the production `CSkill` world lists, terrain-mapped unit position, Fire Wall wave, and removal. Between entry and removal it calls the real `CSkill::OnProcess` at nominal 24 ms intervals until the Ref 3-second lifetime expires, checking loop effect25 and measured expiry timing.

`safety_wall_unit_lifecycle.tsv` replays unit job126 (`UNT_SAFETYWALL`) at the same controlled cell, calls `CSkill::OnProcess` for one nominal 24 ms step, checks loop effect315, and removes the unit with `0x0120`.

`ice_wall_unit_lifecycle.tsv` replays unit job141 (`UNT_ICEWALL`), checks its loop effect74 after one nominal 24 ms process step, then removes it with `0x0120`.

`magnus_unit_lifecycle.tsv` replays unit job132 (`UNT_MAGNUS`), checks its loop effect318 after one nominal 24 ms process step, then removes it with `0x0120`.

`demonstration_unit_lifecycle.tsv` replays unit job177 (`UNT_DEMONSTRATION`), checks its loop effect302 after one nominal 24 ms process step, then removes it with `0x0120`.

`quagmire_unit_lifecycle.tsv` replays unit job142 (`UNT_QUAGMIRE`) for nominal 24 ms process steps through its periodic effect95 threshold, then removes it with `0x0120`.

`basilica_unit_lifecycle.tsv` replays unit job180 (`UNT_BASILICA`), checks its loop effect374 after one nominal 24 ms process step, then removes it with `0x0120`.

`gospel_unit_lifecycle.tsv` replays unit job179 (`UNT_GOSPEL`), checks its loop effect370 after one nominal 24 ms process step, then removes it with `0x0120`.

`volcano_unit_lifecycle.tsv`, `deluge_unit_lifecycle.tsv`, `violent_gale_unit_lifecycle.tsv`, and `land_protector_unit_lifecycle.tsv` replay unit jobs154–157 and verify loop effects239–242 respectively after one nominal 24 ms process step, followed by `0x0120` removal.

`spider_web_unit_lifecycle.tsv` replays unit job183 (`UNT_SPIDERWEB`), checks loop effect404 after one nominal 24 ms process step, then removes it with `0x0120`.

The song unit fixtures cover jobs158–175 (Lullaby through Service for You) and verify the Ref loop effects278–294 after one nominal 24 ms process step; Dissonance job166 uses effect277. Each unit is removed with `0x0120`.

`gravitation_unit_lifecycle.tsv`, `suiton_unit_lifecycle.tsv`, and `tatamigaeshi_unit_lifecycle.tsv` replay jobs184,187,188 and verify Ref loop effects522,620,631 respectively after one nominal 24 ms process step, followed by `0x0120` removal.

`pneuma_default_unit.tsv` replays unit job133 (`UNT_PNEUMA`) and verifies that the Ref default path produces no unit effect during its first process step before `0x0120` removal. Ref has no job133 branch in `CSkill::SendMsg(45)` or `CSkill::OnProcess`; see `MISMATCHES.md`.

`kaen_unit_lifecycle.tsv` and the five `ground_drift_*_unit_lifecycle.tsv` fixtures replay jobs189–194 and verify periodic effects634,638–642 on the 17th nominal 24 ms process step before `0x0120` removal.

`magnus_unit_lifecycle.tsv` replays unit job132 (`UNT_MAGNUS`), checks its loop effect318 after one nominal 24 ms process step, then removes it with `0x0120`.

`sanctuary_unit_lifecycle.tsv` replays unit job131 (`UNT_SANCTUARY`), calls `CSkill::OnProcess` for one nominal 24 ms step, checks loop effect319, and removes the unit with `0x0120`.

`warp_unit_lifecycle.tsv` replays a job128 waiting Warp unit, applies a `0x01D7` base-look/job update to job129, steps both states through `CSkill::OnProcess` (loop effects317/316), then removes the unit. Sprite-change routing checks the skill-unit registry before it creates a runtime actor.

`trap_reveal_lifecycle.tsv` exercises trap job143 with two `0x011F` entries (visible then hidden for a non-party creator), `0x01AC` reveal, and `0x0120` removal. It checks the runtime visibility changes and unit cleanup; model rendering remains resource-dependent.

`bash5.tsv` is a 31-byte `0x0114` skill-damage packet for skill 5 with source AID 1, target AID 2, level 5, one hit, and action 5. `double_strafe46.tsv` is another 31-byte packet for skill46, with damage200, level5, div2 and action9. `fire_bolt19.tsv` carries skill19, damage100, level5, div1, and action4. `heal28.tsv` is a 15-byte `0x011A` no-damage notify packet for Heal28 and value199; it checks effect7 and message88. `sonic_blow136.tsv` uses action9 and division4, checking four target hits plus three self animation WBAs. `asura_strike271.tsv` checks action4's immediate target presentation (WBA field message39, effects0/−1) and the attacker's state2 transition. `grimtooth137.tsv` checks the action4 target WBA with effects0/132. The production-linked runtime tests instantiate the real game mode, world, session, player, and target actor, register the client's default packet routes, dispatch raw packets, poll `ProcessWillBeAttacked` at nominal 24 ms intervals, and compare runtime logs. Delivery assertions use elapsed `timeGetTime` from the same clock as WBA scheduling and allow one measured polling interval plus clock granularity.

All nine initial WP2 acceptance scenarios now run through the production packet router. Storm Gust89 and Sanctuary70 use the 18-byte `0x0117` ground-skill packet; the fixture supplies a 32×32 attribute map and checks effect placement at the decoded cell with terrain height. The first WP3.4 buff scenario, Blessing34, also passes and checks target effect42. Replay-only event traces capture `LaunchEffect`, `PlaceEffect`, `PlayWave`, `SetState`, `SendMsg88`, and `UnportedEffect` calls; WBA logging also covers message108 events that do not use `HandleAttacked`.

The remaining WP10.8 work is to continue the WP3.4 and WP4 acceptance scenarios, then broaden runtime stepping to multiple ground units and the full world update path. Do not change `SKILL_MATRIX.md` rows to `verified(replay ...)` until each row's complete Ref walk-through matches its replay log.
