# Skill packet replay fixtures

Each scenario is UTF-8 TSV with three columns: packet opcode (integer, `0x` prefix accepted), raw packet bytes as hex, and a human-readable comment. Blank lines and lines beginning with `#` are ignored. The opcode is repeated in the row so the runner can check that the metadata matches the little-endian packet header.

`actor_action.tsv` is a 29-byte `0x008A` framing/router fixture used by the standalone smoke test to exercise production `TryReadPacket`, `CGameModePacketRouter`, and `HandleActorActionNotify` logic against lightweight doubles, including malformed and unknown packet rejection.

`bash5.tsv` is a 31-byte `0x0114` skill-damage packet for skill 5 with source AID 1, target AID 2, level 5, one hit, and action 5. `double_strafe46.tsv` is another 31-byte packet for skill46, with damage200, level5, div2 and action9. The production-linked runtime tests instantiate the real game mode, world, session, player, and target actor, register the client's default packet routes, dispatch raw packets, poll `ProcessWillBeAttacked` at nominal 24 ms intervals, and compare runtime logs. Delivery assertions use elapsed `GetTickCount` time and allow one measured polling interval plus clock granularity.

These are the first two WP2 skill scenarios. Fire Bolt19 and the remaining six WP2 scenarios, full actor/cell/job fixtures, and effect/place/state/unported-event logging remain pending.

The remaining WP10.8 work is to add the other eight WP2 scenarios, repeatable 24 ms world stepping for the scenario actors, and log effect launches/placements, states, and unported effects alongside WBA and waves. Do not change `SKILL_MATRIX.md` rows to `verified(replay ...)` until each row's complete Ref walk-through matches its replay log.
