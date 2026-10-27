# `CPlayer::ProcessProceed` source and current-route comparison (2026-10-27)

## Canonical source gate

- Ref: `Ref/Player.cpp:742`, source address `0x005E4920`.
- Canonical executable decompilation: `HighPriest.exe.c:415575`, VA `0x005E4920`.
- `tools/tests/check_player_proceed_source.py` brace-extracts the function definitions, removes comments, collapses whitespace, compares complete bodies, and extracts ordered switch case labels.
- Ref and EXE normalized bodies are equal: 11,462 characters; SHA-256 `ed080d5eeab20024709f8ed6223f6388c6a663d1d3b45ad8886075cd44418e2b`.
- Ordered case IDs in both: `PT_ATTACK`, `PT_ATTACK_2`, `PT_PICKUPITEM`, `PT_SKILL`, `PT_GROUNDSKILL`, `PT_TOUCH_SKILL`.
- The first two labels share one body. The five unique bodies following that alias have normalized lengths 3,861, 1,119, 2,599, 1,045, and 1,656 characters, respectively.

## Branch-level current comparison

| Source branch | Required behavior from Ref/EXE | Current route | Result |
|---|---|---|---|
| `PT_ATTACK` / `PT_ATTACK_2` | Target lookup and state/job gates; 3D range test with forced-state adjustment; 450/1200 ms chase delay; source cell conversion and eight-sector approach; attack request timeout and message 29/40 | Modern attack chase runs in `PumpAttackChaseRequest` (`src/gamemode/GameMode.cpp`) and uses `ResolveAttackChaseCell`, `IsAttackTargetWithinRange`, and modern packet timing | Not compared branch-for-branch. Attack is outside the skill-only port scope; retain as an explicitly excluded source branch in this function audit. |
| `PT_PICKUPITEM` | Resolve item; normalize player/item cells; only proceed in states 0 or >=16; adjacent-cell message 16 and one request per NAID; otherwise message 91; missing item cleanup message 13 | `TryBeginGroundItemPickup` and pending pickup processing in `src/gamemode/GameMode.cpp` | Not compared branch-for-branch. Pickup is outside the skill-only port scope; retain as an explicitly excluded source branch in this function audit. |
| `PT_SKILL` | Resolve self/actor target; closest safe cell with skill range; state gate `<2 || ==4 || >=16`; 3D range `5*range+5.5`, actor type 1 subtracts 20; inside range sends 42 then cleanup 13; otherwise source destination/sector approach and message 91 | `TryRequestPendingSkillFromScreenPoint` arms current chase state; `PumpSkillChaseRequest` applies source projection/fallback order, state and 3D range gates, actor correction/sector table, destination shortcut, packet dispatch and strict 1000 ms resend. | **Passed branch-level source/current comparison.** `tools/tests/check_player_pt_skill_route.py` gates Ref/EXE branch and helper equality plus current operation order. Coincident X/Z uses a safe player-cell fallback because the source helper divides by zero. |
| `PT_GROUNDSKILL` | Convert selected server cell to world; closest safe cell with skill range; source state gate and same 3D threshold/current-destination shortcut; in range message 66 (then mode message 77); otherwise update destination/message 91 | Ground selection now arms the original cell, raw skill range and level; the modern pump applies source projection/fallback order, state/range/current-destination gates, movement destination and resend timing, then sends the position-skill packet for the original cell. | **Passed branch-level source/current comparison.** `tools/tests/check_player_pt_ground_skill_route.py` checks Ref/EXE branch equality and current route order. Coincident X/Z uses the player-cell fallback for the source helper's undefined division. Legacy text-entry windows for IDs 125/220 remain excluded UI paths. |
| `PT_TOUCH_SKILL` | Resolve skill-unit AID; source state/range checks and closest safe cell; in range message 42→76, except jobs 141/143/152 with skill ID 0 use message 29; pursue with message 91, with state-2 exception for those jobs | No current equivalent touch-skill proceed route found | **Open, in scope.** |

The source uses message 42 to dispatch actor-target/touch skills and message 66 for ground skills. Ref `CPlayer::SendMsg` maps these to mode messages 76 and 77. The legacy trap-input and graffiti windows for skill IDs 125 and 220 are excluded UI behavior; those cases do not block porting the underlying gameplay route where packet semantics can be represented by the current client.

## Gate status

The Ref-to-EXE source gate passes. The targeted and ground skill branches have passed source/current comparison; the touch skill branch remains unresolved, so the complete `CPlayer::ProcessProceed` function gate remains open. Do not mark the function complete or begin another function under the plan's function gate until that branch is implemented or resolved with source evidence. This audit does not claim live packet acceptance.
