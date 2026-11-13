# Packet implementation status

**Quest journal**. PK50.02 receives character quest membership and mission details with atomic validation. PK50.03 applies incremental updates and opens the journal with Q or the Quest button in Basic Info in native and Qt gameplay. Installed-skin and live verification remain open. [Quest updates and UI evidence](evidence/PK50.03-quest-incremental.md)

**Battleground chat**. PK41.03 verifies the command, selected send and receive paths, autosave and same-map reset in replay. Live team acceptance remains open. [Evidence](evidence/PK41.03-battleground-chat.md)

**Battleground state**. PK41.02 receives camp identity, scores, team HP and teammate minimap markers. The shared world renderer queues the original opposing-camp bitmap. Installed-client capture and live acceptance remain open. [Evidence](evidence/PK41.02-battleground-state.md)

**Storage password packets**. PK36.01 records the registered layouts and classifies the challenge flow as inactive in the selected server source. PK46 and PK47 carry that finding while review of other profiles remains open. [Evidence](evidence/PK36.01-storage-password-applicability.md)

**Relationship packets**. PK30.04 replays the job and skill updates after adoption and verifies both candidate prompts and parent-menu eligibility in native and Qt. Independent review and live verification remain open. [Evidence](evidence/PK30.01-relationship-wire.md)

**Social packets**. PK16.04 reviews the selected privacy and social paths and verifies equipment lookup refusal. PK34 validates and replaces friend snapshots atomically. Independent review and live verification remain open. [Verification evidence](evidence/PK16.04-verification-boundary.md)

**Privacy configuration**. PK16.02 wires the equipment control and privacy commands to selected server-confirmed settings. PK16.03 still owns the social packet audit. [Evidence](evidence/PK16.01-privacy-wire.md)

**Companion movement recovery**. PK28.04 and PK29.04 verify that stalled basic attack pursuit releases queued orders and delays automatic reacquisition. Manual movement has bounded recovery. Forced follow retries during attack animations and uses the same interval as automatic follow. Live verification remains open. [Evidence](evidence/PK28-PK29-native-companion-ai.md)

**Native companion lifecycle orders**. PK28.04 and PK29.04 cover order cleanup on death, vaporize, deletion, identity replacement, and same-map reuse. Same-GID mercenary refresh preserves orders. Live verification remains open. [Evidence](evidence/PK28-PK29-native-companion-ai.md)

**Companion skill approach recovery**. PK28.04 and PK29.04 verify stalled actor and ground approaches, movement progress, and the release of a reserved cast after a failed approach. Queued-order cleanup across lifecycle transitions and live verification remain open. [Evidence](evidence/PK28-PK29-native-companion-ai.md)

**Native companion targeting**. PK28.04 and PK29.04 verify that an aggressive homunculus and a mercenary defending its owner pick living targets when a closer corpse is present. Six focused application checks and 473 standalone checks passed. Movement failure recovery and live verification remain open. [Evidence](evidence/PK28-PK29-native-companion-ai.md)

**Companion skill use levels**. PK28.04 verifies level controls, family-specific saved levels, SP cost, and cast and drag payloads. The production replay, native/Qt render fixture, 473 standalone checks, and Qt lint passed. [Evidence](evidence/PK28-companion-skill-lists.md)

**Companion list skin**. PK28.04 verifies the frame, Mini button, scrollbar, and resize grip with paired native and Qt captures using synthetic bitmap fixtures. The production packet replay also passed. Live comparison with the installed skin remains open. [Evidence](evidence/PK28-companion-skill-lists.md)

**Companion skill list resizing**. PK28.04 verifies resize snapping, visible row updates, and independent geometry restore. Six focused application checks, 473 standalone checks, and Qt lint passed. [Evidence](evidence/PK28-companion-skill-lists.md)

**Companion window captions and placement**. PK28.04 uses the selected message entries and restores separate positions for both families. Six focused application checks and 473 standalone checks passed. Layout modes, scrollbar parity, and live UI verification remain open. [Evidence](evidence/PK28-companion-skill-lists.md)

**Companion windows on map transition**. PK28.04 replay verifies that same-map reuse closes both companion families' controls, clears learned rows and actor jobs, and cancels a companion skill drag. Six focused application checks and 473 standalone checks passed. [Evidence](evidence/PK28.03-companion-map-reset.md)

**Mercenary identity refresh**. PK29.04 verifies same-GID refresh, replacement cleanup, and job resolution after identity. Six focused application checks and 473 standalone checks passed. [Evidence](evidence/PK29.04-mercenary-refresh.md)

**Selected elemental lifecycle (2026-11-13)**. PK29.03 replay covers the `081D` snapshot, type `0x0A` spawn and movement, ordinary visibility removal, and snapshot retirement on owner death and map reset. Six focused application checks and 473 standalone checks passed. [Evidence](evidence/PK29.03-elemental-snapshot.md)

**Native companion attack cancellation (2026-11-13)**. PK28.04 replay verifies manual attack and cancellation for both companion families. Six focused application checks and 473 standalone checks passed. The main replay also passed two consecutive reruns. [Evidence](evidence/PK28-companion-actor-targets.md)

**Native companion cast progress (2026-11-13)**. PK28.03 replay verifies busy-animation retention for actor and ground skills and the mercenary ground approach within the legacy move limit. Six focused application checks and 473 standalone checks passed. Live validation and long approaches remain open. [Evidence](evidence/PK28-companion-ground-targets.md)

**Native companion skills (2026-11-13)**. PK28.03 and PK29.02 replay cover immediate and reserved actor commands, owner-targeted mercenary support skills 8238–8240, and an in-range mercenary ground skill. Six focused application checks and 473 standalone checks passed. Out-of-range casts and live validation remain open. [Actor evidence](evidence/PK28-pending-companion-actor-skills.md) and [ground evidence](evidence/PK28-companion-ground-targets.md)

**Native companion checkpoint (2026-11-13)**. PK29.02 replay covers contract retirement and selected `029B` and `02A2` stat updates. Reserved ground orders preserve the current target. Six focused application checks and 473 standalone checks passed. Native pending skill dispatch and live contract verification remain open. [Evidence](evidence/PK29.02-mercenary-contract-end.md)

**Selected map property verification (2026-11-11)**. PK19 covers the active `0199` and `01D6` layouts, world lifetime, gameplay rules, and recovery and siege presentation. The `02E7` compatibility receiver has synthetic replay, and the newer `099B` layout is inactive for the selected profile. Independent review and live verification remain open. [Evidence](evidence/PK19.04-map-properties-verification.md)

**Standing resurrection render checkpoint (2026-11-11)**. Native bitmap and Qt captures show the recovery choice for an eligible character. The event PvP packet removes the row from both menus. All 165 application and 473 standalone checks passed. [Evidence](evidence/PK19-recovery-menu-rendering.md)

**Native siege emblem checkpoint (2026-11-11)**. Vulkan captures verified the downloaded and legacy graffiti badges, with no emblem pixels outside siege, during concealment, or for a mismatched version. [Evidence](evidence/PK19-siege-emblem-native-verification.md)

**Previous siege actor emblem checkpoint (2026-11-11)**. The actor packet and concealment paths control siege emblem eligibility. Production lifecycle replay passed. [Evidence](evidence/PK19-siege-actor-emblems.md)

**Standing resurrection action checkpoint (2026-11-11)**. The dead-player menu now offers item 7621 recovery in eligible maps and sends the selected two-byte request. Packet and Qt state replay passed. Live recovery and rendered menu acceptance remain open. [Evidence](evidence/PK19-standing-resurrection.md)

**Map property lifetime checkpoint (2026-11-11)**. Real game-mode entry resets the four world rule flags. Loaded same-map reuse retains them, and session values follow their packet handlers. [Evidence](evidence/PK19-map-property-lifetime.md)

**Dynamic map cell native verification (2026-11-11)**. Vulkan captures on installed Prontera assets verified that `0192` removes the hover highlight on a blocked cell and restores it when the cell is walkable. Production pathfinding and lifecycle replay passed. Independent PK48 and live server checks remain pending. [Evidence](evidence/PK18.04-map-cell-native-verification.md)

**Dynamic map cell loading checkpoint (2026-11-11)**. Matching `0192` updates are retained until GAT binding. Reuse retains the world state, and mode replacement starts a fresh terrain bootstrap. [Evidence](evidence/PK18.03-map-cell-loading.md)

**Dynamic map cell implementation checkpoint (2026-11-11)**. In a loaded world, `0192` now updates the effective GAT type used by movement, skill approach and the hover renderer. Early updates and rendered acceptance remain open. [Evidence](evidence/PK18.02-dynamic-map-cell-updates.md)

**Dynamic map cell source checkpoint (2026-11-11)**. PK18.01 establishes the selected `0192` layout, GAT type mapping, Ice Wall lifecycle, and the collision and hover consumers for implementation. [Evidence](evidence/PK18.01-dynamic-map-cell-scope.md)

**Current map property checkpoint (2026-11-11)**. The variable `02E7` receiver now applies bounded updates to the map property table. The selected server source registers the frame, and the receiver is verified with synthetic replay. [Evidence](evidence/PK19-map-property-row-updates.md)

**Previous map property checkpoint (2026-11-11)**. The legacy `0199` and `01D6` receivers now apply map state and the original ally gates feed gameplay targeting. All 164 application checks and 473 standalone checks passed. [Evidence](evidence/PK19-map-properties.md)

**Previous pending companion actor skill checkpoint (2026-11-11)**. Pending companion skills aimed at ordinary actors now follow the selected inactive AI branch and consume the click. All 164 application checks and 473 standalone checks passed. [Evidence](evidence/PK28-pending-companion-actor-skills.md)

**Previous companion actor targeting checkpoint (2026-11-11)**. Alt-clicking an eligible actor records companion selection and toggles an existing attack target. Actor death clears matching targets. All 164 application checks and 473 standalone checks passed. [Evidence](evidence/PK28-companion-actor-targets.md)

**Previous companion ground targeting checkpoint (2026-11-11)**. Ground clicks now retain the selected companion target transitions with the inactive Lua VM. All 164 application checks and 473 standalone checks passed. [Evidence](evidence/PK28-companion-ground-targets.md)

**Previous companion summon checkpoint (2026-11-11)**. The existing skill-list and shortcut senders cover Call Homunculus and mercenary scroll use. The selected wire requests are now covered by replay. All 164 application checks and 473 standalone checks passed. [Evidence](evidence/PK28-companion-summon-requests.md)

**Previous homunculus upgrade checkpoint (2026-11-11)**. The companion list sends the selected `0112` skill-up request when the server snapshot grants points and the row is upgradable. All 164 application checks and 473 standalone checks passed. [Evidence](evidence/PK28-homunculus-skill-upgrade.md)

**Previous localized system notice checkpoint (2026-11-11)**. The map receiver frames and displays `0291`, including selected mercenary expiry and death. All 164 application checks and 473 standalone checks passed. [Evidence](evidence/PK29-localized-system-notice.md)

**Previous companion skill list checkpoint (2026-11-11)**. The status buttons open independent packet-backed companion lists with scrolling, skill activation, descriptions, and shortcut dragging. All 164 application checks and 473 standalone checks passed. [Evidence](evidence/PK28-companion-skill-lists.md)

**Previous companion action checkpoint (2026-11-11)**. Homunculus feed, rename, and dismissal and mercenary dismissal now run through the selected status controls and confirmation callbacks. All 164 application checks and 473 standalone checks passed. [Evidence](evidence/PK28-companion-status-actions.md)

**Previous companion status window checkpoint (2026-11-11)**. Alt+R and Ctrl+R open the homunculus and mercenary status windows from the session snapshots. Homunculus death and the local skill 244 reset close its window. All 164 application checks and 473 standalone checks passed. [Evidence](evidence/PK28-companion-status-windows.md)

**Previous companion command checkpoint (2026-11-11)**. Mode messages 198 through 200 now send the selected companion commands. Skill 244 resets the local homunculus alive flag. All 164 application checks and 473 standalone checks passed. [Evidence](evidence/PK28-companion-commands.md)

**Previous companion lifecycle checkpoint (2026-11-11)**. Companion actor entries and homunculus death now update the session alive flags. Homunculus hunger warnings use the selected thresholds. All 164 application checks and 473 standalone checks passed. [Evidence](evidence/PK28-companion-lifecycle.md)

**Previous homunculus feed checkpoint (2026-11-11)**. The registered `022F` path now applies feeding success state and missing-food feedback. All 164 application checks and 473 standalone checks passed. [Evidence](evidence/PK28-homun-feed.md)

**Previous companion status checkpoint (2026-11-11)**. The selected homunculus and mercenary status packets now populate the session snapshots. All 164 application checks and 473 standalone checks passed. [Evidence](evidence/PK28-companion-status.md)

**Previous companion identity checkpoint (2026-11-11)**. The selected `0230` and `029B` receive paths now establish companion GIDs. All 164 application checks and 473 standalone checks passed. [Evidence](evidence/PK27-companion-identity.md)

**Previous ground prompt skin control checkpoint (2026-11-11)**. Gameplay Graphics settings now select Basic Skin, directory skins, and ZIP skins for the ground prompts. All 164 application checks and 473 standalone checks passed. [Evidence](evidence/PK27-prompt-picker.md)

**Previous ground prompt ZIP skin checkpoint (2026-11-11)**. The ground prompts now load selected ZIP skins in gameplay using the existing skin setting. All 164 application checks, 473 standalone checks, and 3 focused checks passed. [Evidence](evidence/PK27-prompt-skin-zip.md)

**Previous ground prompt skin checkpoint (2026-11-11)**. Ground prompt artwork and IME palettes now honor the selected directory skin in gameplay. All 164 application checks, 473 standalone checks, and 3 focused checks passed. [Evidence](evidence/PK27-prompt-skin.md)

**Previous ground prompt modern overlay checkpoint (2026-11-11)**. The Windows modern gameplay queue now submits the prompt and its IME popups with premultiplied alpha. All 164 application checks, 473 standalone checks, and 3 focused checks passed. [Evidence](evidence/PK27-prompt-modern-overlay.md)

**Previous ground prompt opacity checkpoint (2026-11-11)**. The Win32 prompts and their controls now fade as a single layer during drag and recover opacity on release. All 164 application checks, 473 standalone checks, and 3 focused checks passed. [Evidence](evidence/PK27-prompt-opacity.md)

**Previous ground prompt native paint checkpoint (2026-11-11)**. The Win32 prompt displays preedit with the source selection palette and blink. All 164 application checks, 473 standalone checks, and 3 focused checks passed. [Evidence](evidence/PK27-prompt-native-paint.md)

**Previous ground prompt keystroke checkpoint (2026-11-11)**. The Win32 ground prompts now render the Traditional Chinese keystroke cells and clear them when composition ends. All 164 application checks, 473 standalone checks, and 3 focused checks passed. [Evidence](evidence/PK27-prompt-keystrokes.md)

**Previous ground prompt candidate checkpoint (2026-11-11)**. The focused ground prompts can render Windows IMM candidates in the source layouts. All 164 application checks, 473 standalone checks, and 3 focused checks passed. [Evidence](evidence/PK27-prompt-candidates.md)

**Previous ground prompt IME checkpoint (2026-11-10)**. The ground prompts now render preedit independently of the committed text and support Qt IME cursor queries and replacement ranges. All 164 application checks, 473 standalone checks, and 3 focused checks passed. [Evidence](evidence/PK27-prompt-ime.md)

**Previous ground prompt clipboard checkpoint (2026-11-10)**. Talkie Box and Graffiti now route the original clipboard shortcuts through the selected service codepage. All 164 application checks, 473 standalone checks, and 3 focused checks passed. [Evidence](evidence/PK27-prompt-clipboard.md)

**Previous ground prompt selection checkpoint (2026-11-10)**. Talkie Box and Graffiti now support caret navigation, Shift selection, and replacement by typed text or an IME commit. All 164 application checks and 473 standalone checks passed. The focused suite passed all 3 checks. [Evidence](evidence/PK27-prompt-selection.md)

**Previous ground prompt encoding checkpoint (2026-11-10)**. Ground text now uses the service codepage on the wire and UTF-8 in the editor and skill bubble. All 164 application checks and 473 standalone checks passed. The focused suite passed 3 checks in 17.03s. [Evidence](evidence/PK27-prompt-codec.md)

**Previous ground prompt text checkpoint (2026-11-10)**. Native and Qt text now reach the focused ground prompt. All 164 application checks and 473 standalone checks passed. The focused suite passed 3 checks in 16.99s. Live IME, selection, and source packet encoding remain open. [Evidence](evidence/PK27-prompt-text.md)

**Previous ground prompt checkpoint (2026-11-10)**. Dragging, pointer release, typing and available palette sampling passed native replays. All 164 application checks and 473 standalone checks passed. The focused suite passed 3 checks in 18.69s, with native input and two palette renders. The full packet plan remains active. [Evidence](evidence/PK27-prompt-input.md)

**Previous ground proceed checkpoint (2026-11-10):** [Canonical65/66/77 and text owners](evidence/PK27-ground-proceed.md) replaces legacy ground transport, preserves raw levels/cells and adds source125/220 prompts84/101 through144. Skill tree unchanged. Deploy-OFF all-target and standalone builds pass; focused3/3 in17.85s, application164/164 in56.92s (-j4), standalone473/473 in13.17s (-j8). Twenty-two prior native/Qt frame pairs, two added native text-prompt frames and real Vulkan ground/capture/pending input pass. DeployON restored configure-only; OldRO unchanged. PK27/PK09.21 and the full plan remain open. Next text color-chip/keyboard/drag/focus/lifetimes, companion/control/picker and remaining plan gates.

**Previous source skill activation checkpoint (2026-11-10):** [Source UI producers and saved levels](evidence/PK27-skill-producers.md) restores backend list/shortcut126 dispatch, per-character raw levels and strict selector ownership. Skill tree window/model/catalog/QML/geometry are unchanged. Deploy-OFF all-target and standalone builds pass; focused3/3 in16.75s, application164/164 in56.44s (-j4), standalone473/473 in12.78s (-j8). Twenty-two native/Qt frame pairs and real Vulkan capture/pending input pass. DeployON restored configure-only; OldRO unchanged. PK27/PK09.21 and the full plan remain open. Next canonical ground65/66/77 and125/220 text owners, then broader companion/control/lifetime gates.

**Previous source pending-input checkpoint (2026-11-10):** [Source actor selectors and weight state](evidence/PK27-source-pending.md) connects stored selector/range/raw levels to actor/unit input and restores signed00B0 weight state/notices. Deploy-OFF all-target and standalone builds pass; focused3/3 in14.78s, application164/164 in53.56s (-j4), standalone473/473 in12.71s (-j8). Twenty-two native/Qt frame pairs and real Vulkan capture plus temporary-skill world input pass. DeployON restored configure-only; OldRO unchanged. PK27/PK09.21 and the full plan remain open. Next migrate legacy shortcut/list producers and remove the positive-level pending fallback, then ground65/66/77/text owners and broader companion/cursor/lifetime gates.

**Previous touch-skill checkpoint (2026-11-10):** [Canonical103 and virtual29/150](evidence/PK27-touch-proceed.md) restores source action dispatch and corrects a false base-no-op verifier assumption. Deploy-OFF all-target and standalone builds pass; focused2/2 in6.87s, application164/164 in51.51s (-j4), standalone473/473 in12.76s (-j8). Twenty-two native/Qt frame pairs and real Vulkan capture input pass. DeployON restored configure-only; OldRO unchanged. PK27/PK09.21 and the full plan remain open. General pending input, shortcut/list producers, ground65/66/77 and broader companion/picker/lifetime gates remain strong-agent work.

**Previous automatic skill checkpoint (2026-11-10):** [0147/126/79 selectors and level policy](evidence/PK27-autorun.md) replaces identify-only dispatch with exact source types/canonical self targets and repairs numeric SP-table tokens. Deploy-OFF all-target and standalone builds pass; focused2/2 in7.20s, application164/164 in53.46s (-j4), standalone473/473 in15.87s (-j8). Twenty-two native/Qt frame pairs and real Vulkan capture input pass. DeployON restored configure-only; OldRO unchanged. PK27/PK09.21 and the full plan remain open. General pending mouse/cursor/actor/ground/touch consumption, shortcut/list producers and broader interruption/companion/picker lifetimes remain strong-agent work.

**Previous shared player skill checkpoint (2026-11-10):** [PT_SKILL ownership and mode76 fallback](evidence/PK27-player-proceed.md) preserves old proceeds after rejected41 while90 overwrites level, dispatches through42/76/13 and restores temporary0147 storage. Deploy-OFF all-target and standalone builds pass; focused2/2 in7.01s, application164/164 in52.31s (-j4) and standalone473/473 in14.00s (-j8). Twenty-two native/Qt frame pairs and real Vulkan capture input pass. DeployON restored configure-only; OldRO unchanged. PK27/PK09.21 and the full plan remain open. General mode126, shortcut/list arming, attack/move/ground/touch lifetimes, companion identity/AI and broader picker/cursor/lock gates remain strong-agent work.

**Previous capture targeting checkpoint (2026-11-10):** [Primary019E capture loop](evidence/PK27-pet-capture-target.md) connects source level0 arm, actor predicates, approach and mode76 to the existing first-click162/019F/01A0 owner. Real socket and Vulkan billboard/picker/WindowProc evidence pass; NPC continuous turn/contact and primary source decisions are retained. Deploy-OFF all-target and standalone builds pass; application164/164 in51.90s (-j4) and standalone473/473 in14.33s (-j8). Twenty-two native/Qt frame pairs and zero QML warnings pass, plus real Vulkan texture/picker/WindowProc capture input. DeployON restored configure-only; OldRO unchanged. PK27/PK09.21 and the whole plan remain open. Shared proceed interruption/reentrancy, companion identity producers/AI and broader picker/cursor/lock gates remain strong-agent work.

**Previous pet taming checkpoint (2026-11-10):** [Factory91/mode76/01A0 owner](evidence/PK27-pet-taming.md) implements source one-attempt click, repeated owner/result semantics, native/passive Qt sprite projection and source frame/timer boundaries. Deploy-OFF all-target and standalone builds pass; focused2/2 in6.40s, application164/164 in53.44s (-j4) and standalone473/473 in16.17s (-j8). Twenty-two native/Qt frame pairs and zero QML warnings include four taming states. DeployON restored configure-only; OldRO unchanged. PK27/PK09.21 and the whole plan remain open. The next strong-agent work is019E arming, primary mouse picking/consumption and approach; server-driven capture is not yet complete.

**Previous capture request checkpoint (2026-11-10):** [Mode162/019F sender and strong-agent continuation](evidence/PK27-pet-capture-request.md) adds the exact6-byte request and socket/failure replay. All-target deploy-OFF build and application164/164 in50.89s pass. Selected-PE review resolves SlotMachine assets and animation boundary/timer traps. 019E targeting/approach, factory91,01A0 result, animation and reset integration remain open. DeployON restored configure-only; OldRO unchanged. PK27/PK09.21 and the whole plan remain open.

**Previous egg-selector checkpoint (2026-11-10):** [Pet egg receiver and copied-row selector](evidence/PK27-pet-egg-selector.md) restores01A6, factory90 and mode159/01A7. Reused lists append copies because source75 is ignored; selection preserves source snapshot/failure effects and keyboard/double-click semantics. Deploy-OFF all-target/standalone builds pass; focused2/2 in5.70s, application164/164 in201.47s and standalone473/473 in73.97s. Eighteen native/Qt frame pairs and zero QML warnings include five egg-selector states, source scrollbar/drag/cancel capture and actual125% OK input. Selected-PE/source and plan checks pass separately. Deploy ON restored configure-only; OldRO unchanged. PK27/PK09.21 and the whole plan remain open.

**Active step: PK09.21.** Continue [remaining chat lifecycle](work-items/PK09.21.md) under still-open PK09.04. Subject: `packet(PK27): restore canonical ground proceed and text owners`. Preserve earlier source/packet/item/corpse proofs; broad requests, PK47 and independent PK48/49/live/deployed gates remain open. The whole packet plan remains unfinished.

**World-entry repair:** [same-world handoff fix](evidence/PK05.05-world-entry.md) remains in the rebuilt candidate and its regression passes. OldRO still has the prior executable and requires redeployment/live retry; no installed acceptance is claimed.

**PetInfo checkpoint (2026-11-10):** [Current evidence](evidence/PK27-pet-window.md) records the implemented functional factory88/controller/native/passive Qt checkpoint. The signed level>=10000 alternate01A2 summary recipient (factory12/message29) is implemented; see [summary evidence](evidence/PK27-pet-summary.md). The mode26 pet context-menu producer and IDs29..33 are implemented; see [context-menu evidence](evidence/PK27-pet-context-menu.md). The 01A6 receiver, source factory90 egg-list window and mode159→01A7 sender are implemented; see [egg selector evidence](evidence/PK27-pet-egg-selector.md). Continue capture019E/01A0, feeding01A3, actor action/dialogue01AA and remaining follow/reset/failure lifetimes. Keep command entries68/87 open for actual DetailLevel and Up recipients. Snapshot validity alone is not presence. PK27 and PK09.21 remain open.


- PK09.14 restores parent detail rectangle/default/height lifecycle, fixed clipped scrolling, visible slot predicates, output reuse and safe child deletion. Separate WID82 card details/hit testing now pass PK09.15.
- PK09.13 restores inventory whole-record fallback, source dimensions/normalization, mini and original height, native/Qt resize and explicit managed close/recreation. Parent detail geometry now passes PK09.14; separate card and map/show lifetime now pass PK09.15/PK09.16.
- PK09.12 shares source-selected startup and manager runtime tab selection, with fresh-process, radio, viewport, native/Qt close/recreation and storage independence coverage. Inventory geometry now passes PK09.13; detail and remembered-show lifecycle pass PK09.14/PK09.15/PK09.16; full item/context acceptance remains open.
- PK37.09 verifies exact C5/C8/C9, strict signed-length/row/quote guards, failed immediate sends, local packetless cancel, ordered CA/CB receipts, native/Qt count prompts and map cleanup. Later NPC contexts cannot lose or be cleared by older replies. Selected market-stock applicability is adjudicated; broad server eligibility and full lifecycle remain open.
- PK37.08 preserves unsigned/zero list quotes, exact64-bit row/aggregate amounts and decimal strings through Qt, fixes overflow-prone deltas and zero sell stock, and fits full numeric labels in native/Qt panes. Broad request/result and transaction eligibility remain open.
- PK37.07 restores NPC catalog/buy/sell descriptions from exact copied rows, fixes final-row scrolling, scrolled-header selection, stale model offsets and Qt header alignment. Passive native input and125% conversion pass. Legacy grid fallback remains a geometry/signature boundary; do not invent a current gesture. Row-price arithmetic now passes PK37.08; broad NPC transactions remain open.
- PK09.11 restores same-key visible selection closure, hidden reopen, explicit forced assignment, owned native close/recreation and saved logical position. Native/Qt selections and force API checks pass; NPC detail fallback gestures remain open.
- PK09.10 shares source All-tab classification across native inventory/storage and visible Qt commerce, fixes Cash18/historical groups and unknown fallback, and verifies raw fields, actual tab delegates and snapshot/hover lifetime. Primary radio predicates have no connected selectable controls.
- PK37.06 resolves commerce detail contexts, remote ordinal identity and copied-offer ownership; merchant/trade/viewed-equipment right-clicks, scale, invalid-callback guards and tooltip layering pass. Broad transaction/placement acceptance stays open.
- PK33.05 verifies selected rental framing/notices/removal, absolute local expiry dates and owner-bound native/Qt detail lifetime. Popup layering, scale and description containment are corrected; broad PK33 stays open.
- PK09.09 restores named/created/forged item composition, message values, normalized-pet and raw-type-seven tab behavior and stationary delayed producer-name refresh in native/Qt UI. Rental expiry now passes PK33.05; other type/context work remains open.
- PK45.05 separates 0194 character-ID names from actor caches and adds selected 00A2 request, retries, reply isolation and map/mode cleanup. Special-item name/UI integration now passes PK09.09.
- PK09.08 restores card-ID grouping, postfix repeat position and slot-zero descriptor ownership; 17 native/Qt cases and 57 actual frames pass. Special named/crafted variants now pass PK09.09; expiry and other contexts remain open.
- PK09.07 repairs list-only socket suffixes leaking into hovers and Qt identification/refinement naming. Actual native/Qt names and quantity delegates/replacement pass. Generic cards now pass PK09.08; special-name variants now pass PK09.09; expiry and other contexts remain required.
- PK37.05 repairs same-index metadata loss, contents/presence conflation and unknown failure notices. Local options own cart removal; counters remain authoritative. Production packet/actor tests and actual QML Cart-button reopening/footer rendering pass. Broader PK37.01–.04 remain open.
- Prior bag/field/colour/map proofs and PK09.02/.03 mapping/route checkpoints remain verified. PK09.04 stays open for remaining type/tab/radio/detail contexts and historical acceptance.
- Deploy-off build and final validation: **118/118 CTest in 154.96s**, **173 PASS packet records**, eight fresh detail child processes, nine detail frames and zero detail-child QML errors. No installed client/live server or numeric row closure.
- NPC/status/mail implementations, PK46.02's177-row inventory/character paths (including broader scope beyond the bounded NPC sends), PK47 and independent PK48/49/deployed provenance remain open. The whole-plan goal remains active.

Preserve [PK09.15 card details](contracts/PK09.CARD-DETAIL.md): separate WID82 snapshot/geometry/toggle, own close and parent coexistence, hidden-band nonzero-prefix hits, strict gaps and child routing, viewport balloons above both panes, popup priority and mode cleanup. Painting and interaction remain separate predicates. Card description sends no combine transaction. Continue remaining full item/status/delta/context acceptance; broader scale/art/locale and PK47/48/49/deployment remain open.

Preserve [PK09.16 inventory show/map reconstruction](contracts/PK09.INVENTORY-SHOW.md): startup show=false despite savedShow1; MakeWindow sets remembered-open; raw hide preserves it; explicit close/button/Alt-E deletes and clears it; bulk map teardown preserves it; basic UI reconstructs valid geometry/tab/mini from the cached record. Do not conflate this with authoritative item-row or transaction retention. Keep earlier inventory/card/NPC proofs. Continue full item/status/delta/context acceptance, broad requests/commerce/NPC/status/mail, PK47 and independent PK48/49/deployment.

Preserve [PK09.17 acknowledgement state/notices](contracts/PK09.INVENTORY-ACKS.md): every active pickup result ends retry intent; additive indices retain metadata; use counts are authoritative and owned by the routed mode; remote use cannot consume local rows; equip result1 alone succeeds and missing rows cannot evict occupants; known notices load their original message IDs. Preserve zero-count drop rejection and pending-index correlation. PK09.18 now accepts the separate item acquisition notification. Follow its continuation instructions for remaining item/context/effect work; full PK09.04 and broad/independent/deployed gates remain open.

PK09.18 completes the prior WID58 item-notification gate. Follow [continuation instructions](work-items/PK09.18.md); preserve its snapshot/sizing/name/color/overflow/lifetime/passive-input boundaries. Money branch and remaining full item/context/effect acceptance stay open. 122/122 CTest in 171.33s; 174 PASS packet records; 9 notification GUI cases and 8 retained frames; zero QML errors.

PK09.19 accepts the bounded source item-use feedback contract. PK09.20 resolves the recovery arithmetic gate; continue remaining full item/context/effect work under its instructions. Money trigger, legacy Yggdrasil latch workflow, audio hardware/animation art and broad/independent/deployed gates remain open.

Preserve [PK09.20 recovery](contracts/PK09.RECOVERY-DELTA.md): signed words, session-owned modulo arithmetic, zero events, recognized statuses, shared RNG and independent maxima. Source-body units and production/native/Qt checks pass. Full item/context/art and broad/independent/deployed acceptance remain open.
