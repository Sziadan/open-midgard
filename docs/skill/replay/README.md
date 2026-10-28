# Skill packet replay fixtures

Each scenario is UTF-8 TSV with three columns: packet opcode (integer, `0x` prefix accepted), raw packet bytes as hex, and a human-readable comment. Blank lines and lines beginning with `#` are ignored. The opcode is repeated in the row so the runner can check that the metadata matches the little-endian packet header.

`actor_action.tsv` is a 29-byte `0x008A` framing/router fixture used by the standalone smoke test to exercise production `TryReadPacket`, `CGameModePacketRouter`, and `HandleActorActionNotify` logic against lightweight doubles, including malformed and unknown packet rejection.

`bash5.tsv` is a 31-byte `0x0114` skill-damage packet for skill 5 with source AID 1, target AID 2, level 5, one hit, and action 5. The production-linked runtime test instantiates the real game mode, world, session, player, and target actor, registers the client's default packet routes, dispatches the raw packet, and checks the resulting WBA entry and delivery. This is the first skill scenario; other eight WP2 scenarios and full 24 ms world stepping remain pending.

The remaining WP10.8 work is to add the other eight WP2 scenarios, repeatable 24 ms world stepping for the scenario actors, and log effect launches/placements, states, and unported effects alongside WBA and waves. Do not change `SKILL_MATRIX.md` rows to `verified(replay ...)` until each row's complete Ref walk-through matches its replay log.
