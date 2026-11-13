# Native homunculus and mercenary AI

Commit `a40a4c01` introduced the C++ companion controller. `TickOwnedCompanionActor` activates the controller for the matching owned actor GID while its session status is alive. `CompanionAiBrain.cpp` manages command order, movement, and target selection. `CompanionAiRuntime.cpp` reads actor state and sends the selected map-server requests.

The selected rAthena `src/map/clif.cpp` routes `0233` through `clif_parse_HomAttack`. It resolves the actor as the sender's active homunculus or mercenary before passing the target GID to `unit_attack`. The client now excludes dead, hidden, and owned actors from basic attack selection and uses the chase sight limit during automatic scans. Skill targeting still allows support skills to select the owner. `PendingCompanionActorSkillReplay` checks the 11-byte attack request for an aggressive homunculus and for mercenary owner defense with a closer corpse present.

The selected rAthena `clif_parse_HomMoveTo` handles `0232` by checking companion ownership and calling `unit_walktoxy`. Invalid and same-cell requests return silently. The movement path can fail without a dedicated result packet, so a successful client socket write alone cannot confirm movement.

The native controller uses observed cell progress for actor and ground skill approaches. It allows the larger of one second or three walk-cell durations for progress, retries once after a stall, and releases the pending cast after another stalled interval. A local send failure releases it immediately. Replay uses an explicit tick time and a slower companion to verify that partial movement defers the retry, the reserved ground cast can proceed after the stall, and the actor approach stops emitting packets. Existing replay still covers reaching cast range and waiting through a busy animation.

PK28.04 and PK29.04 stay in progress. Audit queued-order cleanup on death, dismissal, and re-entry alongside the existing packet snapshots and window tests. The remaining live checks cover both companion families across combat, follow, dismissal, and map changes. Installed skin comparison remains a separate UI check.
