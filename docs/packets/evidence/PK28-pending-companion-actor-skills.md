# Pending companion actor skills

The current baseline uses the native C++ controller introduced by `a40a4c01`. The branch and tests below predate that controller. The pending actor input still consumes the click without producing `SKILL_OBJECT_CMD`. Add a native command producer and verify it through the controller and wire sender, keeping the no-actor path and the player route covered.

The selected `Ref/GameMode.cpp` actor pick handlers intercept homunculus skill IDs 8000 through 8016 when the homunculus is alive, and mercenary skill IDs 8200 through 8237 when the mercenary is alive and the actor attack gate permits the branch. They send AI command 7 only when the companion actor and Lua VM are available. The branch consumes the click even when either is unavailable. See object pick around 6277 and 6370, monster pick around 14368 and 14458, and PC pick around 15153 and 15249. The selected executable uses the inactive VM described in the companion identity evidence.

The previous port let these ordinary actor targets reach `BeginSkillChase` and player messages 41 and 90. The source interception now runs before that player route. Monster Alt target writes follow the source family ordering. A pending homunculus skill stops before the mercenary target branch. PC pending skill targeting keeps its separate modifier behavior.

The ground branch differs. The mercenary command 8 path around `Ref/GameMode.cpp:5596` returns early only after successful AI dispatch. With the VM unavailable it reaches the generic ground-skill path at `:5619`. That fallback remains in place.

Replay uses the production actor skill handler with no companion AI actor and a recording player. It checks homunculus and mercenary skill consumption, homunculus family precedence on Alt, PC targets, no player commands or outgoing socket bytes for the intercepted cases, and retention of ordinary player skill commands. All 164 application checks and 473 standalone checks passed. The player skill tree and OldRO executable matched baseline. Map-type ally gates and live input verification remain open.
