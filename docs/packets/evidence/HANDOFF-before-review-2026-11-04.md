PK14.04/16.04 deferred for stronger replay/profile review; dirty replay-file ownership unresolved. PK16.02 awaits target settings profile. PK18.02 waits cell mapping/dynamic overlay; PK20.02 actor resources; PK22.02–.04 status lifecycle; PK24.03–.04 0284 resource ordering/replay; PK25.03/04 compass lifecycle; PK32.02–.05 full mail workflow; PK50.02–.04 quest model/time semantics. PK44.04 focused replay passed 1/1 with loopback permission, but broader gates, deletion profile mismatch and 0825 security review remain. PK48.01 scoped certification; PK48.02–.04 await evidence. PK46.01 scoped; PK46.02 partial (77 evidence notes including 61 additional request-path notes; all 177 accepted C2S map requests still require complete per-request comparison); PK46.03 crosswalk partial; PK46.04 cannot certify. PK45.01 scoped; PK45.02 has four traced leaves and spawn variants deferred for stronger review. PK45.03 corrected 0x0189 memo semantics and traced NPC number prompt/reply/cancel; numeric signedness, UI replay/color, and remaining handlers are open. PK45.03 remains incomplete; the 0x022C/0x07F7/0x0856 direction boundary and 0x012A/0x0140 owner/profile cases need stronger review. Continue PK46.02 in bounded batches. No rows closed. Preserve tools/tests/replay/PacketStreamReplay.cpp.

PK45.03 now also has the NPC string prompt/reply path traced. Server-side truncation, encoding, and input boundary behavior need stronger review; no rows closed. Continue with the next independent PK45.03 handler. Preserve tools/tests/replay/PacketStreamReplay.cpp.
PK45.03 also has NPC menu receive/selection traced; exact selected-server choice/cancel boundaries need stronger review. Continue with remaining independent PK45.03 handler leaves. Preserve tools/tests/replay/PacketStreamReplay.cpp.
PK45.03 now includes dialogue text/next/close traces. NPC dialog lock release and cleanup timing remain for stronger lifecycle review. Continue with remaining independent PK45.03 handlers.
PK45.03 also has a partial actor state-change trace. 0x0119/0x0229 source/profile reachability plus sit/cart status lifecycle need stronger review; do not close based on registry lengths. Continue remaining registered handlers and preserve tools/tests/replay/PacketStreamReplay.cpp.
PK45.03 also has an item pickup acknowledgment trace. Basic/extended server profile, failure cleanup, and request/ack correlation need stronger review and replay. Continue with remaining registered handlers.
PK45.03 cart metadata/clear handlers are traced. Deep cart item, equipment, and session-inventory synchronization needs a focused review of the minified commerce receiver and replay; continue other handlers independently.
PK45.03 has a complex actor walking decoder note. Verify the heuristic classic/shifted variants against the selected server and captures; include local/remote movement desync cases in PK45.04 replay. Continue independent handlers meanwhile.
PK45.03 actor position correction is traced for 0x0088/0x01FF; confirm high-jump equivalence and replay stop/correction boundaries before closure. Continue remaining handler leaves.
PK45.03 now records 0x0086 movement update and the 16-byte framing vs 12-byte handler minimum discrepancy. Verify packet-stream rejection and movement timing in PK45.04; continue other registered handlers.
PK45.03 actor resurrection 0x0148 is traced. Confirm trailing bytes, absent-actor behavior, and ordering against server/replay; continue remaining handlers independently.
PK45.03 now traces the ground-item 0x009D/0x009E entry and 0x00A1 disappearance through logical state, world-item sync, retry cleanup, and dropped animation. Verify selected-server variant use and visual/resource behavior in replay; continue remaining handlers.
PK45.03 also traces 0x00AF/0x07FA inventory removal, 0x00AA/0x00AC/0x013C equipment acknowledgments, and 0x013B action failures. Confirm server result/type mappings, pending-index correlation, and action retry semantics before closure; continue remaining registered handlers.
PK45.03 now also records actor font/look updates. In particular, robe type 12 marks the billboard dirty without storing its value, while font assignment does not explicitly invalidate it; both need source/render review. Continue remaining handlers and preserve the replay source.
PK45.03 skill-failure 0x0110 state/transport path is traced; full message matrix remains for focused review. Continue the next registered handler.
PK45.03 now traces emotion command 0x00BF through 0x00C0 and the local effect/pending-flag path; verify server handling, timeout behavior, and asset parity. Continue remaining registered handlers.
PK45.03 now records player skill list 0x010F and update 0x010E. Shared 37-byte list parsing rejects partial tails before clearing; verify selected-server framing guarantees, list/update ordering, duplicate/out-of-range IDs, and replay. Continue remaining handlers.
PK45.03 also covers homunculus/mercenary skill lists and updates. Confirm target ID ranges and shared upgrade-flag correlation under out-of-order/cross-target updates; continue remaining handlers.
PK45.03 now includes homunculus/mercenary skill list/update traces and the 0x02B9 shortcut snapshot. Verify target ID domains, shared upgrade-flag correlation, slot ordering/types, and UI refresh through replay; continue other handler families.
PK45.03 now has normal and equipment inventory list traces. Verify each packet's selected-server variant and malformed/partial boundaries, especially ammo preservation and equipment merge/appearance rebuild; continue storage and other handlers.
PK45.03 now covers normal/equipment inventory snapshots and storage lifecycle. Current client source confirms 0x00F2/OpenStorage clears storage rows before list packets; list packets themselves are incremental. Confirm selected-server status-before-list ordering for initial open/split lists and verify add/remove pairing and shared item state before closure. Continue remaining handlers.
PK45.03 storage password framing is fixed from selected server registry sizes (0x023A=4, 0x023C=6). Full password prompt/send/result flow remains unimplemented and needs a stronger server/UI protocol review.
PK45.03 now traces whisper and normal/player/party chat receive paths. Whisper ack correlation uses only one latest pending item; the 0x008D offset-4 fallback is trace-based and needs selected-profile captures. Continue remaining handler families. The storage-password framing fix builds; the workflow remains a placeholder.
PK45.03 now includes broadcast 0x009A/0x01C3 and battlefield 0x02DC chat paths. Legacy broadcast prefixes/colors and battlefield offsets remain for source/capture review. Continue remaining handlers.

PK45.03 current checkpoint (2026-11-04): 113 evidence files are indexed in state.json, and every explicit router.Register(opcode, Handle...) now has an opcode mention in a PK45.03 leaf. This is an inventory/evidence checkpoint only; PK45.03 remains IN_PROGRESS. Complete the 0x013E cast-ACK branch matrix and stronger review for 0x0152 emblem validation/reconciliation; confirm shared guild/effect/status branches against selected-server contracts and run production replay/UI lifecycle checks before closing rows. PK45.04 is IN_PROGRESS; production replay/UI checks remain blocked pending a working Windows SDK/build environment. The packet-size table/server reference is absent for several handlers (including 0x0145, 0x01CD and 0x0215); resolve framing/reachability before semantic closure.


Commerce continuation checkpoint (2026-11-04): added cart-list, cart-delta, vending/shop lifecycle, trade lifecycle and viewed-equipment evidence. Every explicit case label in src/gamemode/Commerce.cpp now has a matching opcode mention in PK45.03 evidence. This is a source-coverage check only; server schemas/status meanings and representative replay remain open, and no rows were closed.

PK45.04 commerce self-review (2026-11-04): commerce receive-label coverage and noted client fields/transitions are source-reviewed; replay remains unrun. RO_BUILD_PACKET_REPLAY_TESTS is restored to OFF after CMake failed with MSBuild Windows SDK registry access denied while building zlib. See docs/packets/evidence/PK45.04-commerce-self-review-replay-blocker.md; resolve SDK access and obtain server/profile fixtures before row closure.


PK46 continuation checkpoint (2026-11-04): PK46.02 has bounded evidence for movement, direction, continuous attack and ground-item take; the 177-request inventory remains open. PK46.03 has an expanded character-delete UI trace confirming a reachable 0x0068/46-byte request conflicts with selected 20080910's 0x01FB/56-byte contract. See the PK46.02/03 evidence leaves and continue their remaining request/reachability reviews; no discovery rows were closed.

PK46.04 bounded self-review (2026-11-04): reviewed current PK46.02/03 leaf claims against source and selected registration evidence; no rows closed. Full package review waits on the 177-request map inventory and remaining login/character branches. Production replay/UI limitation is documented at docs/packets/evidence/PK46.04-bounded-self-review.md and PK45.04 blocker evidence.

PK45.02 selected actor movement follow-up (2026-11-04): traced hash-verified rAthena 20080910 recognized-class spawn/idle forms 0x02ED/0x02EE and walking 0x02EC against current receiver offsets. The client/server weapon/shield interpretation remains fixture-dependent; invalid-class 0x0078/0x007C fallback, effect ordering, actor resource cleanup, and production replay remain open. See docs/packets/evidence/PK45.02-server-spawn-20080910-font-pair.md and PK45.02-server-walking-20080910.md.

PK46.02 map request follow-up (2026-11-04): added inventory drop 0x0116 end-to-end trace from drag/prompt UI through the 10-byte client packet to selected server clif_parse_DropItem fields, guards and failure notice. Five bounded map-request leaves now exist; the 177-request inventory is still open and no rows were closed. See docs/packets/evidence/PK46.02-map-drop-item-request.md.

PK46.02 map inventory follow-up (2026-11-04): added 0x0116 inventory drop and normal use-item paths. The PacketVer22/23 use layouts match selected parser identities; explicit legacy use opcode 0x00A2/8 bytes conflicts with selected 0x00A2 name-request/15-byte registration. Six bounded request leaves are now indexed; the 177-request inventory and replay remain open. See docs/packets/evidence/PK46.02-map-use-item-request.md.

PK46.02 equip follow-up (2026-11-04): added the common 0x00A9 equip sender/parser trace (6-byte struct, selected server clif_parse_EquipItem). Ammo/location handling and acknowledgement/UI lifecycle remain open. Seven bounded map-request leaves are indexed; full 177-row coverage and replay remain open.

PK46.02 continuation checkpoint (2026-11-04): added unequip, skill-up, sit/stand, return-to-save-point, and return-to-character-select traces. PK46.02 now indexes 12 bounded request leaves against the 177-request inventory; replay and full inventory parity remain open, and no rows were closed. See the corresponding PK46.02-map-*.md evidence files.

PK46.02 continuation checkpoint (2026-11-04): added identify-item 0x0178 and compose-card 0x017C selection/cancel traces. PK46.02 now indexes 14 bounded request leaves against the 177-request inventory; insertion/result lifecycle, replay, and full inventory parity remain open, and no rows were closed. See the corresponding PK46.02-map-*.md evidence files.

PK45.03 escalation note (2026-11-04): retain the automatic-spell-list 0x01CD contract as open until authoritative framing, profile reachability, server semantics, and replacement lifecycle are established; retain storage-password prompt/send/result as open pending a complete server/UI protocol trace. These are bounded stronger-review items, not evidence-backed closure claims. PK45.03 remains active; PK45.04 replay is independently blocked by the recorded CMake/Windows SDK environment failure.

PK45.03 guild-alliance follow-up (2026-11-04): prompt presentation and response dispatch are now traced through QML/Commerce to selected clif_parse_GuildReplyAlliance (0x0172/10). Server acceptance semantics, expiry/replacement policy, and replay remain open; no rows closed. See docs/packets/evidence/PK45.03-guild-alliance-invitation.md.

PK45.03 friend-request follow-up (2026-11-04): production prompt, accept/decline/cancel, and 0x0208/14-byte response fields now reconcile with selected clif_parse_FriendsListReply. Server semantics, prompt expiry, and replay remain open; no rows closed. See docs/packets/evidence/PK45.03-friend-handlers.md.

PK45.03 party invitation follow-up (2026-11-04): incoming request now traces to UIJoinPartyAcceptWnd and 10-byte 0x00FF reply; outgoing 6-byte 0x00FC invite matches selected clif_parse_PartyInvite registration. Server state/permission behavior, stale/duplicate prompts, and replay remain open; no rows closed. See docs/packets/evidence/PK45.03-party-create-invite-list.md.

PK45.03 guild-invitation follow-up (2026-11-04): selected-server registry confirms 0x016B/10 to clif_parse_GuildReplyInvite, matching the client's response packet size and fields. Decision polarity and server result semantics remain open; no rows closed.

PK45.03 party-create ordering follow-up (2026-11-04): client sends name-only 0x00F9/26, then after success ack optionally sends 0x0102/6 for nonzero options. The ack path seeds local desired options before the unchecked follow-up; verify server defaults and send-failure behavior by selected-source review/replay. No rows closed; see party-create-invite-list evidence.

PK45.04 continuation (2026-11-04): added a bounded invite-family self-review covering 0x016B, 0x0172, 0x0208, 0x00FF, and 0x00FC against client senders and the selected registry. Server handler semantics and production replay remain open; no rows closed. See docs/packets/evidence/PK45.04-invitation-family-self-review.md.

PK45.04 party-list boundary follow-up (2026-11-04): added source review documenting clear-before-parse and ignored 1–45-byte entry remainder after floor division; malformed behavior remains unvalidated pending selected-source contract and replay. No rows closed. See docs/packets/evidence/PK45.04-party-list-boundary-self-review.md.

PK45.03 sound-loop follow-up (2026-11-04): traced packet name through PlayWave and PlayWaveRelative to CWave resource lookup/backend. BGM normalizer is not used in this SFX call path; internal resource-key normalization/path constraints and server semantics remain open. No rows closed; see PK45.03-sound-loop.md.

PK46.02 warp-skill request follow-up (2026-11-04): added list selection and cancel trace for 0x011B/20 through UISkillSelectWnd and selected clif_parse_UseSkillMap. The "cancel" sentinel server behavior remains unverified; no rows closed. See PK46.02-map-warp-skill-selection-request.md.

PK45.03 skill-failure follow-up (2026-11-04): superseded the prior clipped-source limitation with a bounded cause/btype/message-ID audit for ResolveSkillFailNotices. Server cause meanings, locale/color behavior, and replay remain open; no rows closed. See PK45.03-skill-failure-ack.md.

PK45.03 action-failure follow-up (2026-11-04): completed client type/job switch and repeated-failure state effects. Unknown types are ignored; types 0–2 clear retry state, type 3 reports by job and leaves it intact. Server code mapping/replay remain open; no rows closed. See PK45.03-action-failure.md.

PK45.03 actor-guild-change follow-up (2026-11-04): documented localGuild replacement/update gate, actor job eligibility, emblem validation, and job-722 guild removal path. Identifier domain and visual/server semantics remain open; no rows closed. See PK45.03-actor-guild-change.md.

PK45.03 actor guild-change selected-source follow-up (2026-11-04): hash-pinned 20080910 server defines/emits 0x01B4 as 12-byte AID u32@2, guild ID i32@6, emblem ID u16@10, area-scoped. Updated earlier GID wording. Client visual/cache refresh, trigger call-site lifecycle, and replay remain open; no rows closed.

PK45.03 action-failure selected-source follow-up (2026-11-04): hash-pinned 20080910 rAthena confirms packed 0x013B/4-byte type field and self-send; server types 0–3 align with client messages and job branches. Call-site reachability, repeat timing, and replay remain open; no rows closed.

PK45.03 actor-resurrection selected-source follow-up (2026-11-04): server 0x0148 layout/type semantics now confirmed from hash-pinned source; emitter sends AREA and then respawns disguised actors. Cross-packet ordering and runtime lifecycle remain open; no rows closed. See PK45.03-actor-resurrection.md.

PK45.03 storage emitter follow-up (2026-11-04): pinned source shows clif_storagelist emits no list for zero entries; refresh follows with 0x00F2 amount/count, and 0x00F8 close is separate. Client retains rows until close, so investigate storage.cpp caller ordering before asserting a defect. Transfer correlation/replay remain open; no rows closed.

PK45.03 skill-failure escalation (2026-11-04): hash-pinned server 0x0110 packet is skillId u16, btype i16, itemId u16, flag, cause; client currently reads btype as u32 spanning itemId. Audit itemId call-site usage/profile intent before a code change, then replay nonzero itemId cases. No rows closed.

PK45.03 actor-direction selected-source follow-up (2026-11-04): confirmed 0x009C field domains and captured the disguised-actor second SELF send with rewritten ID/head=0. Seated-animation parity and runtime validation remain open; no rows closed. See PK45.03-actor-direction.md.

PK45.03 skill-failure itemId/cause follow-up (2026-11-04): current external clif.hpp defines causes 71/72 for NEED_ITEM/NEED_EQUIPMENT; four explicit skill.cpp itemId call sites use them, and client resolver drops unknown causes. clif.hpp/skill.cpp are not in PK01.02's pinned hash list; verify provenance and replay before code changes. No rows closed.

PK45.03 party-member-added selected-source follow-up (2026-11-04): effective server profile uses 0x01E9/81 with matching AID, role, state, strings; sharePickup/shareLoot occupy trailing bytes 79/80 but client ignores them. Alternate 0x0104 and UI/replay remain open; no rows closed. See PK45.03-party-member-added.md.

PK45.03 actor-font selected-source follow-up (2026-11-04): clif_font is enabled for PACKETVER 20080910 and sends 0x02EF/8 with sd->id AID and font u16 to AREA, matching client layout. Font asset/render/refresh parity remains open; no rows closed. See PK45.03-actor-appearance-font.md.

PK45.03 guild-alliance server follow-up (2026-11-04): pinned emitter/parser confirm inviter AID correlation and flag 1=accept; client/server ack labels match. Current unpinned guild.cpp seems to send ack 3 for a decline (documented as alliance-limit failure), so verify exact revision/recipient semantics before classifying as defect. guild.cpp hash recorded in evidence; no rows closed.

PK45.03 friend-reply server follow-up (2026-11-04): pinned 0x0208 active 14-byte parser confirms requester AID and 32-bit decision; char ID is ignored. Acceptance requires reciprocal pending requests; result routing and optional auto-add behavior are documented. Friend request-state business logic is unpinned and list/replay gaps remain. No rows closed.

PK45.03 guild notification result follow-up (2026-11-04): pinned server docs confirm 0x0173 alliance flags 0–5, 0x0181 opposition flags 0–3, and 0x015E disband reasons 0–2; each ack sends SELF and client labels align. Verify 0x015F emitter/callsite, cleanup completeness, and replay before closing. No rows closed.

PK45.03 party create/reply follow-up (2026-11-04): pinned parser confirms CreateParty2 name/options fields and 0x00FF reply flag meanings. Map-lock/basic-skill checks precede creation. Business logic, call ordering, invite correlation, list tails, and replay still need review. No rows closed.

PK45.03 party-list emitter follow-up (2026-11-04): pinned source confirms 0x00FB header and 46-byte selected-profile member record; later GID/class/level fields are inactive in 20080910. SELF/PARTY target depends on call form. Client partial-tail and HP-cache behavior still need fixture/replay validation. No rows closed.

PK45.03 guild relation follow-up (2026-11-04): pinned source confirms 0x0184 relation mapping and SELF send; 0x0185 add layout is only backed by a commented emitter, not active runtime code. Verify business callsites/authorization/UI and replay. No rows closed.

PK45.03 0x014C guild relation list follow-up (2026-11-04): pinned emitter confirms the 32-byte row layout and SELF recipient, skips nonpositive IDs, and sends nothing without a guild. Replay and unusual relation values remain open. No rows closed.

PK45.03 guild summary follow-up (2026-11-04): pinned source selects 0x01B6/114 for 20080910, documents old 0x0150 as legacy, and confirms emitter field values plus SELF/no-guild behavior. Older variant and runtime/text validation remain open. No rows closed.

PK45.03 guild member roster follow-up (2026-11-04): pinned emitter validates 0x0154/104 row schema and online state; appearance/intro fields are not consumed by the client. No-guild path sends nothing. Check summary online-count synchronization and replay; no rows closed.

PK45.03 0x0176/0x0182 member-record escalation: client has fixed 106-byte upsert, but pinned server material located here has only bare size registrations and no matching emitter/struct contract. Do not infer the 0x0154 roster schema; request stronger protocol-history/revision review before implementation. No rows closed.

PK45.03 guild online 0x016D follow-up (2026-11-04): pinned helpers confirm 14-byte AID/GID/online record and login batching/exclusion behavior; notice helper targets guild members. First-time UI display caveat/replay remain open. No rows closed.

PK45.03 guild menu flags follow-up (2026-11-04): pinned emitter sends master/member masks 0xD7/0x57; known bits map to tabs, 0x40 remains unknown and unused in Qt. Notice requires master plus 0x80. Validate live refresh; no rows closed.

PK45.03 guild position names follow-up (2026-11-04): pinned emitter sends all 20 ID/name rows in a 564-byte 0x0166 frame for guild sessions; mode/tax arrives separately. Replay/UI refresh remains open. No rows closed.

PK45.03 guild position mode/tax follow-up (2026-11-04): pinned 0x0160 list and 0x0174 update layouts/targets documented; 0174 currently sends one row despite FIXME. Server maps payRate from exp_mode; client calls it tax, but exact units are in unpinned guild business logic and need stronger review. Ranking TODO. No rows closed.

PK45.03 guild member position follow-up (2026-11-04): pinned emitter confirms 12-byte AID/GID/position row and GUILD target. It sends one row per call despite a FIXME about post-commit batching. Verify call sequence/replay; no rows closed.

PK45.03 guild departure follow-up (2026-11-04): pinned server confirms 2008 name-based 0x015A/66 and 0x015C/90 layouts; expulsion account name is empty pre-20100803. Both use GUILD_NOBG. Confirm recipient scope and business ordering/replay; no rows closed.

PK45.03 guild ban list follow-up (2026-11-04): pinned emitter validates 0x0163/88 layout and blank account-name field in 2008 profile; skips zero-AID entries and sends SELF. Presentation/empty-list/replay remain open. No rows closed.

PK45.03 guild notice follow-up (2026-11-04): pinned emitter confirms 182-byte fixed subject/notice packet to SELF, omitted when both fields empty. Verify separate client reset path and encoding/UI/replay. No rows closed.

PK45.03 guild create ack follow-up (2026-11-04): pinned 3-byte SELF emitter; current unpinned guild.cpp explains result flags 0–3 and asynchronous success/failure correlation. Blank-name branch gives no ack; verify intended revision and UI/replay. No rows closed.

PK45.03 guild skill-list follow-up (2026-11-04): pinned emitter confirms 6-byte header/37-byte rows, required-skill filtering and leader-only upgrade flag. Empty qualified list retains header/points; no-guild path sends nothing. Validate client clear/rebuild and malformed-tail/large-length behavior. No rows closed.

PK45.03 guild status 0x01F2 follow-up (2026-11-04): selected PACKETVER server branch emits 20-byte status+appearance records; separate 0x016D SELF snapshot is 14 bytes. Server comment lacks historical cutoff; caller/replay/recipient semantics need review. No rows closed.

PK45.03 guild emblem follow-up (2026-11-04): pinned emitter confirms 0x0152 header/length and direct recipient, omitting empty emblems. Client decompression/BMP bounds and malformed-input safety need stronger review; runtime UI/cache behavior remains open. No rows closed.

PK45.03 commerce small-results escalation: pinned-source result emitter/semantics not located for 0x0249 or 0x00D1 (only bare 0249 size registration). Verify correct service/profile or use capture before classifying client polarity. No rows closed.

PK45.03 guild invitation follow-up (2026-11-04): pinned source confirms 016A prompt fields, 016B response decision polarity, and 0169 result labels/SELF scope. Business validation/current-invite semantics are unpinned; expiry/replay remain open. No rows closed.

PK45.03 trade lifecycle follow-up (2026-11-04): pinned server docs confirm invitation result meanings, lock actor polarity, zeny sentinel and cancellation/success opcodes; client mappings align. 00F0 failure emitter, offer semantics, invite correlation/expiry and replay remain open. No rows closed.

PK45.03 vending follow-up (2026-11-04): pinned emitters confirm 0133/0136 row schemas, 0135 result values, 0137 seller index/amount delta and 0131/0132 board events. Selected PACKETVER 20080910 lacks newer open-store ack. Review omitted result 3, transaction ordering and replay. No rows closed.

PK45.03 commerce cart delta follow-up (2026-11-04): pinned server selects 01C5/22 for 20080910; 0124 is older. Add payload, 0125 removal delta and 012C failure polarity match client. Transaction ordering/replay remains open. No rows closed.

PK45.03 cart item-list follow-up (2026-11-04): pinned profile selects 02E9 normal and 02D2 equipment rows; exact schemas and no-empty-category emission documented. Client category-snapshot replacement preserves the other category. Validate server stackability vs client item-type categories, delete-time semantics and replay. No rows closed.

PK45.03 viewed-equipment follow-up (2026-11-04): pinned handler gates same-map and target visibility/requester permission; response is 43-byte header plus 26-byte equipped rows. Current unpinned defines_pre hash corroborates SAK 20080910 selection. Profile provenance/privacy replay remain open. No rows closed.

PK45.03 broadcast follow-up (2026-11-04): pinned clif emitters confirm 009A blue/ssss prefixes and 01C3 16-byte font header. Client micc/tool parsing and rendered color byte order still need stronger evidence/replay. No rows closed.

PK45.03 whisper follow-up (2026-11-04): pinned server validates fixed 24-byte NUL-terminated target and NUL-terminated message within CHAT_SIZE_MAX-1; 0098 status codes match client and have no request ID. Concurrent whisper ack correlation is impossible from packet alone. Verify exact cap/inter-map behavior/replay; no rows closed.

PK45.03 recovery follow-up (2026-11-04): pinned emitter confirms 013D types 5 HP/7 SP, u16 amount cap, SELF. Unpinned pc.cpp routes SP_AP through clif_heal; determine whether 20080910 can receive AP before classifying client handling. pc.cpp hash recorded. Replay remains open; no rows closed.

PK45.03 ammunition ack follow-up (2026-11-04): pinned emitter confirms 013C carries client index from EQI_AMMO, not item ID/wear mask. Determine no-ammo sentinel via client_index and replay call timing; no rows closed.

PK45.03 ammunition sentinel follow-up (2026-11-04): empty equip_index -1 converts via client_index(+2,u16) to client index 1; valid items start at 2, so client treats it as no ammo and clears prior mark. equip_index declaration/init are unpinned; verify intended revision and replay. No rows closed.

PK45.03 attack-range follow-up (2026-11-04): pinned server sends sd.battle_status.rhw.range via 013A on SP_ATTACKRANGE; emitter does not normalize. Client clamps to >=1. Validate zero possibility/profile and runtime attack/chase behavior. No rows closed.

PK45.03 0139 distance-hint follow-up (2026-11-04): pinned emitter matches target/source coordinates and base range; current unpinned unit.cpp explains trigger and temporary chase +1 condition. Hash recorded; confirm intended revision, then replay downstream chase and edge coordinates. No rows closed.

PK45.03 status point cost follow-up (2026-11-04): pinned 00BE emitter sends SELF status ID/value; stat update callsite sends next-stat point cost for IDs 32–37. Formula/enum are from unpinned files; validate provenance and replay boundaries/UI. No rows closed.

PK45.03 self-stat 0141 follow-up (2026-11-04): pinned emitter confirms u32 type/base/plus and SELF; attribute cases send base and derived bonus separately. Verify numeric enum from intended revision plus client cap/selection/UI and replay. No rows closed.

PK45.03 self-status parameter follow-up (2026-11-04): pinned 00B0/00B1 structs confirm signed int32 values and SELF. Client reads u32; audit negative SP_MANNER behavior using intended server enum/range before defect classification. Level-up effects and broader status mapping/replay remain open. No rows closed.

PK45.03 status summaries follow-up (2026-11-04): pinned 00BD emitter/struct confirms 44-byte initial SELF summary and follow-on attribute/range/ASPD updates. Selected tree has no 0214 summary struct/emitter; identify profile before asserting parity. Cost formula, level-up and replay remain open. No rows closed.

PK45.03 time-sync follow-up (2026-11-04): pinned 007F emitter sends direct u32 timer tick to SELF; 007E/0360 request parser stores client tick and replies with gettick. Confirm timer unit/cadence and validate client wrap/late-sample estimator via replay. No rows closed.

PK45.03 shortcut list follow-up (2026-11-04): pinned server profile emits 02B9 with 27 hotkey records matching client length and field order. Current unpinned mmo.hpp enables HOTKEY_SAVING and MAX_HOTKEYS=27. Validate intended revision plus UI/persistence/replay; no rows closed.

PK45.03 player skill-list follow-up (2026-11-04): pinned server confirms 010F/37 rows, skill metadata/upFlag derivation, 8192-byte cap; WE_CALLPARTNER is separately emitted as 0111. Validate cap behavior and class/relearned skill fixtures/replay. No rows closed.
PK45.03 auto-spell-list follow-up (2026-11-04): selected server branch emits fixed 01CD with seven u32 slots to SELF; learned/level-qualified skills are IDs and unavailable choices are zero. Validate sparse-list UI behavior and exact client/server profile match with replay; no rows closed.
PK45.03 companion skill-list follow-up (2026-11-04): server emitters match 37-byte skill rows for homunculus 0235 and mercenary 029D; homunculus 0239 update fields and SELF routing traced. Confirm client/server revision, row order, and outstanding-upgrade request correlation with replay; no rows closed.
PK45.03 auto-run-skill follow-up (2026-11-04): server defines 0147 as fixed 39-byte ZC_AUTORUN_SKILL and emits it SELF as an auto-cast notification. Client handler sends a skill-use request for Merchant Identify. Do not equate these directions until exact client/server profile and callsite/replay are reviewed; stronger review needed, no rows closed.
PK45.03 skill-message follow-up (2026-11-04): selected server emits packed six-byte 0215 (i32 message ID) to SELF; docs codes 15–20 Gospel effects, 28 Full Strip coating failure. Confirm client dispatch/reachability and localized mapping against selected profile/captures; no rows closed.
PK45.03 blade-stop follow-up (2026-11-04): server confirms fixed 14-byte 01D1 source/target block IDs and u32 active flag, broadcast AREA. Replay actor lookup and inactive cleanup; animation parameters need stronger review. No rows closed.
PK45.03 cart metadata/clear follow-up (2026-11-04): server 0121 carries u16 counts/u32 weights to SELF; 012B is a two-byte cart-off packet that clears storage contents while retaining count/weight. Replay ordering and inventory/equipment synchronization; no rows closed.
PK45.03 actor-position correction follow-up (2026-11-04): server clif_fixpos emits 0088 actor ID/x/y in AREA; clif_blown orders slide then fixpos; disguised actor gets SELF disguised-ID copy. Confirm 01FF and replay ordering; no rows closed.
PK45.03 ground-item-entry follow-up (2026-11-04): server uses 009D/SELF for existing floor items during area entry and 009E/AREA for drops; field order differs as client parser expects. Replay ordering and visuals/resource mapping remain open; no rows closed.
PK45.03 ground-item-disappear follow-up (2026-11-04): server sends 00A1 with item object ID only; AREA by default or SELF to a provided target. Cause is not encoded; verify logical removal/frame timing and duplicate IDs; no rows closed.
PK45.03 normal inventory follow-up (2026-11-04): selected 2008 server chooses 02E8 for stackables and 02D0 separate equip list; sends variable groups SELF and arrow-equip separately when applicable. Validate field parity/order/empty behavior with replay; no rows closed.
PK45.03 party options/removal/position follow-up (2026-11-04): pre-2009 server emits 0101 option u32, 0105 AID/name/result, 0107 AID/x/y; position sent same-map party and (-1,-1) removes dot. Confirm 0101 value-2 and removal result meanings/replay; no rows closed.
PK45.03 equipment inventory follow-up (2026-11-04): server 2008 profile uses 02D0 variable SELF rows with item/type/index, legacy identify/damage, location/wear state, refine/cards/expiry/bind metadata. Sprite number is later-version only; verify client interpretation and duplicate/order behavior; no rows closed.
PK45.03 ban notification follow-up (2026-11-04): server sends 0081 with one-byte error code then EOF; packet framing and disconnect match. Client drops reason diagnostics; decide expected display/logging and review cleanup/replay. No rows closed.
PK45.03 return-to-character-select follow-up (2026-11-04): server 00B3 three-byte ACK documents type 1 as disconnect/char-select, sent SELF. Check callers, pending flag lifecycle and teardown ordering by replay; no rows closed.
PK45.03 skill-unit disappearance follow-up (2026-11-04): server 0120 carries unit.id, sent SELF or AREA depending removal path; targeted Ice Wall clear additionally sends cell update. Replay object removal, duplicates and cell sync; no rows closed.
PK45.03 talkbox contents follow-up (2026-11-04): server emits 0191 block ID + 80-byte message (86 total) via bounded copy to AREA. Validate encoding/termination and missing-unit/empty/multibyte presentation with fixtures/replay; no rows closed.
PK45.03 show-image2 follow-up (2026-11-04): server 01B3 emits 64-byte name + type to SELF using bounded copy; types 0–4 placement, 255 clear-all. Verify asset names/encoding and type-255 rendering with replay; no rows closed.
PK45.03 skill-cast-cancel follow-up (2026-11-04): server emits 01B9 six-byte actor GID as AREA ZC_DISPEL. Confirm triggering causes, actor-missing behavior, and client message/effect ordering by stronger review/replay; no rows closed.
PK45.03 017F follow-up (2026-11-04): server contract is variable guild chat, not party channel; bounded NUL-terminated text, empty ignored, GUILD_NOBG. Client calls HandleGuildNotification; verify channel/sender presentation and empty/terminator parsing with replay. No rows closed.
PK45.03 NPC shop follow-up (2026-11-04): server C6/C7 variable SELF lists use shop stock and eligible inventory rows; CA/CB one-byte result packets. Client buy/sell lists now reject partial 11-/10-byte row tails before mutation; empty lists remain valid. Release build passed. Verify request tails, inventory races, and suppression/UI lifecycle through replay; no rows closed.
PK45.03 NPC show-effect 028A follow-up (2026-11-04): server fields are GID, option state, computed level, opt3; disguised actor gets AREA_WOS and two SELF updates. Validate hide masks, level-99 message, effect-375 lifecycle with stronger review/replay; no rows closed.
PK45.03 timed-status 043F follow-up (2026-11-04): no 043F definition/emitter found in selected server packet/clif sources; do not infer duration/tail from legacy status packet. Stronger review should establish client/server profile pairing or capture evidence. No rows closed.
PK45.03 spirits 01D0 follow-up (2026-11-04): selected server defines actor ID + u16 count for servant/abyss balls; client maps it to job-specific visual effects. Confirm semantic/profile pairing and caller scopes with stronger review/replay; no rows closed.
PK45.03 effect notification 0284 follow-up (2026-11-04): server emits actor ID/effect ID/effect-dependent u32 value; requested route plus disguised SELF copy. Locate matching callsites and verify client 331/332 state mutations/dispatcher reachability by stronger review/replay; no rows closed.
PK45.03 spirits2 01E1 follow-up (2026-11-04): struct exists, but selected server spirit-count emitter sends 01D0 and no SPIRITS2 emitter use was found. Locate matching revision/capture before assigning 01E1 meaning; stronger review needed, no rows closed.
PK45.03 login refusal follow-up (2026-11-04): server defines 006A as opcode + error byte + 20-byte unblock time (23 total); sender currently sends empty time. Confirm client error mapping and surrounding identifier/session cleanup with replay; no rows closed.
PK45.03 login acceptance follow-up (2026-11-04): selected server 0069 has sex@46 and server rows@47, while client handler trace reads sex@38/min 39. Treat as high-priority profile/layout mismatch; inspect exact binary and capture. No rows closed.
PK45.03 ignored 01B0 follow-up (2026-11-04): server uses it for NPC class/sprite changes, 11-byte actor/type/class record; player classes skipped. Client ignores it; validate visual parity/intent with replay. No rows closed.
PK45.03 ignored packet group follow-up (2026-11-04): identified 0100 client request, 0199 map-property update, 02C9 party-invite state sent SELF. Remaining opcodes need per-entry definition/callsite/capture audit; stronger-review batch, no rows closed.
PK45.03 ignored packet group (2026-11-04): added source identities for 01D6, 02B1/02B2, 02D3, 02D9/02DA; 0814/0816 are 2010+ gated and outside selected 2008 profile. 01F8/0283/02D5/02E7 still need authoritative review/captures; no rows closed.
PK45.03 ignored group: 0283 is account-ID handoff notification before character-server auth request. No selected map-source match for 01F8/02D5/02E7; group remains partial, resolve from matching source revision/captures; no rows closed.
PK45.03 ignored group correction: selected clif_packetdb contains 01F8/2, 02D5/2, 02E7/variable, 0283/6 registrations; these establish parser framing only. 0283 also has S2C use, so verify directional reuse and resolve semantics via dispatch/captures; no rows closed.
PK45.03 accept-enter variants (2026-11-04): 0073 is legacy pre-2008 branch; selected server 20080910 emits 02EB/13 bytes including font. Client both handlers share time/position and world-mode switch. Capture/load lifecycle still needed; no rows closed.
PK45.03 019B notification-effect leaf (2026-11-04): server AREA actor/effect-code packet; client handles level-up variants only, other server-documented results fall through here. Verify alternate UI paths, active profile and captures; no rows closed.
PK45.03 01F1 login notification-error leaf (2026-11-04): client resets login handshake, shows text dialog, disconnects; packet-size entry is variable but no selected rAthena emitter found and inventory stage says map. Resolve stage/profile and capture; no rows closed.
PK45.03 actor walking alternate IDs (2026-11-04): 022C/07F7/0856 share client skeleton decoder; server has parser sizes only (022C 64/65, others variable), no S2C emitter found. Reconcile direction/profile with captures before supporting these variants; no rows closed.
PK45.03 0B0D note (2026-11-04): server remove-effect packet is version-gated >=2018-10-02; selected 2008 does not emit it, client has no handler. Keep as later-profile context; no rows closed.
PK45.03 scope gap: 0094/00F7/012A/0140/0234 are inventory-labeled C2S parser registrations but owned by PK45 under an S2C handler task. Stronger review should confirm client send paths and move ownership if justified; rows remain UNREVIEWED.
PK45.03 storage-close follow-up (2026-11-04): 00F8 is header-only server ZC_CLOSE_STORE to SELF; client closes storage session/UI. Confirm empty/reopen/NPC transition ordering with replay; no rows closed.
PK45.03 ground-skill 0117 (2026-11-04): server/client 18-byte fields align; normal AREA and disguised split routing; client actor/world message path and special local IDs. Verify missing actors, skill visuals and routing with replay; no rows closed.
PK45.03 skill-result notifications (2026-11-04): 0114/01DE server damage forms, 0115 client path has only commented server implementation, 011A server no-damage result to AREA; client handlers mapped. Confirm compile branch, actors/disguise/action timing with replay; no rows closed.
PK45.03 handler index follow-up: added names for storage list/item/close, homunculus/mercenary skill list/update, NPC shop results, party invite ACK variants, player skill update, skill-unit set variants, and storage-password request/result. Remaining handler-name gaps need separate leaf/source checks.
PK45.03 direct effect 01F3 (2026-11-04): server sends actor/effect IDs by caller route plus disguised SELF copy; client directly launches resolved effect. Validate profile/resource/actor behavior by replay; no rows closed.
PK45.03 use-item ACKs (2026-11-04): client 00A8/01C8 handlers reconcile remaining amount; 01C8 adds item/actor potion effect. Server sends failures SELF/success AREA. Confirm active compile branch and inventory/actor/potion lifecycle with replay; no rows closed.

PK45.03 map-change receive: inspect docs/packets/evidence/PK45.03-map-change-receive.md. Client handlers are traced; retrieve the pinned server snapshot to verify 20080910 emitters, routes, transfer behavior, and profile. No discovery rows closed.

PK45.03 handler index: see docs/packets/evidence/PK45.03-map-handler-index-crosswalk.md; six apparent missing names are cross-referenced to existing packet evidence, not newly audited. Continue remaining PK45.03 per-handler/variant criteria.

PK45.03 ownership-scope update: six PK45-owned C2S rows are recorded in PK45.03-c2s-owner-scope-review.md, including 0x00A7 WalkToXY (8-byte client registration). No ownership changes or row closures made. The 52/52 opcode reference count only confirms mention, not semantic completeness.

PK45 handler count caveat: PK45.03-map-handler-index-crosswalk.md records baseline 136 vs inventory 135 vs current router lexical 140 unique names. Reconcile baseline index extraction/profile/guards before claiming the 136-function gate; name presence is not semantic coverage.

PK45.03 map-change source trace is now in PK45.03-map-change-receive.md; pinned clif.cpp hash matches PK01.02. Remaining gate is runtime/replay for server transfer/reconnect and map/UI lifecycle, not missing source.

Coverage owner correction: AUDIT-0x00A7 now belongs to PK46 (PK46.02-map-movement.md), still UNREVIEWED; PK45 C2S scope review now lists five remaining rows.

PK45.03 broadcast prefix boundary updated in PK45.03-broadcast-chat.md; selected clif_broadcast emits ordinary/blue/ssss only. Client micc/tool branches remain unresolved reachability.

PK45.03-c2s-owner-scope-review.md now includes final selected-server parser/length/line for five unresolved C2S rows. Their ownership remains open pending client profile and feature dependency review.

PK46.02-storage-transfer-requests.md traces 0x0094/0x00F7 client sends and selected server parser offsets. Coverage primary owner is PK46, PK45 secondary; both rows stay UNREVIEWED. PK45 scope review now lists three unresolved C2S rows.

PK45.03-c2s-owner-scope-review.md now records profile collision for 0x012A and missing current-client sender symbols for 0x0140/0x0234; stronger profile/source-date review required before reassignment.

PK45.03-warp-map-list.md now includes selected server layout, profile guard, caller reachability, and fixed-array boundary. Replay/UI lifecycle remains the only open validation for this leaf; no rows closed.

PK45.03-npc-menu-input.md now includes selected server 0x00B7/0x00B8 field and ordinal/cancel contract. Keep the >254 script choice overflow edge and UI lifecycle in replay review; no row closed.

PK45.03-self-status-parameters.md now has pinned signed emitter evidence; no client Manner storage case found, so signedness risk remains unclassified until negative-value reachability and level-up branches are verified.

PK45.03-npc-string-input.md now has selected server prompt/reply fields and bounded parser copy; verify UI encoding and exact byte boundary with fixtures/replay before closure.

PK45.03-npc-number-input.md now records the selected server parser and client signed/unsigned boundary. Confirm UI accepted range and script semantics before changing behavior or closing the row.

PK45.03-actor-position-correction.md now covers both selected server variants and paired order (01FF slide then 0088 fixpos), including disguise routing. Replay for interpolation/sitting/local-ack behavior remains open.

PK45.03-actor-resurrection.md adds unversioned pc.cpp call-site observations with SHA-256; pin/verify pc.cpp against selected source before treating those caller conditions as authoritative. Production lifecycle replay remains open.

PK45.03-dialogue-text-next-close.md now traces emitters/parser fields plus unversioned npc_scriptcont state transitions; npc.cpp is absent from PK01.02 hashes, so verify that source before relying on lifecycle branches. Replay dialog lock and repeated/late click cases.

PK45.03-action-failure.md includes current five caller observations with hashes; confirm caller files against selected source before treating reachability as proven. Repeat notification/retry-state replay remains open.

PK45.03-actor-move-update.md now distinguishes 0x0086 compatibility handler from selected 0x02EC walking packet; detailed 0x02EC wire fields are in PK45.02-server-walking-20080910.md. Keep short-packet guard and interpolation review open.

PK45.03-ground-item-disappear.md now records two supplemental unversioned map.cpp callers: lifetime expiry and explicit clear send 0x00A1 immediately before map removal/free. Verify against selected source; pickup path and runtime synchronization remain unresolved.

Coverage row AUDIT-0x007B now records S2C ownership and evidence in PK45.03-actor-move-update.md. The old-profile wire contract is still missing; do not close the row until it is sourced.
Coverage row AUDIT-0x0073 now records S2C ownership, supported by the packetver23 accept-enter profile. Keep it unreviewed pending a sourced historical server wire layout.
AUDIT-0x007F evidence is in PK45.03-server-time-notification.md; tick unit/cadence and canonical contract registration remain open. Keep row UNREVIEWED.
0x007F is request driven; runtime replay and pinned-build timer configuration remain unverified. timer.cpp supplemental hash is recorded in PK45.03-server-time-notification.md; keep AUDIT-0x007F UNREVIEWED.
AUDIT-0x0080 is source-backed by PK45.02-actor-vanish.md (7-byte GID/reason S2C packet); do not close pending canonical contract link and replay acceptance.
AUDIT-0x0086 has client-side S2C contract evidence in PK45.03-actor-move-update.md; do not equate it with the selected 0x02EC server contract. Historical mapping remains open.
AUDIT-0x0087 is source-backed by PK45.03-self-move-ack.md: pinned 20080910 server sends 12-byte 0x0087 to SELF. Retain replay and canonical registration follow-up.
AUDIT-0x0088 is source-backed by PK45.03-actor-position-correction.md; replay same-cell and 0x01FF/0x0088 sequencing before closure.
AUDIT-0x008A is the legacy 29-byte actor-action variant; do not merge its contract with selected 0x02E1. PK45.02-actor-action-notify.md records the branch; retain replay and canonical-link follow-up.
AUDIT-0x009C is source-backed by PK45.03-actor-direction.md; retain sit/stand and invalid-value replay and canonical contract follow-up.
0x00B3 inter-server result forwarding is supplemental unpinned chrif.cpp evidence (hash in PK45.03-return-character-select-ack.md). Confirm producer and replay disconnect lifecycle before closure.
AUDIT-0x00C0 server request/reply contract is in PK45.03-emotion-request-ack.md; retain rendering and pending-reply replay follow-up.
AUDIT-0x00E5 is source-backed by PK45.03-commerce-trade-lifecycle.md; keep row UNREVIEWED pending protocol-status semantics and lifecycle replay.
AUDIT-0x00F8 is source-backed by PK45.03-storage-lifecycle.md; replay empty refresh and close/list ordering before closure.
AUDIT-0x0119 is a legacy S2C actor-state form; keep distinct from selected 0x0229. PK45.03-actor-state-change.md records the PACKETVER branch. Runtime state/cart replay remains required.
AUDIT-0x0122 is a legacy cart equipment-list variant; keep separate from selected 0x02D2 and unreviewed pending historical field semantics.
AUDIT-0x0123 is the legacy normal-cart row-list variant; keep separate from selected 0x02E9 pending historical schema review.
AUDIT-0x0124 is the legacy S2C cart-add variant; keep distinct from selected 0x01C5. Cart delta and status evidence in PK45.03-commerce-cart-deltas.md; replay remains open.
AUDIT-0x0138 is client-backed only; its historical server identity/profile is unresolved. PK45.03-commerce-vending-shop-lifecycle.md records inspected server alternatives. Stronger source/profile review and replay required.
AUDIT-0x0139 is emitter-backed; verify unpinned unit.cpp trigger against selected source and replay chase hint expiry/retry before closure.
AUDIT-0x013A is emitter-backed in PK45.03-attack-chase-range.md; replay zero/extreme values and attack/chase behavior before closure.
AUDIT-0x013B is evidence-backed by PK45.03-action-failure.md; unversioned call sites and repeat/retry behavior still need stronger source review and replay.
AUDIT-0x013D is wire-backed by PK45.03-recovery-notification.md; verify the unpinned AP path and replay HP/SP zero/over-range behavior before closure.
AUDIT-0x0145 needs stronger review: client handler exists without a packet-size entry; pinned server uses 0x01B3. Source a historical profile/framing contract before closure.
AUDIT-0x0148 is emitter-backed; replay status/map-load ordering and client render lifecycle before closure.
AUDIT-0x0191 is wire-backed by PK45.03-talkbox-chat-contents.md; verify message field lifetime, encoding, missing-unit behavior and replay before closure.
AUDIT-0x0194 is a 30-byte name-by-GID response, not a party payload; evidence is in PK45.02-actor-name-ack.md. Request correlation and cache cleanup/replay remain open.
AUDIT-0x0195 is source-backed by PK45.02-actor-name-ack.md; request correlation, cache invalidation and UI replay remain open.
AUDIT-0x019B is wire-backed by PK45.03-notify-effect-019b.md; verify unsupported effect codes and replay before closure.
AUDIT-0x01B3 is source-backed in PK45.03-show-image2.md; verify name encoding, type-255 field requirements and asset rendering in replay.
AUDIT-0x01CF server layout is documented in PK45.03-devotion-list.md; stronger review needed for AID-to-effect-vector payload mapping and actor resolution before closure.
PK45 AUDIT-0x01D1: coverage basis now cites PK45.03-blade-stop.md and GameModePacket.cpp:1027. Stronger review still needed for AID identity, flag lifecycle, forced-animation semantics, and replay.
PK45 AUDIT-0x01D2: coverage basis cites PK45.03-combo-delay.md and local handler/cache consumer. Stronger review still needed for tick semantics, lifecycle cleanup, and replay.
PK45 AUDIT-0x01D3: coverage basis cites PK45.03-sound-loop.md and GameModePacket.cpp:972. Stronger review still needed for server action semantics, duration units, name encoding/path behavior, and replay.
PK45 AUDIT-0x01DA: changed direction from unresolved to profile-supported S2C; basis cites PK45.03-actor-walking-decoder.md. Do not treat it as selected-server parity: selected 20080910 emits 0x02EC; historical identity/layout/capture need reconciliation.
PK45 AUDIT-0x01F1: corrected map/unresolved hint to login/char handshake S2C using PK45.03-login-notify-error-01f1.md and LoginMode.cpp:2025. Client variable framing is known; selected server emitter and message contract remain unresolved.
PK45 AUDIT-0x01FF: coverage basis cites selected emitter/layout in PK45.03-actor-position-correction.md. Replay high-jump/fix-position ordering, local movement acknowledgments and remote interpolation before closure.
PK45 AUDIT-0x0229: coverage basis cites PK45.03-actor-state-change.md. Replay state flag transitions, cart/UI synchronization, unknown actors and teardown before closure.
PK45 AUDIT-0x022C: coverage basis cites PK45.03-actor-walking-decoder.md. Treat as alternate-profile only until registration context and authoritative capture resolve identity/layout.
PK45 AUDIT-0x0234: retain C2S packet as unresolved owner; PK45.03-c2s-owner-scope-review.md gives selected parser identity but no client send path. Stronger review must resolve profile/date and feature owner before reassignment.
PK45 AUDIT-0x0297: coverage basis cites PK45.03-commerce-cart-item-lists.md. Treat as alternate profile, retain UNREVIEWED; reconcile profile identity and server field/category semantics before closure.
PK45 AUDIT-0x02B9: coverage basis cites PK45.03-shortcut-key-list.md. Replay slot mapping and update ordering against the matching 2008 SAK client/server profile before closure.
PK45 AUDIT-0x02EB: coverage basis cites PK45.03-accept-enter-variants.md. Replay map-load timing and world initialization order before closure.
PK45 AUDIT-0x02EC: coverage basis cites PK45.02-server-walking-20080910.md and PK45.03-actor-walking-decoder.md. Replay movement tick/interpolation and local/remote actor behavior before closure.
PK45 AUDIT-0x02EF: coverage basis cites PK45.03-actor-appearance-font.md. Review account-ID/AID resolution, unsupported font values, resource lookup and visible refresh before closure.
PK45 AUDIT-0x022C/0x07F7/0x0856: direction remains unresolved despite client receive handlers; alternate profiles and server packet-database registrations lack authoritative capture reconciliation. See PK45.03-actor-walking-decoder.md.
PK45 AUDIT-0x0B0D: coverage basis cites PK45.03-remove-effect-0b0d.md. Do not treat as active selected 2008 behavior; stronger review may decide disposition for later-profile-only packet and client-handler absence.
PK45 AUDIT-0x012A/0x0140: do not reassign based only on selected parser names. PK45.03-c2s-owner-scope-review.md documents 0x012A semantic collision and absent 0x0140 client sender; resolve client profile/date and owner.
PK46.02: added PK46.02-map-trade-request.md for 0x00E4. Review selected server parser validation and trade acceptance/rejection/timeout cleanup; no row closed.
PK46.02: added PK46.02-map-trade-ack-request.md for 0x00E6. Review server reply/result order and stale/duplicate invitation behavior; keep AUDIT-0x00E6 UNREVIEWED.
PK46.02: added PK46.02-map-trade-add-item-request.md for 0x00E8. Verify server index-0 currency convention and item/amount validation against parser source and replay; keep row UNREVIEWED.
PK46.02: added 0x00EB and 0x00EF leaves in PK46.02-map-trade-lock-request.md and PK46.02-map-trade-commit-request.md. Review server lock and transaction lifecycle before closure.
PK46.02: added PK46.02-map-trade-cancel-request.md for 0x00ED. Replay late responses and send-failure/disconnect races before closure; keep row UNREVIEWED.
PK46.02: added PK46.02-map-emotion-request.md for 0x00BF. Verify server ID/action mapping and one-second boundary with selected source/replay; keep AUDIT-0x00BF UNREVIEWED.
PK46.02: added PK46.02-map-global-chat-request.md for profile-dependent global chat. Confirm the active deployment profile and selected server parser text contract before closure; keep row UNREVIEWED.
PK46.02: added PK46.02-map-whisper-request.md for 0x0096. Verify parser name/text validation and non-ASCII/NUL behavior against selected server; keep row UNREVIEWED.
PK46.02: added PK46.02-map-reject-whisper-request.md for 0x00CF. Verify whether parser action changes persistent ignore state or only rejects a pending whisper; keep row UNREVIEWED.
PK46.02: added PK46.02-map-private-message-ignore-requests.md for 0x00D0/0x00D3. Search raw-buffer and profile-specific send paths in PK16 before any disposition; keep rows UNREVIEWED.
PK46.02: added PK46.02-map-load-end-ack.md for 0x007D. Replay initial-send failure, repeated retry and synchronization threshold against selected map entry; keep row UNREVIEWED.
PK46.02: added PK46.02-map-actor-name-request.md for modern 0x008C. Keep legacy 0x008C global-chat meaning separate; review response/cache correlation and replay before closure.
PK46.02: added PK46.02-map-ground-skill-request.md for 0x0113. Stronger review needed for source/destination range semantics and server rejection/result branches; keep row UNREVIEWED.
PK46.02: added PK46.02-map-close-vending-request.md for 0x012E. Replay close/send-failure/pending-open races; keep row UNREVIEWED.
PK46.02: added PK46.02-map-vending-list-request.md for 0x0130. Trace the action-8 UI label and server result path; keep row UNREVIEWED.
PK46.02 0x0130: stronger review must locate the caller for unnamed action 8 or classify it as an unreachable client send path; no current menu item maps it.
PK46.02: added PK46.02-map-vending-purchase-request.md for 0x0134. Verify 0x0135 emitter/result fields and stale stock/seller behavior against selected server and replay; keep row UNREVIEWED.
PK46.02: added PK46.02-map-guild-info-request.md for 0x014F. Trace the five reply subtypes and 0x014D permissions sequence before closure; keep row UNREVIEWED.
PK46.02: added PK46.02-map-guild-emblem-request.md for 0x0151. Trace response/cache update and invalidation against selected server; keep row UNREVIEWED.
PK46.02: added PK46.02-map-guild-member-position-request.md and PK46.02-map-guild-leave-request.md for 0x0155/0x0159. Verify server authorization and response lifecycle; keep rows UNREVIEWED.
PK46.02: added PK46.02-map-guild-position-info-request.md for 0x0161. Confirm repeated ID and permission/tax field semantics against selected server before closure; keep row UNREVIEWED.
PK46.02: added PK46.02-map-guild-invite-request.md for 0x0168. Trace invite accept/reject and membership update against server before closure; keep row UNREVIEWED.
PK46.02: added PK46.02-map-guild-invite-reply-request.md for 0x016B. Trace prompt creation and server membership result; keep row UNREVIEWED.
PK46.02: added PK46.02-map-guild-notice-request.md for 0x016E. Verify selected server notice persistence and UTF-8 field boundaries before closure; keep row UNREVIEWED.
PK46.02: added PK46.02-map-guild-alliance-reply-request.md for 0x0172. Trace alliance prompt origin and server state update; keep row UNREVIEWED.
PK46.02: added PK46.02-map-card-composition-request.md for 0x017A. Trace composition response and selection/cancel flow against server before closure; keep row UNREVIEWED.
PK46.02: added PK46.02-map-change-cart-request.md for 0x01AF. Verify cart availability and state-update response against selected server; keep row UNREVIEWED.
PK46.02 0x01B2: stronger review needed for the inactive close branch that sends an empty frame. Do not assume it opens a valid shop; compare intended UI action and server parser before disposition.
PK46.02: added PK46.02-map-add-friend-request.md and PK46.02-map-friend-reply-request.md for 0x0202/0x0208. Verify server result/cache updates and prompt origin; keep rows UNREVIEWED.
PK46.02: added PK46.02-map-mail-window-request.md and PK46.02-map-mail-send-request.md for 0x0246/0x0248. Review mail window send-failure state plus server delivery and credit semantics; keep rows UNREVIEWED.
PK46.02: added PK46.02-map-view-player-equipment-request.md for 0x02D6. Stronger feature review needed to decide whether ignored 0x02D7 is intentional or the inspect response is missing; keep row UNREVIEWED.
PK46.02 0x0438: stronger review required for all skill-target callers and active profile mapping; see PK46.02-map-skill-target-request-0438.md. Do not collapse PacketVer22 0x0072 into this row.

PK46.02: added PK46.02-map-party-invite-request.md for 0x00FC. UI player-context action, 6-byte send and selected registry parser/length traced; parser body and effects remain open; no row closed.
PK46.02: added PK46.02-map-npc-contact-request.md for 0x0090. Map click eligibility, 7-byte send and selected registry parser/length traced; parser body and NPC session effects remain open; no row closed.

PK46.02: added PK46.02-map-storage-close-request.md for 0x0193. Storage-window action, two-byte send and selected registry parser/length traced; result lifecycle and parser body remain open; no row closed.

PK46.02: added PK46.02-map-npc-dialog-close-request.md for 0x0146. NPC dialog close actions, six-byte send and selected registry match traced; the client clears local dialog state before send success; parser and recovery semantics remain open.

PK46.02: added PK46.02-map-npc-next-request.md for 0x00B9. Dialog Next action, six-byte send and selected registry match traced; UI clears action before send completion, with failure/recovery semantics open.

PK46.02: added PK46.02-map-npc-menu-selection-request.md for 0x00B8. NPC menu action, seven-byte send and selected registry match traced; choice semantics and send-failure UI recovery remain open.

PK46.03: added PK46.03-loginmode-character-send-paths.md to trace native UI -> LoginMode -> active-profile 0x0066/0x0067 sends. Both sender results are ignored; mode/result lifecycle remains open; no rows closed.

PK46.03: added PK46.03-character-server-entry-path.md for active-profile 0x0065 after endpoint/auth gates; production request capture linked; send failure and response lifecycle remain open.

PK46.02 stronger review: Packet.h assigns 0x0072 to both LegacyMapServerSend::kWantToConnection and PacketVer22MapServerSend::kUseSkillToId; selected registry maps 0072 to the 25-byte skill parser. Reconcile profile coexistence and 20080910 applicability before treating the legacy connection send as valid; keep requests separate and rows open.

PK46.02: added PK46.02-map-status-increase-request.md for 0x00BB. Status-window action, five-byte send and selected parser/length/field positions traced; stat-cost/refusal behavior remains open.

PK46.02: added PK46.02-map-shortcut-slot-update-request.md for 0x02BA. Shortcut drag/drop mutations, 11-byte send and selected parser/field positions traced; persistence and send-failure reconciliation remain open.

PK46.02: materialized all 177 nonempty parser registrations from the selected final registry in PK46.02-c2s-map-request-inventory.csv, with coverage ownership joins and partial-note pointers. This inventory is a work index, not completed per-request parity.

PK46.02: added PK46.02-map-npc-string-input-request.md for 0x01D5, linking existing PK45.03 prompt/reply evidence. Variable frame and submit path traced; encoding, parser bounds and failure recovery remain open.

PK46.02: added PK46.02-map-npc-number-input-request.md for 0x0143, linking existing PK45.03 prompt/reply evidence. Numeric UI conversion and ten-byte send traced; parser value bounds and failure recovery remain open.

PK46.02: added PK46.02-map-party-create-request.md for 0x00F9. Party-option submit, fixed 26-byte send and selected registry match traced; name semantics and pending option/result lifecycle remain open.

PK46.02: added PK46.02-map-npc-shop-deal-type-request.md for 0x00C5, linking PK45.03 shop evidence. Buy/sell selection and seven-byte send traced; shop-result lifecycle remains open.

PK46.02: added PK46.02-map-npc-shop-purchase-request.md (0x00C8) and PK46.02-map-npc-shop-sell-request.md (0x00C9). Confirm actions, variable frames and row layouts traced; parser validation and result semantics remain open.

PK46.02: added PK46.02-map-party-invite-reply-request.md for 0x00FF. Accept/reject action, ten-byte send and selected registry match traced; prompt provenance and failure recovery remain open.

PK46.02 stronger review: 0x0100 is reached only through the current master-only disband helper in the bounded call-site search; it sends member removals then leave without waiting. Its aggregate result can mask partial failure. Inspect non-master leave reachability and server sequence semantics.
PK46.02: added PK46.02-map-party-leave-request.md (0x0100) and PK46.02-map-party-member-removal-request.md (0x0103); both retain rows open pending server sequencing/authorization review.

PK46.02 stronger review: UIEquipWnd's Remove Option action currently routes to BuildActiveCartOffPacket/0x012A, while the selected final registry maps 012a to clif_parse_RemoveOption. Length agrees but request identity is unresolved; inspect effective profile and handler body before closure.

PK46.02: added PK46.02-map-exit-to-windows-request.md for 0x018A. Exit-menu action, pending-disconnect guard, send rollback and selected registry match traced; server disconnect timing remains open.

PK46.02 stronger review: zone-entry path has PacketVer22 0x009B/26 and PacketVer23 0x0436/19 registered, plus legacy 0x0072 layouts that collide numerically with the selected skill parser; resolve effective profile family and parser parity before closure.
PK46.02: added PK46.02-map-zone-entry-request.md to connect LoginMode zone connection to profile-specific request builders; send result/profile matrix remain open.

PK46.02 stronger review: automatic time sync sends modern 0x0089/8 matching clif_parse_TickSend, but its legacy 0x007E/6 branch conflicts with selected registry 007e length 102 clif_parse_UseSkillToPosMoreInfo. Prove legacy profile unreachable or resolve identity before closure.
PK46.02: added PK46.02-map-time-sync-request.md with both timer branches and response-state update behavior; profile parity remains open.

PK46.02: added PK46.02-unmatched-registered-request-paths.md for 0x0099, 0x00C1, 0x00CC and 0x00CE. No current sender was found in the bounded client search; retain PK23/PK47 primary ownership and UNREVIEWED rows pending their review.

PK46.02: added PK46.02-unimplemented-chat-room-request-paths.md for 0x00D5/00D9/00DE/00E0/00E2/00E3. Current source explicitly says chat-room UI/interaction is not wired; preserve PK24/PK31 owners and UNREVIEWED rows pending owner review.

PK46.02: added PK46.02-map-party-option-change-request.md for 0x0102. Existing-party option UI and post-create follow-up sends traced; server authorization/default-bit behavior remains open.

PK46.02: added PK46.02-party-chat-request-path.md for 0x0108. Current chat submission exposes global/whisper paths only in the bounded search; preserve PK17 ownership and UNREVIEWED status pending owner review.

PK46.02: added PK46.02-unimplemented-cart-transfer-request-paths.md for 0x0126–0x0129. Current cart activation/selection exists, but no transfer sender was found in the bounded source search; preserve PK37 ownership pending review.

PK46.02 checkpoint (2026-11-04): 96 of 177 accepted C2S map requests have partial note pointers; 81 lack one. Added a bounded negative-path note for registry request 0x012F; the current traced vending UI emits 0x01B2. All request comparisons remain open, with PK37 ownership preserved.

PK46.02 follow-up (2026-11-04): added a bounded no-sender finding for selected-registry request 0x0118 (StopAttack). The coverage row remains UNREVIEWED with no primary owner, and alternate/generic send routes need review. Inventory now has partial pointers for 97 of 177 requests; 80 have none. No rows closed.

PK46.02 continuation (2026-11-04): recorded bounded unresolved sender/profile findings for 0x011D RequestMemo, 0x013F GM_Item_Monster, and alternate map-move request 0x0140. Inventory now has partial note pointers for 100 of 177 requests; 77 have none. These notes are not parity conclusions; no rows closed.

PK46.02 continuation (2026-11-04): recorded bounded sender search gaps for selected registry requests 0x0149, 0x0153, 0x015B, 0x015D, and 0x0165. The next requests are alliance/guild-message paths. Inventory currently has partial note pointers for 105 of 177 requests; 72 have none. No rows closed.

PK46.02 continuation (2026-11-04): documented bounded guild/chat/alliance gaps for 0x0170/0x017E/0x0180/0x0183, and production/reset/GM gaps for 0x018E/0x0197/0x0198/0x019C/0x019D. Inventory pointers: 114 of 177; 63 have no pointer. All remain partial/open; no rows closed.

PK46.02 continuation (2026-11-04): added pet request gaps (0x019F/0x01A1/0x01A5/0x01A7), traced the actual 0x00BF emotion command against distinct 0x01A9 registry request, traced making-arrow selection/cancel 0x01AE, and recorded GMShift gaps 0x01BA/0x01BB. Inventory pointers: 122/177; 55 unpointed. No requests closed.

PK46.02 continuation (2026-11-04): documented CreateParty2 0x01E8 separately from current CreateParty 0x00F9, bounded gaps for GM recall/AutoSpell/novice/adoption/repair/friend requests, PVP/ranking/refine/homunculus requests, and homunculus/storage-password/mail requests. Inventory pointers: 152/177; 25 unpointed. No rows closed.

PK46.02 continuation (2026-11-04): added bounded request-path gaps for auction/feeling/cooking, mail-return/auto-revive/mercenary/cash-shop, and newer party/quest/system/service requests. Every accepted map request now has at least one partial note pointer in the inventory (177/177); pointers do not mean full per-request comparison or closure. No rows closed.

PK46.03 follow-up (2026-11-04): traced the login window's request-account and intro buttons into explicit not-implemented LoginMode branches; neither emits a packet. Continue with real account-login variant/profile paths, then full PK46.02 field comparisons and PK46.04 runtime review.

PK46.03 account login follow-up (2026-11-04): login UI to account connection and default 0x0064/55-byte serializer path are source-traced, with service/settings hash and alternate packet branches called out. Compare all variants with pinned server parsers; runtime verification remains open.

PK46.04 self-review correction (2026-11-04): removed stale wording that PK46.02 had only four evidence leaves. Inventory has 177 partial note pointers, but most rows are not full per-request comparisons; continue against the schema and do not treat pointers as coverage closure.

PK46.04 continuation wording refreshed (2026-11-04): all 177 PK46.02 requests have partial pointers, but the required full action/profile/field-map/server-handler comparisons are still outstanding. PK46.03 now includes account-login client branch tracing; independent parser comparison across variants remains open. Replay/build gate remains unverified.

PK46.03 account-login variant follow-up (2026-11-04): client-side field maps added for legacy, login-channel, PC-bang, Login4, and password-hash request/response variants. Account parser comparison and deployed settings/profile applicability need stronger review; no PK46.03 rows closed.

PK46.04 build checkpoint (2026-11-04): offline CMake regeneration succeeded with cached dependencies, and the current open_midgard Release target built successfully. Build fixes: Windows/Winsock include/platform guards, constexpr BR4 table input, and explicit zlib/Lua target linkage. RO_BUILD_PACKET_REPLAY_TESTS=OFF; replay executable absent and replay tests not run. This does not establish packet parity.

PK46.02 zone-entry follow-up (2026-11-04): exact client layouts now documented for 0x0436/19, 0x009B/26, and 0x0072/19 or 22. Source default resolves PacketVer23 plus ZoneConnectProfile=inherit; 0x0072 remains incompatible with the selected 25-byte skill parser unless deployment profile proves that branch unreachable. SendPacket result is ignored. No row closed.

PK46.02 zone-entry failure follow-up (2026-11-04): packet-builder failure and SendPacket failure now both disconnect/clear connection state, surface the failure, and return to login. Current Release build passed. Runtime retry/UI behavior remains unverified; no row closed.


PK46.04 replay follow-up (2026-11-04): replay gate was configured ON, but the target fails to compile with RO_ENABLE_QT6_UI=OFF because the replay source directly uses QtUiStateAdapter/QVariantMap; no replay runtime result exists. Verify in a Qt-enabled environment or split the Qt adapter case into an explicitly gated test while preserving its coverage. Production Release build success remains separate evidence.

PK45.03 follow-up (2026-11-04): client source now confirms NPC string input has no configured character cap and passes std::string data, and repeated 0x00B7 menu packets replace the existing client menu state. Remaining stronger-review items: non-ASCII and encoded byte boundary/truncation; malformed delimiters and server behavior above 254 menu choices.

PK45.03 dialogue lifecycle follow-up (2026-11-04): client cleanup/re-prompt state is now source-mapped. Selected-server script-lock release timing and exceptional script termination still need stronger server/runtime review.

PK45.03 movement follow-up (2026-11-04): production stream decoder rejects a fixed-size 0x0086 frame shorter than the active table's 16-byte size before dispatch. The 12-byte handler guard is only relevant to direct invocation or alternate table contexts; PK45.04 still needs movement timing/runtime review.

PK45.03 resurrection follow-up (2026-11-04): client-side 0x0148 size, trailing-field handling, absent-actor behavior, and message/state-reset order are source-mapped; only broader server/cross-packet runtime ordering remains open.

PK45.03 actor-state follow-up (2026-11-04): receive-to-actor-state transition is source-mapped for body/effect/health/PK fields and cart/bird dispatch. Visual sit/cart teardown, actor removal/unknown GIDs, and alternate-profile arrivals remain stronger replay/runtime items.

PK45.03 pickup follow-up (2026-11-04): client uses one pending ground-object AID; success consumes/clears it, nonzero result keeps it for timed retry while the ground item exists. Ack has no object ID, so stale/duplicate ack ordering and selected-server emitter still need replay/server-source review.

PK45.03 cart-list follow-up (2026-11-04): client receiver enforces whole-row framing, replaces only the incoming cart category, rejects partial rows without committing, and treats a header-only packet as category clear. Selected-server emitter/schema and replay validation remain open.

PK45.03 cart-delta follow-up (2026-11-04): receiver add merges quantity at matching item index while replacing row metadata; remove subtracts or erases at the amount threshold. Both use minimum-length checks and accept extra bytes. Replay still needed for malformed/large amounts, duplicates/reordering, and list/delta transaction order.

PK45.03 cart inventory boundary follow-up (2026-11-04): equipment/normal cart row offsets are mapped; 26-byte equipment tail remains uninterpreted. Cart receiver writes commerce cart state only, so session-inventory transfer consistency and ack ordering still need source/replay review.

PK45.03 position-correction follow-up (2026-11-04): both 0x0088 and 0x01FF use the same 10-byte handler; same/previous-cell corrections stop and snap movement, clear local pending ack, and re-register actor position. Replay still needed for visual timing and repeated/cross-packet ordering.

PK45.03 emotion follow-up (2026-11-04): focused reference search found no reader/timeout for m_reqEmotionTick; the pending flag clears only on the special failure case or a valid local actor emotion packet. Lost ack can leave the informational flag set. Confirm intended timeout/missing-actor policy before changing behavior.

PK45.03 inventory snapshot follow-up (2026-11-04): added row-alignment rejection before normal/equipment inventory clear, preserving prior state on partial tails; empty-list clear behavior remains. Equipment clear preserves ammo item types 10/16/17 and rebuilds appearance. Release build passed; replay coverage remains needed for partial rows/empty lists/ammo/appearance.

PK45.03 actor font follow-up (2026-11-04): source-wide search found no rendering consumer for m_charfont; 0x02EF stores the ID but visible font parity is not implemented/proven in this tree. Renderer/assets contract needs stronger graphics review before implementation. Robe type 12 billboard invalidation still needs separate source/render review.

PK45.03 companion-skill follow-up (2026-11-04): shared 37-byte list parser now rejects partial tails before clearing the selected skill set; empty-list clear behavior remains. Release build passed. Target domains, shared upgrade-flag semantics, slot types/order, and UI refresh still need replay/profile review.

PK45.03 storage-list follow-up (2026-11-04): normal/equipment storage lists now reject non-integral row payloads before applying entries; valid empty lists remain accepted after 0x00F2 clears state. Release build passed. Replay of empty/split lists, malformed tails, and add/remove ordering remains open.

PK45.03 shortcut follow-up (2026-11-04): 0x02B9 slots are mapped in wire order to 27 session indices; client UI maps 9 slots per page over 3 pages, with zero ID clearing. Selected-profile capture and rendered refresh timing remain for replay/UI review.

PK45.03 NPC string sender follow-up (2026-11-04): traced GameMsg dispatch into RequestNpcInputString; sender uses strlen bytes, includes a zero terminator via zero-initialized packet storage, and rejects total lengths above 0xFFFF. No conversion is performed here. UI encoding and boundary interoperability remain replay/fixture gaps.

PK46.02 tracker reconciliation (2026-11-04): state.json now indexes all 96 existing PK46.02 evidence notes; package remains IN_PROGRESS. The accepted-request contract and user-path comparison across all 177 map requests is still incomplete; no discovery rows were closed.

PK46.02 opcode census (2026-11-04): 177 unique selected-registry parser opcodes are explicitly mentioned across 97 indexed PK46.02 notes. This verifies name coverage only; field/profile/user-path comparison for all 177 and discovery review remain open.

PK46.02 movement request follow-up (2026-11-04): verified the exact client coordinate bit packing and 0..1023 bounds for 0x00A7; the packed three-byte destination at offset 5 matches the selected 8-byte server registration. Legacy profile selection and UI/replay evidence remain open.

PK46.02 add-friend follow-up (2026-11-04): verified current rAthena parser source hashes match PK01.02 snapshot. The selected 0x0202 parser rejection cases and success behavior (pending IDs plus target-side 0x0207) are now recorded; 0x0208/client friend-list update and replay remain open.

PK46.02 friend-flow follow-up (2026-11-04): traced selected 0x0208 server reply decisions in the hash-matched source: offset 6 is ignored, acceptance uses reciprocal pending character IDs, and friend_auto_add controls reciprocal insertion. Client 0x0209/result and cached list refresh remain open.

PK46.02 friend workflow follow-up (2026-11-04): source-traced 0x0202 -> 0x0207 target prompt -> 0x0208 response -> 0x0209 result through selected rAthena and current client handlers. Status 0 updates friend UI; failures are generic and do not mutate it. Runtime replay/stale prompt behavior remain open.

PK46.02 identify-item follow-up (2026-11-04): traced valid identify through skill_identify -> 0x0179 -> client inventory flag/window refresh; invalid and cancel requests return without this acknowledgement. Recorded the previously unpinned skill.cpp hash. Runtime replay and visible-dialog acceptance remain open.

PK46.02 drop failure follow-up (2026-11-04): selected PACKETVER=20080910 returns basic 0x00AF with count zero for a rejected drop. HandleItemRemove now clears the pending request without deleting inventory. The 0x07FA later-profile field and runtime replay remain open.

PK46.02 equipment request follow-up (2026-11-04): resolved the ammo position branch at source level. Client inventory normalizes types 10/16/17 to EQP_AMMO and sends nonzero 0x8000; selected server ignores that position for IT_AMMO and forces EQP_AMMO. Ack lifecycle/replay remain open.

PK46.02 unequip follow-up (2026-11-04): selected 20080910 0x00AC result semantics match the client receiver (zero failure, nonzero success). Server invalid-index failure uses index zero, which cannot match the client pending item; reachability/correlation needs stronger review.

PK46.02 storage failure follow-up (2026-11-04): selected rAthena emits 0x00F6 count zero on failed deposit/withdraw paths; successful deletes carry a positive amount. HandleStorageItemRemoved now ignores nonpositive counts rather than erasing the storage row. Ordering/replay remain open.

PK46.02 warp-skill selection follow-up (2026-11-04): verified `mapindex_getmapname` preserves the literal cancel sentinel and `skill_castend_map` clears pending menu-skill state without casting. Destination-name semantics and runtime replay remain open.

PK46.02 warp-skill follow-up (2026-11-04): unknown AL_WARP destination yields a server skill-failure response and clears pending menu-skill state. General map-name case comparison is deferred for stronger review of the map-index DB comparator; replay remains open.

PK46.02 inspect-equipment follow-up (2026-11-04): corrected the prior no-response assessment. Selected server emits variable 0x02D7 snapshots when permission/map checks pass; client Commerce.Receive parses rows and QtCommerce publishes them to UI. Actual visible rendering and replay remain open.

PK45.03/PK46.02 viewed-equipment correction (2026-11-04): corrected the stale “no response handler” conclusion. The pinned server emits 0x02D7; Commerce.Receive and QtCommerce parse/publish the 43+26n snapshot. Source hashes and selected guards are pinned in PK01.02. Rendered UI and replay remain open.
