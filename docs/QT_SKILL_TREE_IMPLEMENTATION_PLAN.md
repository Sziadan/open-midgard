# Qt skill tree and staged allocation plan

Status: implemented. See `QT_SKILL_TREE_VALIDATION.md` for catalog provenance, completed checks, and remaining live gameplay checks.

## Requested behavior

Replace the current skill list with a connected skill tree that uses the shared Qt window, control, typography, icon, selection, and tooltip design. Show the complete skill tree for the current job, including inherited skills and unlearned/locked skills. Do not show skills belonging only to future job advancements.

Each node has a skill name above its icon and a level selector below it: `< X1 / X2 >`.

- X1 is the selected cast/drag level, not the number of points to allocate.
- X2 is the confirmed learned level plus any unapplied points assigned to that skill. It is not the skill's absolute level cap.
- Both numbers are black by default. X1 is red when lower than X2. X2 is red while it includes unapplied points.
- For Heal learned to 7 with a cap of 10, the selector initially reads `7 / 7`; selecting level 3 reads `3 / 7`, with 3 red. The cap of 10 appears in the skill details/tooltip.
- A new unlearned skill reads `0 / 0` with disabled arrows. A pending first point can show `1 / 1` with the second 1 red.
- Clicking the icon stages one point. Dragging an icon to a shortcut uses X1 and must not also allocate a point.
- Replace the bottom Use and Close buttons with Apply and Reset. Keep the shared title-bar close/collapse controls.

## Findings in the current code and server

- `UISkillListWnd::GetSortedSkills()` deliberately filters out skills that are neither learned nor currently upgradeable. Its row model holds pointers to server-listed skills, so it cannot represent a complete tree of locked skills.
- `CSkillMgr::SkillMetadata` supplies names, descriptions, icons, and SP costs, but no complete job membership/prerequisite graph. `PLAYER_SKILL_INFO::needSkillList` exists, but the current packet path does not populate such a graph.
- `ApplySkillMetadata()` currently estimates `skillMaxLv` from the number of SP-cost entries. That is insufficient as the authoritative job-specific cap, particularly for passive skills.
- The local rAthena repository has job trees, inheritance, job-specific maximum levels, prerequisites, and base/job level requirements in its skill-tree databases. Imports and server configuration must be included.
- Packet version is configured as `20080910`. The server source also enables Renewal by default in `src/config/renewal.hpp`; an old packet version does not establish which ruleset the running server uses. Confirm the actual build flags and active database before generating the client catalog.
- In both inspected Acolyte databases, Increase Agility requires Heal 3; Decrease Agility requires Increase Agility 1. Heal 3 is therefore a transitive prerequisite of Decrease Agility.
- `RequestUpgradeSkillLevel()` sends `PACKET_CZ_SKILLUP` for one skill increment. rAthena's `pc_skillup()` consumes one point and sends skill/point updates. This is not a batch transaction.
- `RequestShortcutSlotUse()` currently prefers the highest learned level over the level saved in the shortcut slot. That must change for the requested level selector to affect actual casting.

## 1. Complete, server-aligned tree catalog

Build a versioned client catalog from the configured rAthena server data, rather than deriving trees from the current skill-list packet or SP-cost tables.

- Resolve the active Renewal/pre-Renewal database and `db/import` overrides, job inheritance, exclusions, skill IDs, and job IDs.
- Export skill identity, job membership, actual maximum level, ordered prerequisite skill/level pairs, base/job level gates, and any known non-point requirements.
- Include a source revision/hash, schema version, ruleset, and packet profile identifier. Make regeneration repeatable after server database changes.
- Ship the generated catalog with the client; do not make the running client depend on `C:\Users\sziad\source\repos\rathena-master` or access to server source files.
- Recommended format: versioned JSON, loaded by the Qt integration using `QJsonDocument` into plain C++ structures. Keep graph/planning logic independent of Qt and network code.
- Validate missing IDs, invalid required levels, impossible caps, inheritance, and cycles during export and load. Display a clear data error for invalid trees; never guess and spend points.
- Merge this catalog with server-confirmed learned levels and points. Server packets remain authoritative for actual learned state.
- Distinguish quest-granted, temporary, copied, equipment-granted, and other non-point skills. Display relevant skills with their actual restrictions; do not treat them as ordinary allocation targets. Homunculus/mercenary trees remain separate from the player tree.

## 2. Separate confirmed state, draft state, and selected cast levels

Introduce a skill-tree controller with three independent sets of state:

- **Confirmed:** server-reported levels, available points, current job/levels, and skill flags.
- **Draft:** pending increments per skill and a dependency-valid ordered allocation history.
- **Selection:** the desired cast level per skill, independent of pending allocations.

For each skill, derive its projected level from confirmed level plus pending increments. Derive remaining draft points from confirmed available points minus reserved draft points. Do not modify `CSession`'s learned skills while previewing allocations.

Unlock previews use projected prerequisite levels, allowing a draft to unlock further skills before Apply. Base/job level and quest restrictions still apply and cannot be satisfied by allocating an unrelated skill point.

Keep draft state per character in memory while the window is merely closed/reopened. Clear or invalidate it with an explanation on logout, character/job change, server skill reset, or incompatible state changes. Reconcile other authoritative updates before allowing Apply. Closing the window must never apply points implicitly.

## 3. Dependency allocation: one point per click

For an icon click:

1. Reject allocation if no draft points remain, the target is capped, or its non-point restrictions cannot be met.
2. If the target is unlocked using projected levels, stage exactly one level on it.
3. Otherwise traverse unmet prerequisites in a deterministic order from the catalog. Recursively find the first prerequisite that is itself available and below the level required along that path.
4. Stage exactly one point on that prerequisite. Record which skill received it and why, and briefly emphasize that node/path.
5. Recompute projected unlocks, highlights, point totals, and pending labels.

Repeated clicks on Decrease Agility from zero produce this exact allocation sequence: Heal 1, Heal 2, Heal 3, Increase Agility 1, Decrease Agility 1. The first click does not allocate the entire chain. Shared prerequisites receive only their missing required levels; the planner does not automatically max them.

Use a cycle guard and deterministic ordering. If no valid point-spend path exists because of a quest/job-level gate, absent prerequisite, or invalid catalog, explain the block and spend nothing.

## 4. Actual tree layout and visual states

- Use a scrollable/pannable tree viewport with dependency lines, stable node positions, and a persistent footer. Size the viewport for the current client/UI scale rather than forcing the whole graph onto a tiny list-sized panel.
- Group inherited job branches clearly within the current job's full tree. All skills remain represented; scrolling/panning reaches nodes outside the viewport.
- Prefer authored positions for common jobs, with a stable layered graph layout as the fallback. Store layout metadata separately from gameplay prerequisites. Avoid excessive line crossings and positions that jump after each allocation.
- Use one geometry source for node bounds, icons, arrows, viewport transforms, and hit testing. Do not independently scroll QML visuals while C++ continues testing unscrolled coordinates.
- Unlearned icons are greyed out. Learned icons retain their normal art. A lock indicator distinguishes unmet prerequisites from an unlocked but unlearned skill.
- With draft points remaining, an unlocked skill below its actual job cap has a distinct upgrade border/badge. A pending skill also has an unapplied indicator; pending, available, and prerequisite states must be distinguishable without color alone.
- Hovering an unlearned skill highlights its dependency path, including transitive prerequisites. Display required/current projected levels on the prerequisite nodes and/or links, e.g. `Heal 2 / 3 required`, distinguishing fulfilled and missing requirements.
- Show non-skill gates, the absolute cap, current/projected learned levels, and description in a readable tooltip/details area. Keep locked nodes readable and their details accessible.
- Reuse the shared header, tabs where needed, buttons, focus/hover states, and grey tooltips. Do not introduce a separate theme for the tree.

## 5. Level selection, dragging, and shortcut execution

- The arrows change X1 only. They never allocate or remove points.
- Initialize X1 to the confirmed learned level, with 0 for unlearned skills. Preserve the selected level when more points are staged: staging Heal from 7 to 8 initially shows `7 / 8`, with both numbers red. The one exception is staging the first point on an unlearned skill, which initializes X1 from 0 to 1. The arrows can select any level from 1 through projected X2, with confirmed-only dragging enforced below.
- Clamp X1 after Reset or authoritative changes. Store the selection per skill for the character's current session.
- A normal click/release on an icon stages an allocation. Moving past the drag threshold starts dragging and cancels allocation from that gesture. Arrow clicks must not arm dragging.
- Passive skills remain visible and allocatable but cannot be dragged as usable shortcuts. Preserve right-click skill descriptions.
- Only confirmed learned levels can be used or saved to a shortcut. If X1 selects a pending-only level, show `Apply these points before using this level` and prevent the drag. Confirmed levels remain draggable even if the skill has additional pending points.
- Pass X1 through `DRAG_INFO.skillLevel` into the shortcut slot. Fix shortcut use to honor the stored level, clamped to the server-confirmed learned level, across self, actor-targeted, ground-targeted, and delayed-targeting paths.
- Show the saved cast level in shortcut displays/tooltips. If a server reset makes a stored level invalid, clamp or disable it until valid again. Do not substitute the highest learned level for every shortcut.

## 6. Apply and Reset

**Reset** discards only unapplied draft increments, restores projected levels/point totals, and clamps selected levels as needed. It sends no skill-up packets and cannot undo confirmed allocations.

**Apply** revalidates the entire draft against the latest confirmed state, then submits a queue of one-point requests in dependency order:

1. Freeze draft editing and expose an applying/progress state; prevent double Apply.
2. Send one increment and wait for the expected skill-level confirmation. Reconcile the accompanying point/skill-list updates, which may arrive in a different order.
3. Continue only when confirmed state makes the next request valid. Keep in-flight points reserved so an intermediate point update cannot create extra spendable points.
4. Treat a full skill-list refresh as state synchronization, not proof that an arbitrary request succeeded. Match expected skill and level changes.
5. On completion, clear pending indicators and update the confirmed point budget. X2 becomes black once the displayed level is confirmed and has no remaining pending increments.
6. On rejection, timeout, or disconnect, stop the queue, refresh/reconcile, and clearly identify applied versus remaining points. Confirmed increments cannot be rolled back by Reset. Discard or revalidate remaining invalid increments rather than continuing blindly.

Apply is disabled with no draft or with invalid state. Reset is disabled with no draft and while a request is in flight. Retain pending changes when the window closes during an idle draft; keep the apply coordinator independent of visibility so closing cannot corrupt an in-flight sequence.

The current protocol cannot promise atomic all-or-nothing Apply. A single atomic server transaction would require a separate server/protocol feature; this implementation should use safe sequential confirmation with the existing server.

## Implementation order and code areas

1. **Catalog and rules:** add exporter/catalog validation and plain C++ graph structures. Verify the actual server profile and inherited Acolyte tree first.
2. **Draft planner:** implement projected levels, budget accounting, deterministic recursive allocation, reset, and authoritative-state reconciliation with focused unit tests.
3. **Apply coordinator:** integrate `GameMode.cpp`, `GameModePacket.cpp`, and relevant `CSession` updates. Test ordered acknowledgements, failure, timeout, and disconnect before exposing Apply in the GUI.
4. **Tree UI:** replace `UISkillListWnd`'s row/pointer model with tree nodes/edges and the shared viewport geometry. Extend `QtUiStateAdapter.cpp`; extract tree/node presentation into dedicated QML components registered in the resource collection.
5. **Level selector and shortcuts:** implement X1/X2 styling, click-versus-drag behavior, confirmed-only shortcut assignment, and level-respecting execution.
6. **Polish and validation:** remove bottom Use/Close, add Apply/Reset and point/progress summary, validate job coverage and interaction at supported UI scales, then build with the MinGW Qt preset and deploy to `D:\Spel\OldRO`.

## Acceptance checks

- A fresh character sees learned, unlearned-unlocked, and locked skills in its current/inherited job tree, with no future-job skills mixed in.
- Hovering Decrease Agility shows Heal 3 and Increase Agility 1 along the dependency chain, with accurate fulfilled/missing amounts.
- Five repeated clicks from zero stage Heal 3, Increase Agility 1, and Decrease Agility 1, consuming five draft points and sending zero upgrade packets before Apply.
- One point remaining stages only one valid increment. Maxed, quest-only, impossible, and cyclic dependencies never consume an inappropriate point.
- Pending allocations unlock subsequent draft paths; Reset returns to the confirmed state and never sends packets.
- Apply handles prerequisites in the correct order, does not double-submit, and stops safely after partial failure. Test out-of-order point/list updates and absent acknowledgements.
- Heal confirmed at 7 can be selected/dragged at 3, displayed as `3 / 7`, and actually cast at level 3 from the shortcut bar.
- Staging Heal 8 makes X2 red; Reset restores 7; Apply makes 8 confirmed/black. X1's lower-than-X2 color remains correct independently.
- An unlearned/pending-only skill cannot be used before confirmation. Icon dragging does not spend a point, and an icon click does not accidentally use a skill.
- Verify inherited/advanced jobs, passive skills with no SP-cost table, quest and granted skills, no points, changed jobs, skill resets, long names, and missing icons/catalog entries.
- Render-check Basic Info, Options, and the tree together to verify shared styling, at 100%, 125%, 150%, and 200% UI scale. Exercise real clicks, panning, arrows, drag/drop, Apply/Reset, and server confirmations in a live session.

The catalog includes all server job trees, including future jobs not currently supported by the client. The window selects only the current job's tree and its inherited skills.
