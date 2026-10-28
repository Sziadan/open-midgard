# Skill packet replay fixtures

Each scenario is UTF-8 TSV with three columns: packet opcode (integer, `0x` prefix accepted), raw packet bytes as hex, and a human-readable comment. Blank lines and lines beginning with `#` are ignored. The opcode is repeated in the row so the runner can check that the metadata matches the little-endian packet header.

`bash5.tsv` is only a framing/router seed: it contains one 29-byte `0x008A` actor-action packet and is not a recorded or behavior-validated Bash 5 exchange. `skill_packet_replay_checks` exercises production `TryReadPacket`, `CGameModePacketRouter`, and `HandleActorActionNotify` logic against it, including malformed and unknown packet rejection. Lightweight world, actor, and packet-profile doubles capture the handler's message 11 payload.

The full WP10.8 harness is still pending: real `CGameMode`/`CWorld` fixtures, registered production skill handlers, 24 ms world advancement, runtime event logging, and the nine WP2 acceptance scenarios. Do not change `SKILL_MATRIX.md` rows to `verified(replay ...)` based on this smoke fixture.
