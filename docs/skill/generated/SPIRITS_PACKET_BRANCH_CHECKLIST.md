# `0x01D0` / `Zc_Spirits` branch checklist

Source comparison: `python tools/tests/check_spirits_packet_source.py` compares the complete normalized canonical EXE body (`HighPriest.exe.c`, VA `0x5A7F30`) to `Ref/GameModePacket.cpp:26133`, checks ordered gameplay branch anchors against `src/gamemode/GameModePacket.cpp::HandleSpirits`, verifies dispatch ID 464 in both sources, and verifies `Ref/RunningServer/packet_db.txt` declares `0x01d0,8`.

The checker confirms the full EXE/Ref handler bodies match and validates ordered branch anchors in the implementation:

1. Effects-enabled gate.
2. Resolve the local player when packet AID equals session AID; otherwise resolve actor by AID.
3. Null-actor exit.
4. Send the initial actor message 109 using job mapping 4038 → 504, 24 → 629, otherwise 228.
5. AID-zero gate around subsequent refresh/effect work.
6. Burrow-or-hiding state branch repeats the job-specific message 109 and does not launch a new effect.
7. Visible-state branch launches effect 504 for job 4038, otherwise 629 for job 24 or 228 for other jobs, with zero offset and zero parameter.
8. Copy the signed 16-bit packet value at offset `+6` to `pos.x`.
9. If launch succeeded, send effect message 14 with the position pointer.

| Check | Status |
|---|---|
| Canonical EXE body equals Ref body after whitespace normalization | PASS |
| Dispatcher ID 464 routes to `Zc_Spirits` in EXE and Ref | PASS |
| Packet database opcode/length | PASS: `0x01D0`, 8 bytes |
| Production implementation branch comparison | PASS: checked by the same script against ordered `HandleSpirits` branch anchors, including actor resolution and null message senders |

The current handler adds null-data, minimum-length, packet-ID, and world-pointer guards around the source logic for defensive packet/object handling. These guard additions do not change the valid packet branch mapping. The executable initializes only `pos.x`; the reconstructed implementation zero-initializes `position.y/z` deterministically because those source components are indeterminate, and sends the vector only when launch succeeds.

This checklist covers gameplay state/effect behavior only; no window or presentation work is included.
