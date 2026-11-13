# Pending companion actor skills

With the C++ controller introduced by `a40a4c01`, the pending actor input queues `SKILL_OBJECT_CMD` on the owned companion. Shift uses the reserved command queue. The command carries the selected level and actor GID independently of the player's pending cursor state. If the companion actor is unavailable, the click is still consumed.

`PendingCompanionActorSkillReplay` verifies an immediate mercenary command and a reserved homunculus command through the production controller, each producing one 10-byte `0438` request. It also checks the no-actor and ordinary player paths. The selected rAthena packet registration is `clif_packetdb.hpp:1146-1152`. [Ground command evidence](PK28-companion-ground-targets.md)

Selected rAthena `db/pre-re/skill_db.yml` defines 8238, 8239 and 8240 as support skills, and `db/mercenary_db.yml` assigns them to mercenaries. The replay feeds all three skills through `029D`, arms the skill from its learned row, and checks the owner GID in `0438` from the C++ controller. ID 8241 has no target type in the selected skill DB and is filtered from the list. The source-level settings range and contract retirement use the same usable mercenary ID bound.

The selected `Ref/GameMode.cpp` actor pick handlers intercept homunculus skill IDs 8000 through 8016 when the homunculus is alive, and mercenary skill IDs 8200 through 8237 when the mercenary is alive and the actor attack gate permits the branch. They send AI command 7 only when the companion actor and Lua VM are available. The branch consumes the click even when either is unavailable. See object pick around 6277 and 6370, monster pick around 14368 and 14458, and PC pick around 15153 and 15249. The selected executable uses the inactive VM described in the companion identity evidence.

The previous port let these ordinary actor targets reach `BeginSkillChase` and player messages 41 and 90. The source interception now runs before that player route. Monster Alt target writes follow the source family ordering. A pending homunculus skill stops before the mercenary target branch. PC pending skill targeting keeps its separate modifier behavior.

The ground branch differs. The mercenary command 8 path around `Ref/GameMode.cpp:5596` returns early only after successful AI dispatch. With the VM unavailable it reaches the generic ground-skill path at `:5619`. That fallback remains in place.

Replay uses the production actor skill handler with no companion AI actor and a recording player. It checks homunculus and mercenary skill consumption, homunculus family precedence on Alt, PC targets, no player commands or outgoing socket bytes for the intercepted cases, and retention of ordinary player skill commands. All 164 application checks and 473 standalone checks passed. The player skill tree and OldRO executable matched baseline. Map-type ally gates and live input verification remain open.
