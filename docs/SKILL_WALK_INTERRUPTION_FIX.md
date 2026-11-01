# Skills interrupting a walk — 2026-11-01

Ordinary accepted skill use should stop at the current server cell and start the skill pose there. It must not complete the outstanding click-to-walk path instantly. Selecting a skill or sending a request alone does not move or stop the actor; out-of-range approach and failed requests retain their existing handling. Running/movement skills retain their own server behavior.

Evidence from the local server: `C:/Users/sziad/source/repos/rathena-master/src/map/unit.cpp:2475` calls `unit_stop_walking(src, USW_FIXPOS)` before `clif_skillcasting`, except for running units. `unit_stop_walking:1695` cancels the walking timer, centers subcell offsets, sends the current `bl->x/y` via `clif_fixpos`, then clears the path and sets its destination to that current cell. `clif.cpp:2203` serializes that cell as ZC_STOPMOVE (0x0088).

Original client evidence: `HighPriest.exe.c:362037` and `Ref/GameModePacket.cpp:27330` (`Zc_Stopmove`) dispatch message19 with the packet's signed x/y, for either self or another actor, without filtering cells behind the displayed path segment. `Ref/GameModePacket.cpp:33843` (`Zc_useskill_ack`) changes skill state and launches casting feedback; it does not place the actor at the pending walk destination. `Ref/Pc.cpp:5678` (`CPc::SetState`) preserves the current position when entering skill states (the separate hit-reaction completion position is unrelated).

Two implementation defects combined:

- `HandleSelfMoveAck` saves the acknowledged destination in `g_session.m_playerPosX/Y`. Every active `CGameMode::OnRun` called `SyncBootstrapSelfActorWorldPos`, which overwrote the actor's position with that destination whenever `m_isMoving` became false. Accepting a skill clears that flag, causing the observed jump.
- `HandleActorSetPosition` could discard the authoritative stop when its cell was at or behind the current path segment, leaving the stale destination and path in place.

Removed the active-frame bootstrap placement call; initial placement remains in `AdvanceMapLoading`. Removed the stop-discard filter and its unused helper functions. The existing position packet application updates session/chase cells, resets the walk and places the actor at the server-provided cell. No guessed stop packet, destination-based placement or immediate local stop on skill intent was added.

Validation: `SkillWalkStopTests.cpp` compiles the production stop-packet and position-application functions. A stop on a cell behind the displayed segment now cancels the walk, clears its path, updates session/chase position, clears the waiting flag and replaces the movement endpoint with the confirmed cell; skill/idle poses retain that position. Invalid and truncated packets leave it untouched. `check_skill_walk_frame.py` verifies gameplay does not call bootstrap placement while map loading still does. Both focused checks pass. The MinGW Qt client build succeeds. Full parity CTest configuration has the pre-existing packet actor-lookup extraction issue recorded in earlier validation notes; no full-suite pass is claimed. Live validation remains with the user.
