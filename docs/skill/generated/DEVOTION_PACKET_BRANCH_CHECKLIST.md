# `0x01CF` / `Zc_DevotionList` branch checklist

`python tools/tests/check_devotion_packet_source.py` compares the full normalized canonical EXE handler at VA `0x5A7E30` with `Ref/GameModePacket.cpp:10558`, checks dispatcher case 463, packet database length 28, `clif.c` field documentation, current packet-size framing, and the current handler route.

The ordered gameplay mapping is:

1. Return when effects are disabled.
2. Resolve the devoter as the local player for matching session AID, or look up the actor otherwise.
3. When the devoter exists, send message 109 with effect 232 exactly five times to remove the previous set.
4. Iterate exactly five devotee AIDs at offsets `+6`, `+10`, `+14`, `+18`, and `+22`; skip zero IDs.
5. For every nonzero ID, form position X from the devoter AID and Y from the devotee AID.
6. If the devoter exists, launch effect 232 at zero offset and send message 14 with the position only when launch succeeds.

The two loops still run when the actor lookup fails, with their actor-dependent effects skipped. Packet short `+26` (maximum distance) is not read by the handler. The executable sets only position X/Y; implementation zeroes the otherwise-uninitialized Z for deterministic behavior, recorded in `MISMATCHES.md`.
