# `0x01E1` / `Zc_Spirits2` branch checklist

The source comparator `python tools/tests/check_spirits2_packet_source.py` compares the complete normalized canonical EXE body at VA `0x5A8160` with `Ref/GameModePacket.cpp:26218`. It also compares canonical EXE `CAbleToMakeEffect::IsEffectExist` at VA `0x559FD0` with `Ref/GameActor.cpp`, then compares its loop semantics to current `EffectExists`. Dispatch case 481, packet-db length 8, current packet-size framing, and the `HandleSpirits2` route are verified.

The checker verifies canonical EXE/Ref `CGameActor::GetJob` equality and confirms that the getter does not transform job IDs 4038 or 24. The branch comparison maps the sources to current implementation in order:

1. Effects-enabled guard.
2. Local-player resolution on self AID; actor lookup otherwise; null actor return.
3. Hidden/burrow branch removes the job-specific effect (4038→504, 24→629, default→228), without launch.
4. Visible branch selects the same job-specific effect and checks the attached-effect list for an existing matching effect type.
5. Only when that effect is absent and AID is nonzero, zero-initialize the position/offset and launch at zero offset.
6. Set position X from the signed short at packet `+6`; send message 14 with a null sender only when launch succeeds.

The source-side helper traverses the effect list, skips null entries, matches `m_type`, and returns on the first match or false at end. Current `EffectExists` uses the same list, null skip, and `GetEffectType()` getter (`GetEffectType()` returns `m_type`). The packet wrapper adds malformed-packet and missing-world guards around the source logic.
