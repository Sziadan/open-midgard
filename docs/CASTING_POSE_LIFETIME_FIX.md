# Casting pose lifetime

The rebuilt billboard selector treated the casting action as a one-shot clip.
After its motion-count timer elapsed, IsTransientActionActive returned false and
the player-style billboard selected idle even though actor state 8 was still
casting. The recharge-bar overlay also forced idle when its estimate expired.

State 8 now keeps its current action selected independently of clip duration.
Ref/Pc.cpp:5853 sets action 96 and marks the motion finished, holding the casting
pose; Ref/GameActor.cpp:1911 advances state 8 without a clip-duration transition
to idle. The change restores that separation between actor state and the
billboard renderer's one-shot lifetime. Attack/use animation timers are unchanged.

The progress bar may expire at its advertised time, but it no longer changes
actor state. The server's cast result, interruption, failure, movement or death
transition determines the displayed action. The cast-cancel handler explicitly
returns casting states 8/13 to idle and releases the casting freeze; a late
cancel does not replace a subsequent attack, hit or death pose.

CastingPoseTests.cpp compiles the production action-lifetime selector, recharge
overlay and cast-cancel receiver. Checks hold a three-frame action through a
ten-second cast and beyond bar expiry, confirm ordinary use/idle states stop
being retained, and verify interruption plus late-cancel behavior. Run through
tools/tests/check_ammo_cursor_regressions.py. These focused checks pass; live
gameplay confirmation remains for the user.
