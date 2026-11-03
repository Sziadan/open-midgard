# Packet implementation audit

Snapshot: 2026-11-03. Client default `packetver23`; rAthena custom `PACKETVER 20080910`, `PACKETVER_SAK_NUM 20080910`.

This is a client-facing login/character/map protocol audit, not a request to implement inter-server login/char/map traffic in the client. No gameplay code or server code was changed during this audit.

The audit evaluates rAthena preprocessor guards, compiles its actual registry/packed wire sizes, executes a stand-alone probe of the client's actual final registration order and packet-size implementation, and reviews the resulting gaps against source. This is a static audit, not a claim that every handled packet has passed a live replay.

## What the counts mean

- 990 opcode rows in the combined configured registry/client/declaration inventory.
- 603 final map registry entries from 886 assignments; last assignment wins.
- 377 client size-table/router entries; 136 named map handler functions indexed for review.
- 21 manually reviewed findings, with source evidence and recommended behavior.
- 19 explicit empty map handlers. Every one is listed as unfinished or a direction/version issue requiring resolution.
- 195 incoming declaration/reference candidates have no handler; 119 of those also have active sender/reference evidence and a known wire length.
- 182 incoming candidates have no client framing; 106 have sender/reference evidence plus a known layout.
- 177 server-accepted request opcode rows are included in the outbound inventory. Literal/profile constructor evidence is not proof of a working UI-to-server feature.

**Do not read these numbers as counts of guaranteed live failures.** The server database retains historical aliases, some declarations are unavailable under 2008, and some newer sender functions are compiled even when their call sites require a later feature. The inventory preserves that reachability uncertainty. Registered handlers are marked “semantics not certified,” never “complete.”

## Highest-priority implementation and correctness findings

### P01 · P0 · Account-ID handshake is represented as fake opcodes

Packets: `0x8482`, `0x8483`.

Evidence level: Confirmed source defect.

The character server sends a raw four-byte account ID before normal packets. The client instead special-cases 0x8482/0x8483 as four-byte packets and ignores them. Other account IDs can be mistaken for real packet headers; this is not a general handshake implementation.

**Required handling:** Consume and validate the raw account-ID prefix in the correct connection state, including fragmented reads, before starting ordinary framing.

Sources: [GronPacket.cpp:323](D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/network/GronPacket.cpp:323), [LoginMode.cpp:1935](D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/LoginMode.cpp:1935), [char_clif.cpp:850](C:/Users/sziad/source/repos/rathena-master/src/char/char_clif.cpp:850).

### P02 · P0 · Unknown packet recovery can destroy stream alignment

Packets: connection framing / handshake state.

Evidence level: Confirmed source defect.

RecvPacket drops two bytes for an unknown opcode and scans the remaining payload as headers. Missing implementations therefore affect unrelated later packets, rather than only losing one feature.

**Required handling:** Implement correct framing for supported packets and use explicit connection-state/error recovery when a boundary cannot be determined. Never pretend a payload scan is successful decoding.

Sources: [Connection.cpp:533](D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/network/Connection.cpp:533).

### P03 · P0 · Guild member status is still unframed and unhandled

Packets: `0x01F2`.

Evidence level: Confirmed source defect.

rAthena sends a 20-byte ZC_UPDATE_CHARSTAT on guild member login/logout. It is absent from the client size table and router. The newly added 0x016D/0x01EB handlers do not replace it.

**Required handling:** Decode account/character/status/appearance fields, update the guild roster and visible-member/minimap state, and consume the exact 2008 layout.

Sources: [clif.cpp:8682](C:/Users/sziad/source/repos/rathena-master/src/map/clif.cpp:8682), [GronPacket.cpp:25](D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/network/GronPacket.cpp:25).

### P04 · P0 · Pet status length is wrong for the configured server

Packets: `0x01A2`.

Evidence level: Confirmed source defect.

The client frames 37 bytes. The compiled 20080910 server struct is 35 bytes; its extra job word is enabled only from 20081126. The client also has no pet-status handler. This can consume two bytes of the next packet.

**Required handling:** Use the profile-correct 35-byte layout and implement pet state/UI, with different lengths only under the appropriate newer profile.

Sources: [GronPacket.cpp:260](D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/network/GronPacket.cpp:260), [packets_struct.hpp:5441](C:/Users/sziad/source/repos/rathena-master/src/map/packets_struct.hpp:5441).

### P05 · P0 · Ground-skill text request uses the wrong active contract

Packets: `0x0190`, `0x007E`.

Evidence level: Confirmed source defect.

The client emits PACKET_CZ_USESKILLTOPOSINFO as opcode 0x0190, 90 bytes. The evaluated server database ends with 0x0190 as a 19-byte ActionRequest; UseSkillToPosMoreInfo is 0x007E, 102 bytes with padding/offsets. Sending the current request can corrupt the server receive stream.

**Required handling:** Build this request from the actual 20080910 server opcode, length and field offsets; test the real request against the server parser rather than only the old Ref layout.

Sources: [Packet.h:444](D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/network/Packet.h:444), [MapSendProfile.cpp:942](D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/network/MapSendProfile.cpp:942), [clif_packetdb.hpp:531](C:/Users/sziad/source/repos/rathena-master/src/map/clif_packetdb.hpp:531).

### P06 · P2 · Duplicate registration hides an advertised inventory handler

Packets: `0x01EF`.

Evidence level: Confirmed registration/profile conflict; active impact is version-dependent.

Actual final registration replaces HandleEquipInventoryList with commerce cart handling. The receive profile advertises 0x01EF as timed equipment inventory while commerce interprets it as a normal cart list. For the active 2008 server the inventory opcode is 0x02D0, so this is primarily a conflicting compatibility claim, not proof that live inventory currently breaks.

**Required handling:** Make aliases profile-specific and ensure one unambiguous layout and owner per opcode. Remove unsupported compatibility claims or provide a tested route.

Sources: [MapSendProfile.cpp:215](D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/network/MapSendProfile.cpp:215), [GameModePacket.cpp:8765](D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/GameModePacket.cpp:8765).

### P07 · P1 · Party positions are discarded

Packets: `0x0107`.

Evidence level: Confirmed source defect.

A real server-generated party minimap position packet reaches an empty handler. The minimap has m_partyPosList rendering support, but this notification does not feed it.

**Required handling:** Update/remove party markers by account ID and honor the server removal sentinel and map changes.

Sources: [GameModePacket.cpp:8801](D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/GameModePacket.cpp:8801), [clif.cpp:8065](C:/Users/sziad/source/repos/rathena-master/src/map/clif.cpp:8065).

### P08 · P1 · Map cell and map-property changes are discarded

Packets: `0x0192`, `0x0199`, `0x01D6`, `0x02E7`.

Evidence level: Confirmed source defect.

These registrations all use HandleIgnorePacket. Dynamic map cell changes and map mode/restriction/property updates cannot be represented by consuming their bytes.

**Required handling:** Apply cell flags/appearance and the profile-specific map-property fields to world state, restrictions and indicators; verify each family against Ref and active rAthena senders.

Sources: [GameModePacket.cpp:8811](D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/GameModePacket.cpp:8811), [clif.cpp:5363](C:/Users/sziad/source/repos/rathena-master/src/map/clif.cpp:5363).

### P09 · P1 · NPC class-change notification is discarded

Packets: `0x01B0`.

Evidence level: Confirmed source defect.

ZC_NPCSPRITE_CHANGE is registered to the empty handler. Handling other sprite-change opcodes does not implement this wire contract.

**Required handling:** Decode the target and class/view change, rebuild the correct sprite or model, and invalidate render/picking caches.

Sources: [GameModePacket.cpp:8799](D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/GameModePacket.cpp:8799), [clif.cpp:1471](C:/Users/sziad/source/repos/rathena-master/src/map/clif.cpp:1471).

### P10 · P1 · Storage password flow is a message/log stub

Packets: `0x023A`, `0x023B`, `0x023C`.

Evidence level: Confirmed source defect.

The request handler explicitly says the flow is not implemented; the result handler only logs result/error count. No complete prompt/reply/retry state machine exists.

**Required handling:** Provide the proper password UI and protocol response, handle success/failure/retry counts, and resume storage only when authorized.

Sources: [GameModePacket.cpp:2416](D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/GameModePacket.cpp:2416), [GameModePacket.cpp:2427](D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/GameModePacket.cpp:2427).

### P11 · P1 · Equipment/privacy and party-invite settings are discarded

Packets: `0x02C8`, `0x02C9`, `0x02D8`, `0x02D9`, `0x02DA`.

Evidence level: Confirmed source defect.

The server configuration and party invitation-state notifications use empty handlers. Request/acknowledgement state is not reflected in client preferences and privacy controls.

**Required handling:** Implement the supported 2008 settings fields, related commands/UI requests, and server acknowledgements without assuming a local toggle succeeded.

Sources: [GameModePacket.cpp:8832](D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/GameModePacket.cpp:8832), [clif.cpp:17719](C:/Users/sziad/source/repos/rathena-master/src/map/clif.cpp:17719).

### P12 · P1 · Server-directed disconnect is ignored

Packets: `0x02D5`.

Evidence level: Confirmed client stub; server reachability needs testing.

The client registers this to an empty handler. Ref dispatches it to Zc_Isvr_Disconnect. The currently configured rAthena sender was not established by the scan, so event reachability is a separate question from the missing implementation.

**Required handling:** Implement the appropriate disconnect/transition and UI/network cleanup for the supported family.

Sources: [GameModePacket.cpp:8842](D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/GameModePacket.cpp:8842), [GameModePacket.cpp:2384](D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/Ref/GameModePacket.cpp:2384).

### P13 · P1 · Show-EFST notification is only partially implemented

Packets: `0x028A`.

Evidence level: Confirmed partial implementation.

The handler implements level-99 aura and the PC Assumptio bit, but not the other Ref GameModeShowEFST cases. A registered status/effect packet is therefore not full visual support.

**Required handling:** Implement the remaining supported PC/NPC effect flags and removal/lifetime semantics, with visual tests for each effect family.

Sources: [GameModePacket.cpp:599](D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/GameModePacket.cpp:599), [GameModePacket.cpp:4595](D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/Ref/GameModePacket.cpp:4595).

### P14 · P2 · Extended status values are parsed only as icon timing

Packets: `0x043F`.

Evidence level: Confirmed unused fields; per-status impact needs review.

HandlePacket043F reads status, actor, flag and remaining time, but ignores the remaining value fields. Those fields can carry status-specific behavior. This newer receive family is supported by client code but is not the configured 2008 status packet.

**Required handling:** Associate value fields with the relevant status semantics before calling the extended packet implemented.

Sources: [GameModePacket.cpp:1109](D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/GameModePacket.cpp:1109).

### P15 · P1 · Guild rank changes do not refresh local authority

Packets: `0x0156`, `0x0160`, `0x0166`, `0x0174`.

Evidence level: Confirmed state propagation gap.

Guild packets update WindowState and the local rank title, while localGuild.permissions remains sourced from the belong packet. A changed rank or rank mode can leave invite/expel controls and guards stale until another refresh/login.

**Required handling:** Recompute local permissions/title from the current member position after both member-position and position-definition changes.

Sources: [GameModePacket.cpp:5261](D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/GameModePacket.cpp:5261), [QtCommerce.cpp:39](D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/qtui/QtCommerce.cpp:39).

### P16 · P2 · Guild menu permissions are stored but not applied

Packets: `0x014E`.

Evidence level: Confirmed partial implementation.

The menuFlags value is stored by the decoder, but the Qt adapter does not expose it and the guild UI always offers its six tabs.

**Required handling:** Apply the server menu capability mask to tabs/actions while preserving server authority.

Sources: [GuildState.h:47](D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/core/GuildState.h:47), [QtUiStateAdapter.cpp:3820](D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/qtui/QtUiStateAdapter.cpp:3820), [RoGuild.qml:50](D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/qtui/qml/RoGuild.qml:50).

### P17 · P1 · Guild receive support is not complete guild functionality

Packets: `0x015B`, `0x015D`, `0x0170`, `0x017E`, `0x0180`, `0x0183`.

Evidence level: Confirmed feature/request gaps from UI and send-path review.

The guild window implements notices, ranks, skills, emblem upload and leaving, but has no corresponding actions for expulsion, disbanding, initiating an alliance/opposition, removing a relation, or guild-chat sending.

**Required handling:** Add permission-gated actions, proper confirmation where needed, request builders and acknowledgement-driven updates.

Sources: [QtUiStateAdapter.cpp:3851](D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/qtui/QtUiStateAdapter.cpp:3851), [clif.cpp:14584](C:/Users/sziad/source/repos/rathena-master/src/map/clif.cpp:14584).

### P18 · P1 · Group chat transport and filtering are incomplete

Packets: `0x0108`, `0x0109`, `0x017E`, `0x017F`.

Evidence level: Confirmed partial group-chat feature.

Incoming party chat exists, but outbound chat paths provide global and whisper sends rather than party/guild requests. Guild messages are currently recorded with the party channel, so guild and party filtering cannot be independent.

**Required handling:** Implement distinct party/guild send modes and channels, with the server-specific request headers and separate filtering/history behavior.

Sources: [GameMode.cpp:4736](D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/GameMode.cpp:4736), [GameModePacket.cpp:7217](D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/GameModePacket.cpp:7217), [UINewChatWnd.cpp:142](D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/ui/UINewChatWnd.cpp:142).

### P19 · P1 · Legacy mail is compose/send only

Packets: `0x023F`, `0x0240`, `0x0241`, `0x0242`, `0x0243`, `0x0244`, `0x0245`, `0x0247`, `0x024A`, `0x0255`, `0x0257`, `0x0273`, `0x0274`.

Evidence level: Confirmed subsystem gap.

The recent implementation handles composing/sending and its result, but not inbox listing, reading, attachments, deletion, new-mail notification or return handling. Most of their incoming packets are unframed.

**Required handling:** Implement the full legacy-mail state machine and UI, rather than treating a working send acknowledgement as complete mail support.

Sources: [Commerce.cpp:146](D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/Commerce.cpp:146), [clif.cpp:16070](C:/Users/sziad/source/repos/rathena-master/src/map/clif.cpp:16070).

### P20 · P2 · Snapshot parsers accept partial record arrays

Packets: `0x00A3`, `0x00A4`, `0x00A5`, `0x00A6`, `0x0201`, `0x02D0`, `0x02D1`, `0x02E8`, `0x02EA`.

Evidence level: Confirmed validation gap; malformed input required.

Several inventory/storage/friend list handlers loop over complete records without first rejecting a trailing partial record. Some clear the previous snapshot first, so a malformed list can replace good state with a truncated list. Commerce and GuildState already reject partial arrays more consistently.

**Required handling:** Validate the complete payload and decode into temporary state before replacing snapshots; add malformed/empty/fragmentation cases.

Sources: [GameModePacket.cpp:2204](D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/GameModePacket.cpp:2204), [GameModePacket.cpp:2242](D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/GameModePacket.cpp:2242), [GameModePacket.cpp:7673](D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/GameModePacket.cpp:7673).

### P21 · P1 · Server trade distance failure notifies only one peer

Packets: `0x00E7`, `0x01F5`.

Evidence level: Confirmed server behavior defect; client mitigation exists.

In trade_tradeack, the acceptance-distance failure sends TRADE_ACK_TOOFAR only to the accepting client and clears both partners. The requester gets no failure packet. Client preflight/expiry guards mitigate this but do not repair that server behavior.

**Required handling:** For a server-side fix, send the failure/cancellation to both clients before clearing partners, and test different GM permissions and movement between request/accept.

Sources: [trade.cpp:108](C:/Users/sziad/source/repos/rathena-master/src/map/trade.cpp:108), [Commerce.cpp:38](D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/Commerce.cpp:38).

## Every explicit empty receive registration

- **0x0100** — Server defines this as C2S LeaveParty. Resolve/remove the bogus receive registration; do not invent an incoming message. Client length: 2; server: 2.
- **0x0107** — Party minimap position/update/removal. Client length: 10; server: 10.
- **0x0192** — Dynamic map-cell change. Client length: 24; server: 24.
- **0x0199** — Map property/mode state. Client length: 4; server: 4.
- **0x01B0** — NPC sprite/class change. Client length: 11; server: 11.
- **0x01D6** — Map type/property restrictions. Client length: 4; server: 4.
- **0x01F8** — Ref arms the adoption skill; implement adoption flow and verify server version. Client length: 2; server: 2.
- **0x0283** — Map connection account-ID confirmation: decode/validate identity. Client length: 6; server: 6.
- **0x02B1** — Quest list snapshot. Client length: -1; server: -1.
- **0x02B2** — Quest mission/objective information. Client length: -1; server: -1.
- **0x02C9** — Party invitation refusal setting. Client length: 3; server: 3.
- **0x02D3** — Bind-on-equip notification. Client length: 4; server: 4.
- **0x02D5** — Inter-server disconnect/transition. Client length: 2; server: 2.
- **0x02D9** — Server configuration result/state. Client length: 10; server: 10.
- **0x02DA** — Equipment-sharing/config notification. Client length: 3; server: 3.
- **0x02DD** — Battleground team/emblem identity. Client length: 32; server: 32.
- **0x02E7** — Extended map-property state. Client length: -1; server: -1.
- **0x0814** — Buying-store board/list entry; newer family. Client length: 86; server: None.
- **0x0816** — Buying-store board removal; newer family. Client length: 6; server: None.

## Complete missing incoming-handler candidate list

Each row below is unimplemented at dispatch, but not every row is proven reachable during ordinary 2008 play. A known size and sender are stated separately. Names are the server symbols; senders identify the behavior to implement. Historical/unused rows without an established incoming direction remain in the machine inventory rather than being mislabeled as missing game features.

- **0x0074 · HEADER_ZC_REFUSE_ENTER** — server size 3; client size 0; clif_authrefuse. active sender/reference plus known wire layout.
- **0x00C2 · HEADER_ZC_USER_COUNT** — server size 6; client size 6; clif_user_count. active sender/reference plus known wire layout.
- **0x00CD · legacy numeric opcode** — server size 3; client size 0; clif_GM_kickack. active sender/reference plus known wire layout.
- **0x00D2 · HEADER_ZC_SETTING_WHISPER_STATE** — server size 4; client size 4; clif_wisall. active sender/reference plus known wire layout.
- **0x00D4 · HEADER_ZC_WHISPER_LIST** — server size -1; client size -1; clif_PMIgnoreList. active sender/reference plus known wire layout.
- **0x00D6 · HEADER_ZC_ACK_CREATE_CHATROOM** — server size 3; client size 3; clif_createchat. active sender/reference plus known wire layout.
- **0x00D7 · HEADER_ZC_ROOM_NEWENTRY** — server size -1; client size -1; clif_dispchat. active sender/reference plus known wire layout.
- **0x00D8 · HEADER_ZC_DESTROY_ROOM** — server size 6; client size 6; clif_clearchat. active sender/reference plus known wire layout.
- **0x00DA · HEADER_ZC_REFUSE_ENTER_ROOM** — server size 3; client size 3; clif_joinchatfail. active sender/reference plus known wire layout.
- **0x00DB · HEADER_ZC_ENTER_ROOM** — server size -1; client size 0; clif_joinchatok. active sender/reference plus known wire layout.
- **0x00DC · HEADER_ZC_MEMBER_NEWENTRY** — server size 28; client size 28; clif_addchat. active sender/reference plus known wire layout.
- **0x00DD · HEADER_ZC_MEMBER_EXIT** — server size 29; client size 29; clif_chat_leave. active sender/reference plus known wire layout.
- **0x00DF · HEADER_ZC_CHANGE_CHATROOM** — server size -1; client size -1; clif_changechatstatus. active sender/reference plus known wire layout.
- **0x00E1 · HEADER_ZC_ROLE_CHANGE** — server size 30; client size 30; clif_chat_role. active sender/reference plus known wire layout.
- **0x010A · HEADER_ZC_MVP_GETTING_ITEM** — server size 4; client size 0; clif_mvp_item. active sender/reference plus known wire layout.
- **0x010B · HEADER_ZC_MVP_GETTING_SPECIAL_EXP** — server size 6; client size 0; clif_mvp_exp. active sender/reference plus known wire layout.
- **0x010C · HEADER_ZC_MVP** — server size 6; client size 0; clif_mvp_effect. active sender/reference plus known wire layout.
- **0x010D · HEADER_ZC_THROW_MVPITEM** — server size 2; client size 0; clif_mvp_noitem. active sender/reference plus known wire layout.
- **0x011E · HEADER_ZC_ACK_REMEMBER_WARPPOINT** — server size 3; client size 0; clif_skill_memomessage. active sender/reference plus known wire layout.
- **0x0144 · HEADER_ZC_COMPASS** — server size 23; client size 0; clif_viewpoint. active sender/reference plus known wire layout.
- **0x014A · legacy numeric opcode** — server size 6; client size 0; clif_manner_message. active sender/reference plus known wire layout.
- **0x014B · HEADER_ZC_NOTIFY_MANNER_POINT_GIVEN** — server size 27; client size 0; clif_GM_silence. active sender/reference plus known wire layout.
- **0x0188 · HEADER_ZC_ACK_ITEMREFINING** — server size 8; client size 0; clif_refine. active sender/reference plus known wire layout.
- **0x0189 · HEADER_ZC_NOTIFY_MAPINFO** — server size 4; client size 0; clif_skill_teleportmessage. active sender/reference plus known wire layout.
- **0x018B · legacy numeric opcode** — server size 4; client size 0; clif_disconnect_ack. active sender/reference plus known wire layout.
- **0x018C · HEADER_ZC_MONSTER_INFO** — server size 29; client size 0; clif_skill_estimation. active sender/reference plus known wire layout.
- **0x018D · HEADER_ZC_MAKABLEITEMLIST** — server size -1; client size 0; clif_skill_produce_mix_list. active sender/reference plus known wire layout.
- **0x018F · HEADER_ZC_ACK_REQMAKINGITEM** — server size 6; client size 0; clif_produceeffect. active sender/reference plus known wire layout.
- **0x019A · legacy numeric opcode** — server size 14; client size 0; clif_pvpset. active sender/reference plus known wire layout.
- **0x019E · HEADER_ZC_START_CAPTURE** — server size 2; client size 0; clif_catch_process. active sender/reference plus known wire layout.
- **0x01A0 · HEADER_ZC_TRYCAPTURE_MONSTER** — server size 3; client size 0; clif_pet_roulette. active sender/reference plus known wire layout.
- **0x01A2 · HEADER_ZC_PROPERTY_PET** — server size 35; client size 37; clif_send_petstatus. active sender/reference plus known wire layout.
- **0x01A3 · HEADER_ZC_FEED_PET** — server size 5; client size 5; clif_pet_food. active sender/reference plus known wire layout.
- **0x01A4 · HEADER_ZC_CHANGESTATE_PET** — server size 11; client size 11; clif_send_petdata. active sender/reference plus known wire layout.
- **0x01A6 · legacy numeric opcode** — server size -1; client size -1; clif_sendegg. active sender/reference plus known wire layout.
- **0x01AA · HEADER_ZC_PET_ACT** — server size 10; client size 10; clif_pet_emotion. active sender/reference plus known wire layout.
- **0x01AB · HEADER_ZC_PAR_CHANGE_USER** — server size 12; client size 12; clif_changemanner. active sender/reference plus known wire layout.
- **0x01B1 · legacy numeric opcode** — server size 7; client size 7; clif_showdigit. active sender/reference plus known wire layout.
- **0x01E0 · legacy numeric opcode** — server size 30; client size 0; clif_account_name. active sender/reference plus known wire layout.
- **0x01E6 · HEADER_ZC_COUPLENAME** — server size 26; client size 0; clif_callpartner. active sender/reference plus known wire layout.
- **0x01EA · HEADER_ZC_CONGRATULATION** — server size 6; client size 0; clif_wedding_effect. active sender/reference plus known wire layout.
- **0x01F2 · HEADER_ZC_UPDATE_CHARSTAT** — server size 20; client size 0; clif_guild_memberlogin_notice. active sender/reference plus known wire layout.
- **0x01F6 · legacy numeric opcode** — server size 34; client size 0; clif_Adopt_request. active sender/reference plus known wire layout.
- **0x01FC · HEADER_ZC_REPAIRITEMLIST** — server size -1; client size 0; clif_item_repair_list. active sender/reference plus known wire layout.
- **0x01FE · HEADER_ZC_ACK_ITEMREPAIR** — server size 5; client size 0; clif_item_repaireffect. active sender/reference plus known wire layout.
- **0x0205 · HEADER_ZC_DIVORCE** — server size 26; client size 0; clif_divorced. active sender/reference plus known wire layout.
- **0x020D · HEADER_HC_BLOCK_CHARACTER** — server size -1; client size 0; chclif_block_character. active sender/reference plus known wire layout.
- **0x020E · legacy numeric opcode** — server size 32; client size 0; clif_starskill. active sender/reference plus known wire layout.
- **0x0210 · legacy numeric opcode** — server size 22; client size 0; clif_PVPInfo. active sender/reference plus known wire layout.
- **0x0216 · legacy numeric opcode** — server size 6; client size 0; clif_Adopt_reply. active sender/reference plus known wire layout.
- **0x0219 · HEADER_ZC_BLACKSMITH_RANK** — server size 282; client size 0; clif_ranklist. active sender/reference plus known wire layout.
- **0x021A · HEADER_ZC_ALCHEMIST_RANK** — server size 282; client size 0; clif_ranklist. active sender/reference plus known wire layout.
- **0x021B · HEADER_ZC_BLACKSMITH_POINT** — server size 10; client size 0; clif_update_rankingpoint. active sender/reference plus known wire layout.
- **0x021C · HEADER_ZC_ALCHEMIST_POINT** — server size 10; client size 0; clif_update_rankingpoint. active sender/reference plus known wire layout.
- **0x0221 · HEADER_ZC_NOTIFY_WEAPONITEMLIST** — server size -1; client size 0; clif_item_refine_list. active sender/reference plus known wire layout.
- **0x0223 · HEADER_ZC_ACK_WEAPONREFINE** — server size 8; client size 0; clif_upgrademessage. active sender/reference plus known wire layout.
- **0x0224 · HEADER_ZC_TAEKWON_POINT** — server size 10; client size 0; clif_update_rankingpoint. active sender/reference plus known wire layout.
- **0x0226 · HEADER_ZC_TAEKWON_RANK** — server size 282; client size 0; clif_ranklist. active sender/reference plus known wire layout.
- **0x022F · HEADER_ZC_FEED_MER** — server size 5; client size 0; clif_hom_food. active sender/reference plus known wire layout.
- **0x0230 · HEADER_ZC_CHANGESTATE_MER** — server size 12; client size 0; clif_send_homdata. active sender/reference plus known wire layout.
- **0x0236 · HEADER_ZC_KILLER_POINT** — server size 10; client size 0; clif_update_rankingpoint. active sender/reference plus known wire layout.
- **0x0238 · HEADER_ZC_KILLER_RANK** — server size 282; client size 0; clif_ranklist. active sender/reference plus known wire layout.
- **0x0240 · legacy numeric opcode** — server size -1; client size 0; clif_Mail_refreshinbox. active sender/reference plus known wire layout.
- **0x0242 · legacy numeric opcode** — server size -1; client size 0; clif_Mail_read. active sender/reference plus known wire layout.
- **0x0245 · legacy numeric opcode** — server size 3; client size 0; clif_mail_getattachment. active sender/reference plus known wire layout.
- **0x024A · legacy numeric opcode** — server size 70; client size 0; clif_Mail_new. active sender/reference plus known wire layout.
- **0x0250 · legacy numeric opcode** — server size 3; client size 0; clif_Auction_message. active sender/reference plus known wire layout.
- **0x0252 · legacy numeric opcode** — server size -1; client size 0; clif_Auction_results. active sender/reference plus known wire layout.
- **0x0253 · legacy numeric opcode** — server size 3; client size 0; clif_feel_req. active sender/reference plus known wire layout.
- **0x0255 · legacy numeric opcode** — server size 5; client size 0; clif_Mail_setattachment. active sender/reference plus known wire layout.
- **0x0256 · legacy numeric opcode** — server size 5; client size 0; clif_Auction_setitem. active sender/reference plus known wire layout.
- **0x0257 · legacy numeric opcode** — server size 8; client size 0; clif_mail_delete. active sender/reference plus known wire layout.
- **0x025A · HEADER_ZC_MAKINGITEM_LIST** — server size -1; client size 0; clif_cooking_list. active sender/reference plus known wire layout.
- **0x025D · legacy numeric opcode** — server size 6; client size 0; clif_Auction_close. active sender/reference plus known wire layout.
- **0x025F · legacy numeric opcode** — server size 6; client size 0; clif_Auction_openwindow. active sender/reference plus known wire layout.
- **0x0260 · legacy numeric opcode** — server size 6; client size 0; clif_Mail_window. active sender/reference plus known wire layout.
- **0x0274 · legacy numeric opcode** — server size 8; client size 0; clif_Mail_return. active sender/reference plus known wire layout.
- **0x0287 · HEADER_ZC_PC_CASH_POINT_ITEMLIST** — server size -1; client size 0; clif_cashshop_show. active sender/reference plus known wire layout.
- **0x0289 · legacy numeric opcode** — server size 12; client size 0; clif_cashshop_ack. active sender/reference plus known wire layout.
- **0x028E · HEADER_HC_ACK_IS_VALID_CHARNAME** — server size 4; client size 0; chclif_reqrename_response. active sender/reference plus known wire layout.
- **0x0290 · HEADER_HC_ACK_CHANGE_CHARNAME** — server size 4; client size 0; chclif_rename_response. active sender/reference plus known wire layout.
- **0x0291 · HEADER_ZC_MSG** — server size 4; client size 0; clif_msg. active sender/reference plus known wire layout.
- **0x0293 · HEADER_ZC_BOSS_INFO** — server size 70; client size 0; clif_bossmapinfo. active sender/reference plus known wire layout.
- **0x0294 · legacy numeric opcode** — server size 10; client size 0; clif_readbook. active sender/reference plus known wire layout.
- **0x0298 · HEADER_ZC_CASH_TIME_COUNTER** — server size 8; client size 0; clif_rental_time. active sender/reference plus known wire layout.
- **0x0299 · HEADER_ZC_CASH_ITEM_DELETE** — server size 6; client size 0; clif_rental_expired. active sender/reference plus known wire layout.
- **0x029B · legacy numeric opcode** — server size 80; client size 0; clif_mercenary_info. active sender/reference plus known wire layout.
- **0x02A2 · legacy numeric opcode** — server size 8; client size 0; clif_mercenary_updatestatus. active sender/reference plus known wire layout.
- **0x02B3 · questAddType** — server size 107; client size 0; clif_quest_add. active sender/reference plus known wire layout.
- **0x02B4 · legacy numeric opcode** — server size 6; client size 0; clif_quest_delete. active sender/reference plus known wire layout.
- **0x02B5 · questUpdateType** — server size -1; client size 0; clif_quest_update_objective. active sender/reference plus known wire layout.
- **0x02B7 · legacy numeric opcode** — server size 7; client size 0; clif_quest_update_status. active sender/reference plus known wire layout.
- **0x02B8 · HEADER_ZC_ITEM_PICKUP_PARTY** — server size 22; client size 0; clif_party_show_picker. active sender/reference plus known wire layout.
- **0x02BB · HEADER_ZC_EQUIPITEM_DAMAGED** — server size 8; client size 0; clif_item_damaged. active sender/reference plus known wire layout.
- **0x02C1 · legacy numeric opcode** — server size -1; client size 0; clif_channel_msg, clif_messagecolor_target. active sender/reference plus known wire layout.
- **0x02CB · legacy numeric opcode** — server size 65; client size 0; clif_instance_create. active sender/reference plus known wire layout.
- **0x02CC · legacy numeric opcode** — server size 4; client size 0; clif_instance_changewait. active sender/reference plus known wire layout.
- **0x02CD · legacy numeric opcode** — server size 71; client size 0; clif_instance_status. active sender/reference plus known wire layout.
- **0x02CE · legacy numeric opcode** — server size 10; client size 0; clif_instance_changestatus. active sender/reference plus known wire layout.
- **0x02DE · legacy numeric opcode** — server size 6; client size 0; clif_bg_updatescore, clif_bg_updatescore_single. active sender/reference plus known wire layout.
- **0x02DF · legacy numeric opcode** — server size 36; client size 0; clif_bg_xy, clif_bg_xy_remove. active sender/reference plus known wire layout.
- **0x02E0 · HEADER_ZC_BATTLEFIELD_NOTIFY_HP** — server size 34; client size 0; clif_bg_hp. active sender/reference plus known wire layout.
- **0x02F0 · legacy numeric opcode** — server size 10; client size 0; clif_progressbar. active sender/reference plus known wire layout.
- **0x02F2 · legacy numeric opcode** — server size 2; client size 0; clif_progressbar_abort. active sender/reference plus known wire layout.
- **0x0440 · HEADER_ZC_MILLENNIUMSHIELD** — server size 10; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x0441 · HEADER_ZC_SKILLINFO_DELETE** — server size 4; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x0442 · HEADER_ZC_SKILL_SELECT_REQUEST** — server size -1; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x07DB · HEADER_ZC_HO_PAR_CHANGE** — server size 8; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x07E1 · HEADER_ZC_SKILLINFO_UPDATE2** — server size 15; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x07E2 · HEADER_ZC_MSG_VALUE** — server size 8; client size 0; clif_msg_value. active sender/reference plus known wire layout.
- **0x07E6 · HEADER_ZC_MSG_SKILL** — server size 8; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x07FC · partyleaderchangedType** — server size None; client size 0; clif_party_leaderchanged. sender/reference exists; wire/reachability needs review.
- **0x07FD · item_drop_announceType, package_item_announceType** — server size None; client size 0; clif_broadcast_obtain_special_item. sender/reference exists; wire/reachability needs review.
- **0x0803 · legacy numeric opcode** — server size 4; client size 0; clif_PartyBookingRegisterAck. active sender/reference plus known wire layout.
- **0x0805 · legacy numeric opcode** — server size -1; client size 0; clif_PartyBookingSearchAck. active sender/reference plus known wire layout.
- **0x0807 · legacy numeric opcode** — server size None; client size 0; clif_PartyBookingDeleteAck. sender/reference exists; wire/reachability needs review.
- **0x0809 · legacy numeric opcode** — server size None; client size 0; clif_PartyBookingInsertNotify. sender/reference exists; wire/reachability needs review.
- **0x080A · legacy numeric opcode** — server size None; client size 0; clif_PartyBookingUpdateNotify. sender/reference exists; wire/reachability needs review.
- **0x080B · legacy numeric opcode** — server size None; client size 0; clif_PartyBookingDeleteNotify. sender/reference exists; wire/reachability needs review.
- **0x0810 · legacy numeric opcode** — server size None; client size 0; clif_buyingstore_open. sender/reference exists; wire/reachability needs review.
- **0x0812 · legacy numeric opcode** — server size None; client size 0; clif_buyingstore_open_failed. sender/reference exists; wire/reachability needs review.
- **0x0813 · HEADER_ZC_MYITEMLIST_BUYING_STORE** — server size -1; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x0818 · HEADER_ZC_ACK_ITEMLIST_BUYING_STORE** — server size -1; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x081A · legacy numeric opcode** — server size None; client size 0; clif_buyingstore_trade_failed_buyer. sender/reference exists; wire/reachability needs review.
- **0x081B · buyingStoreUpdateItemType** — server size None; client size 0; clif_buyingstore_update_item. sender/reference exists; wire/reachability needs review.
- **0x081C · legacy numeric opcode** — server size None; client size 0; clif_buyingstore_delete_item. sender/reference exists; wire/reachability needs review.
- **0x081D · legacy numeric opcode** — server size None; client size 0; clif_elemental_info. sender/reference exists; wire/reachability needs review.
- **0x081E · HEADER_ZC_EL_PAR_CHANGE** — server size 8; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x0824 · HEADER_ZC_FAILED_TRADE_BUYING_STORE_TO_SELLER** — server size 6; client size 0; clif_buyingstore_trade_failed_seller. active sender/reference plus known wire layout.
- **0x0828 · HEADER_HC_DELETE_CHAR3_RESERVED** — server size 14; client size 0; chclif_char_delete2_ack. active sender/reference plus known wire layout.
- **0x082A · HEADER_HC_DELETE_CHAR3** — server size 10; client size 0; chclif_char_delete2_accept_ack. active sender/reference plus known wire layout.
- **0x082C · HEADER_HC_DELETE_CHAR3_CANCEL** — server size 10; client size 0; chclif_char_delete2_cancel_ack. active sender/reference plus known wire layout.
- **0x082D · HEADER_HC_ACCEPT_ENTER2** — server size 29; client size 0; chclif_mmo_send082d. active sender/reference plus known wire layout.
- **0x0837 · HEADER_ZC_SEARCH_STORE_INFO_FAILED** — server size 3; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x083A · HEADER_ZC_OPEN_SEARCH_STORE_INFO** — server size 4; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x083D · HEADER_ZC_SSILIST_ITEM_CLICK_ACK** — server size 6; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x0840 · HEADER_HC_NOTIFY_ACCESSIBLE_MAPNAME** — server size -1; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x08B3 · legacy numeric opcode** — server size -1; client size 0; clif_showscript. active sender/reference plus known wire layout.
- **0x08B9 · HEADER_HC_SECOND_PASSWD_LOGIN** — server size 12; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x08C0 · HEADER_ZC_ACK_SE_CASH_ITEM_LIST2** — server size -1; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x08CA · HEADER_ZC_ACK_SCHEDULER_CASHITEM** — server size -1; client size 0; clif_cashshop_list. active sender/reference plus known wire layout.
- **0x08CF · HEADER_ZC_SPIRITS_ATTRIBUTE** — server size 10; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x08D2 · legacy numeric opcode** — server size None; client size 0; clif_snap. sender/reference exists; wire/reachability needs review.
- **0x08D5 · HEADER_HC_ACK_CHANGE_CHARACTER_SLOT** — server size 8; client size 0; chclif_moveCharSlotReply. active sender/reference plus known wire layout.
- **0x08D6 · HEADER_ZC_CLEAR_DIALOG, script_clearType** — server size 6; client size 0; clif_scriptclear. active sender/reference plus known wire layout.
- **0x08D8 · bgqueue_ackType** — server size None; client size 0; clif_bg_queue_apply_result. sender/reference exists; wire/reachability needs review.
- **0x08D9 · bgqueue_notify_entryType, bgqueue_updateinfoType** — server size None; client size 0; clif_bg_queue_apply_notify. sender/reference exists; wire/reachability needs review.
- **0x08DB · bgqueue_notice_deleteType** — server size None; client size 0; clif_bg_queue_cancel_result. sender/reference exists; wire/reachability needs review.
- **0x08DF · bgqueue_battlebeginsType** — server size None; client size 0; clif_bg_queue_lobby_notify. sender/reference exists; wire/reachability needs review.
- **0x08E1 · legacy numeric opcode** — server size None; client size 0; clif_bg_queue_ack_lobby. sender/reference exists; wire/reachability needs review.
- **0x08FF · HEADER_ZC_EFST_SET_ENTER** — server size 24; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x0908 · HEADER_ZC_INVENTORY_TAB** — server size 5; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x090E · HEADER_ZC_ENTRY_QUEUE_INIT** — server size 2; client size 0; clif_bg_queue_entry_init. active sender/reference plus known wire layout.
- **0x0979 · legacy numeric opcode** — server size None; client size 0; clif_ackworldinfo. sender/reference exists; wire/reachability needs review.
- **0x097D · HEADER_ZC_ACK_RANKING** — server size 288; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x097E · HEADER_ZC_UPDATE_RANKING_POINT** — server size 12; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x0988 · HEADER_ZC_NOTIFY_CLAN_CONNECTINFO** — server size 6; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x0989 · HEADER_ZC_ACK_CLAN_LEAVE** — server size 2; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x098E · HEADER_ZC_NOTIFY_CLAN_CHAT** — server size -1; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x099D · HEADER_HC_ACK_CHARINFO_PER_PAGE** — server size -1; client size 0; chclif_mmo_send099d. active sender/reference plus known wire layout.
- **0x09A0 · HEADER_HC_CHARLIST_NOTIFY** — server size 6; client size 0; chclif_charlist_notify. active sender/reference plus known wire layout.
- **0x09A6 · HEADER_ZC_BANKING_CHECK, banking_checkType** — server size 12; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x09A8 · HEADER_ZC_ACK_BANKING_DEPOSIT, banking_deposit_ackType** — server size 16; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x09AA · HEADER_ZC_ACK_BANKING_WITHDRAW, banking_withdraw_ackType** — server size 16; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x09AD · HEADER_ZC_ACK_CASH_BARGAIN_SALE_ITEM_INFO** — server size 10; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x09B2 · HEADER_ZC_NOTIFY_BARGAIN_SALE_SELLING** — server size 8; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x09B3 · HEADER_ZC_NOTIFY_BARGAIN_SALE_CLOSE** — server size 4; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x09B7 · HEADER_ZC_ACK_OPEN_BANKING** — server size 4; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x09B9 · HEADER_ZC_ACK_CLOSE_BANKING** — server size 4; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x09C4 · HEADER_ZC_ACK_COUNT_BARGAIN_SALE_ITEM** — server size 8; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x09CA · legacy numeric opcode** — server size None; client size 0; clif_skill_unit_test. sender/reference exists; wire/reachability needs review.
- **0x09CD · HEADER_ZC_MSG_COLOR** — server size 8; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x09DA · HEADER_ZC_ACK_GUILDSTORAGE_LOG** — server size -1; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x0A02 · HEADER_ZC_DRESSROOM_OPEN** — server size 4; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x0A1A · legacy numeric opcode** — server size None; client size 0; clif_roulette_open. sender/reference exists; wire/reachability needs review.
- **0x0A1C · rouletteinfoackType** — server size None; client size 0; clif_roulette_info. sender/reference exists; wire/reachability needs review.
- **0x0A20 · roulettgenerateackType** — server size None; client size 0; clif_roulette_generate. sender/reference exists; wire/reachability needs review.
- **0x0A23 · legacy numeric opcode** — server size None; client size 0; clif_achievement_list_all. sender/reference exists; wire/reachability needs review.
- **0x0A24 · legacy numeric opcode** — server size None; client size 0; clif_achievement_update. sender/reference exists; wire/reachability needs review.
- **0x0A26 · legacy numeric opcode** — server size None; client size 0; clif_achievement_reward_ack. sender/reference exists; wire/reachability needs review.
- **0x0A28 · HEADER_ZC_ACK_OPENSTORE2** — server size 3; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x0A47 · HEADER_ZC_STYLE_CHANGE_RES** — server size 3; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x0AB2 · HEADER_ZC_GROUP_ISALIVE** — server size 7; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x0AE7 · HEADER_ZC_PARTY_REQ_MASTER_TO_JOIN** — server size 38; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x0AF0 · legacy numeric opcode** — server size None; client size 0; clif_attendence_response. sender/reference exists; wire/reachability needs review.
- **0x0AF6 · HEADER_ZC_ACK_RANKING2** — server size 88; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x0AFA · HEADER_ZC_PARTY_JOIN_REQ_ACK_FROM_MASTER** — server size 58; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x0AFD · HEADER_ZC_GUILD_POSITION** — server size -1; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x0B0D · HEADER_ZC_REMOVE_EFFECT** — server size 10; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x0B68 · HEADER_ZC_TARGET_SPIRITS** — server size 12; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x0B6B · HEADER_ZC_SUMMON_HP_INIT** — server size 14; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x0B6C · HEADER_ZC_SUMMON_HP_UPDATE** — server size 12; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x0B8D · HEADER_ZC_REPUTE_INFO** — server size -1; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x0B9A · HEADER_ZC_UI_OPEN_V3** — server size 11; client size 0; no sender established by scan. declared/registered; runtime emission not established.
- **0x0C0C · HEADER_ZC_GM_CHECKER** — server size 4; client size 0; no sender established by scan. declared/registered; runtime emission not established.

## Complete accepted-request inventory

This records every active map-server request registration. “Evidence found” points to a constructor/profile use in inventory.json; it is not a semantics certification. “Not established” is a review queue, not an automatic declaration that the request is absent. A legacy opcode may be an alias of an implemented request under another profile.

- **0x0072 · clif_parse_UseSkillToId** — 25 bytes; positions `6,10,21,0`; outbound path not established by scan.
- **0x007D · clif_parse_LoadEndAck** — 2 bytes; positions `0,0`; constructor/profile reference found.
- **0x007E · clif_parse_UseSkillToPosMoreInfo** — 102 bytes; positions `5,9,12,20,22,0`; outbound path not established by scan.
- **0x0085 · clif_parse_ChangeDir** — 11 bytes; positions `7,10,0`; constructor/profile reference found.
- **0x0089 · clif_parse_TickSend** — 8 bytes; positions `4,0`; constructor/profile reference found.
- **0x008C · clif_parse_GetCharNameRequest** — 11 bytes; positions `7,0`; constructor/profile reference found.
- **0x0090 · clif_parse_NpcClicked** — 7 bytes; positions `0,0`; outbound path not established by scan.
- **0x0094 · clif_parse_MoveToKafra** — 14 bytes; positions `7,10,0`; outbound path not established by scan.
- **0x0096 · clif_parse_WisMessage** — -1 bytes; positions `2,4,28,0`; constructor/profile reference found.
- **0x0099 · clif_parse_Broadcast** — -1 bytes; positions `0,0`; outbound path not established by scan.
- **0x009B · clif_parse_WantToConnection** — 26 bytes; positions `4,9,17,21,25,0`; outbound path not established by scan.
- **0x009F · clif_parse_UseItem** — 14 bytes; positions `4,10,0`; outbound path not established by scan.
- **0x00A2 · clif_parse_SolveCharName** — 15 bytes; positions `11,0`; outbound path not established by scan.
- **0x00A7 · clif_parse_WalkToXY** — 8 bytes; positions `5,0`; constructor/profile reference found.
- **0x00A9 · clif_parse_EquipItem** — 6 bytes; positions `0,0`; constructor/profile reference found.
- **0x00AB · clif_parse_UnequipItem** — 4 bytes; positions `2,0`; constructor/profile reference found.
- **0x00B2 · clif_parse_Restart** — 3 bytes; positions `2,0`; outbound path not established by scan.
- **0x00B8 · clif_parse_NpcSelectMenu** — 7 bytes; positions `2,6,0`; outbound path not established by scan.
- **0x00B9 · clif_parse_NpcNextClicked** — 6 bytes; positions `2,0`; outbound path not established by scan.
- **0x00BB · clif_parse_StatusUp** — 5 bytes; positions `2,4,0`; outbound path not established by scan.
- **0x00BF · clif_parse_Emotion** — 3 bytes; positions `0,0`; constructor/profile reference found.
- **0x00C1 · clif_parse_HowManyConnections** — 2 bytes; positions `0,0`; outbound path not established by scan.
- **0x00C5 · clif_parse_NpcBuySellSelected** — 7 bytes; positions `0,0`; outbound path not established by scan.
- **0x00C8 · clif_parse_NpcBuyListSend** — -1 bytes; positions `2,4,0`; outbound path not established by scan.
- **0x00C9 · clif_parse_NpcSellListSend** — -1 bytes; positions `2,4,0`; outbound path not established by scan.
- **0x00CC · clif_parse_GMKick** — 6 bytes; positions `2,0`; outbound path not established by scan.
- **0x00CE · clif_parse_GMKickAll** — 2 bytes; positions `0,0`; outbound path not established by scan.
- **0x00CF · clif_parse_PMIgnore** — 27 bytes; positions `2,26,0`; constructor/profile reference found.
- **0x00D0 · clif_parse_PMIgnoreAll** — 3 bytes; positions `0,0`; outbound path not established by scan.
- **0x00D3 · clif_parse_PMIgnoreList** — 2 bytes; positions `0,0`; outbound path not established by scan.
- **0x00D5 · clif_parse_CreateChatRoom** — -1 bytes; positions `0,0`; outbound path not established by scan.
- **0x00D9 · clif_parse_ChatAddMember** — 14 bytes; positions `0,0`; outbound path not established by scan.
- **0x00DE · clif_parse_ChatRoomStatusChange** — -1 bytes; positions `0,0`; outbound path not established by scan.
- **0x00E0 · clif_parse_ChangeChatOwner** — 30 bytes; positions `2,6,0`; outbound path not established by scan.
- **0x00E2 · clif_parse_KickFromChat** — 26 bytes; positions `2,0`; outbound path not established by scan.
- **0x00E3 · clif_parse_ChatLeave** — 2 bytes; positions `0,0`; outbound path not established by scan.
- **0x00E4 · clif_parse_TradeRequest** — 6 bytes; positions `2,0`; constructor/profile reference found.
- **0x00E6 · clif_parse_TradeAck** — 3 bytes; positions `2,0`; constructor/profile reference found.
- **0x00E8 · clif_parse_TradeAddItem** — 8 bytes; positions `0,0`; constructor/profile reference found.
- **0x00EB · clif_parse_TradeOk** — 2 bytes; positions `0,0`; constructor/profile reference found.
- **0x00ED · clif_parse_TradeCancel** — 2 bytes; positions `0,0`; constructor/profile reference found.
- **0x00EF · clif_parse_TradeCommit** — 2 bytes; positions `0,0`; constructor/profile reference found.
- **0x00F3 · clif_parse_GlobalMessage** — -1 bytes; positions `2,4,0`; constructor/profile reference found.
- **0x00F5 · clif_parse_TakeItem** — 8 bytes; positions `4,0`; constructor/profile reference found.
- **0x00F7 · clif_parse_MoveFromKafra** — 22 bytes; positions `14,18,0`; outbound path not established by scan.
- **0x00F9 · clif_parse_CreateParty** — 26 bytes; positions `0,0`; outbound path not established by scan.
- **0x00FC · clif_parse_PartyInvite** — 6 bytes; positions `0,0`; outbound path not established by scan.
- **0x00FF · clif_parse_ReplyPartyInvite** — 10 bytes; positions `0,0`; outbound path not established by scan.
- **0x0100 · clif_parse_LeaveParty** — 2 bytes; positions `0,0`; outbound path not established by scan.
- **0x0102 · clif_parse_PartyChangeOption** — 6 bytes; positions `2,0`; outbound path not established by scan.
- **0x0103 · clif_parse_RemovePartyMember** — 30 bytes; positions `0,0`; outbound path not established by scan.
- **0x0108 · clif_parse_PartyMessage** — -1 bytes; positions `2,4,0`; outbound path not established by scan.
- **0x0112 · clif_parse_SkillUp** — 4 bytes; positions `2,0`; constructor/profile reference found.
- **0x0113 · clif_parse_UseSkillToPos** — 22 bytes; positions `5,9,12,20,0`; constructor/profile reference found.
- **0x0116 · clif_parse_DropItem** — 10 bytes; positions `5,8,0`; constructor/profile reference found.
- **0x0118 · clif_parse_StopAttack** — 2 bytes; positions `0,0`; outbound path not established by scan.
- **0x011B · clif_parse_UseSkillMap** — 20 bytes; positions `0,0`; constructor/profile reference found.
- **0x011D · clif_parse_RequestMemo** — 2 bytes; positions `0,0`; outbound path not established by scan.
- **0x0126 · clif_parse_PutItemToCart** — 8 bytes; positions `0,0`; outbound path not established by scan.
- **0x0127 · clif_parse_GetItemFromCart** — 8 bytes; positions `0,0`; outbound path not established by scan.
- **0x0128 · clif_parse_MoveFromKafraToCart** — 8 bytes; positions `0,0`; outbound path not established by scan.
- **0x0129 · clif_parse_MoveToKafraFromCart** — 8 bytes; positions `0,0`; outbound path not established by scan.
- **0x012A · clif_parse_RemoveOption** — 2 bytes; positions `0,0`; constructor/profile reference found.
- **0x012E · clif_parse_CloseVending** — 2 bytes; positions `0,0`; constructor/profile reference found.
- **0x012F · clif_parse_OpenVending** — -1 bytes; positions `2,4,0,84,0`; outbound path not established by scan.
- **0x0130 · clif_parse_VendingListReq** — 6 bytes; positions `2,0`; constructor/profile reference found.
- **0x0134 · clif_parse_PurchaseReq** — -1 bytes; positions `0,0`; constructor/profile reference found.
- **0x013F · clif_parse_GM_Item_Monster** — 26 bytes; positions `2,0`; outbound path not established by scan.
- **0x0140 · clif_parse_MapMove** — 22 bytes; positions `0,0`; outbound path not established by scan.
- **0x0143 · clif_parse_NpcAmountInput** — 10 bytes; positions `0,0`; outbound path not established by scan.
- **0x0146 · clif_parse_NpcCloseClicked** — 6 bytes; positions `0,0`; outbound path not established by scan.
- **0x0149 · clif_parse_GMReqNoChat** — 9 bytes; positions `2,6,7,0`; outbound path not established by scan.
- **0x014D · clif_parse_GuildCheckMaster** — 2 bytes; positions `0,0`; outbound path not established by scan.
- **0x014F · clif_parse_GuildRequestInfo** — 6 bytes; positions `2,0`; constructor/profile reference found.
- **0x0151 · clif_parse_GuildRequestEmblem** — 6 bytes; positions `2,0`; constructor/profile reference found.
- **0x0153 · clif_parse_GuildChangeEmblem** — -1 bytes; positions `2,4,0`; outbound path not established by scan.
- **0x0155 · clif_parse_GuildChangeMemberPosition** — -1 bytes; positions `0,0`; constructor/profile reference found.
- **0x0159 · clif_parse_GuildLeave** — 54 bytes; positions `0,0`; constructor/profile reference found.
- **0x015B · clif_parse_GuildExpulsion** — 54 bytes; positions `0,0`; outbound path not established by scan.
- **0x015D · clif_parse_GuildBreak** — 42 bytes; positions `0,0`; outbound path not established by scan.
- **0x0161 · clif_parse_GuildChangePositionInfo** — -1 bytes; positions `2,4,0`; constructor/profile reference found.
- **0x0165 · clif_parse_CreateGuild** — 30 bytes; positions `2,6,0`; outbound path not established by scan.
- **0x0168 · clif_parse_GuildInvite** — 14 bytes; positions `0,0`; constructor/profile reference found.
- **0x016B · clif_parse_GuildReplyInvite** — 10 bytes; positions `0,0`; constructor/profile reference found.
- **0x016E · clif_parse_GuildChangeNotice** — 186 bytes; positions `2,6,66,0`; constructor/profile reference found.
- **0x0170 · clif_parse_GuildRequestAlliance** — 14 bytes; positions `2,6,10,0`; outbound path not established by scan.
- **0x0172 · clif_parse_GuildReplyAlliance** — 10 bytes; positions `2,6,0`; constructor/profile reference found.
- **0x0178 · clif_parse_ItemIdentify** — 4 bytes; positions `0,0`; constructor/profile reference found.
- **0x017A · clif_parse_UseCard** — 4 bytes; positions `0,0`; constructor/profile reference found.
- **0x017C · clif_parse_InsertCard** — 6 bytes; positions `0,0`; constructor/profile reference found.
- **0x017E · clif_parse_GuildMessage** — -1 bytes; positions `2,4,0`; outbound path not established by scan.
- **0x0180 · clif_parse_GuildOpposition** — 6 bytes; positions `2,0`; outbound path not established by scan.
- **0x0183 · clif_parse_GuildDelAlliance** — 10 bytes; positions `2,6,0`; outbound path not established by scan.
- **0x018A · clif_parse_QuitGame** — 4 bytes; positions `2,0`; outbound path not established by scan.
- **0x018E · clif_parse_ProduceMix** — 10 bytes; positions `0,0`; outbound path not established by scan.
- **0x0190 · clif_parse_ActionRequest** — 19 bytes; positions `5,18,0`; constructor/profile reference found.
- **0x0193 · clif_parse_CloseKafra** — 2 bytes; positions `0,0`; outbound path not established by scan.
- **0x0197 · clif_parse_ResetChar** — 4 bytes; positions `0,0`; outbound path not established by scan.
- **0x0198 · clif_parse_GMChangeMapType** — 8 bytes; positions `2,4,6,0`; outbound path not established by scan.
- **0x019C · clif_parse_LocalBroadcast** — -1 bytes; positions `0,0`; outbound path not established by scan.
- **0x019D · clif_parse_GMHide** — 6 bytes; positions `2,0`; outbound path not established by scan.
- **0x019F · clif_parse_CatchPet** — 6 bytes; positions `2,0`; outbound path not established by scan.
- **0x01A1 · clif_parse_PetMenu** — 3 bytes; positions `2,0`; outbound path not established by scan.
- **0x01A5 · clif_parse_ChangePetName** — 26 bytes; positions `2,0`; outbound path not established by scan.
- **0x01A7 · clif_parse_SelectEgg** — 4 bytes; positions `2,0`; outbound path not established by scan.
- **0x01A9 · clif_parse_SendEmotion** — 6 bytes; positions `2,0`; outbound path not established by scan.
- **0x01AE · clif_parse_SelectArrow** — 4 bytes; positions `0,0`; outbound path not established by scan.
- **0x01AF · clif_parse_ChangeCart** — 4 bytes; positions `2,0`; constructor/profile reference found.
- **0x01B2 · clif_parse_OpenVending** — -1 bytes; positions `2,4,84,85,0`; constructor/profile reference found.
- **0x01BA · clif_parse_GMShift** — 26 bytes; positions `2,0`; outbound path not established by scan.
- **0x01BB · clif_parse_GMShift** — 26 bytes; positions `2,0`; outbound path not established by scan.
- **0x01BC · clif_parse_GMRecall** — 26 bytes; positions `2,0`; outbound path not established by scan.
- **0x01BD · clif_parse_GMRecall** — 26 bytes; positions `2,0`; outbound path not established by scan.
- **0x01CE · clif_parse_AutoSpell** — 6 bytes; positions `0,0`; outbound path not established by scan.
- **0x01D5 · clif_parse_NpcStringInput** — -1 bytes; positions `0,0`; outbound path not established by scan.
- **0x01DF · clif_parse_GMReqAccountName** — 6 bytes; positions `2,0`; outbound path not established by scan.
- **0x01E7 · clif_parse_NoviceDoriDori** — 2 bytes; positions `0,0`; outbound path not established by scan.
- **0x01E8 · clif_parse_CreateParty2** — 28 bytes; positions `0,0`; outbound path not established by scan.
- **0x01ED · clif_parse_NoviceExplosionSpirits** — 2 bytes; positions `0,0`; outbound path not established by scan.
- **0x01F7 · clif_parse_Adopt_reply** — 14 bytes; positions `0,0`; outbound path not established by scan.
- **0x01F9 · clif_parse_Adopt_request** — 6 bytes; positions `2,0`; outbound path not established by scan.
- **0x01FD · clif_parse_RepairItem** — 15 bytes; positions `0,0`; outbound path not established by scan.
- **0x0202 · clif_parse_FriendsListAdd** — 26 bytes; positions `2,0`; constructor/profile reference found.
- **0x0203 · clif_parse_FriendsListRemove** — 10 bytes; positions `2,6,0`; outbound path not established by scan.
- **0x0208 · clif_parse_FriendsListReply** — 14 bytes; positions `2,6,10,0`; constructor/profile reference found.
- **0x020F · clif_parse_PVPInfo** — 10 bytes; positions `2,6,0`; outbound path not established by scan.
- **0x0212 · clif_parse_GMRc** — 26 bytes; positions `2,0`; outbound path not established by scan.
- **0x0213 · clif_parse_Check** — 26 bytes; positions `2,0`; outbound path not established by scan.
- **0x0217 · clif_parse_ranklist_blacksmith** — 2 bytes; positions `0,0`; outbound path not established by scan.
- **0x0218 · clif_parse_ranklist_alchemist** — 2 bytes; positions `0,0`; outbound path not established by scan.
- **0x021D · clif_parse_LessEffect** — 6 bytes; positions `0,0`; outbound path not established by scan.
- **0x0222 · clif_parse_WeaponRefine** — 6 bytes; positions `2,0`; outbound path not established by scan.
- **0x0225 · clif_parse_ranklist_taekwon** — 2 bytes; positions `0,0`; outbound path not established by scan.
- **0x022D · clif_parse_HomMenu** — 5 bytes; positions `2,4,0`; outbound path not established by scan.
- **0x0231 · clif_parse_ChangeHomunculusName** — 26 bytes; positions `2,0`; outbound path not established by scan.
- **0x0232 · clif_parse_HomMoveTo** — 9 bytes; positions `0,0`; outbound path not established by scan.
- **0x0233 · clif_parse_HomAttack** — 11 bytes; positions `2,6,10,0`; outbound path not established by scan.
- **0x0234 · clif_parse_HomMoveToMaster** — 6 bytes; positions `2,0`; outbound path not established by scan.
- **0x0237 · clif_parse_ranklist_killer** — 2 bytes; positions `0,0`; outbound path not established by scan.
- **0x023B · clif_parse_StoragePassword** — 36 bytes; positions `2,4,20,0`; outbound path not established by scan.
- **0x023F · clif_parse_Mail_refreshinbox** — 2 bytes; positions `0,0`; outbound path not established by scan.
- **0x0241 · clif_parse_Mail_read** — 6 bytes; positions `2,0`; outbound path not established by scan.
- **0x0243 · clif_parse_Mail_delete** — 6 bytes; positions `2,0`; outbound path not established by scan.
- **0x0244 · clif_parse_Mail_getattach** — 6 bytes; positions `2,0`; outbound path not established by scan.
- **0x0246 · clif_parse_Mail_winopen** — 4 bytes; positions `2,0`; constructor/profile reference found.
- **0x0247 · clif_parse_Mail_setattach** — 8 bytes; positions `2,4,0`; outbound path not established by scan.
- **0x0248 · clif_parse_Mail_send** — -1 bytes; positions `2,4,28,68,69,0`; constructor/profile reference found.
- **0x024B · clif_parse_Auction_cancelreg** — 4 bytes; positions `2,0`; outbound path not established by scan.
- **0x024C · clif_parse_Auction_setitem** — 8 bytes; positions `2,4,0`; outbound path not established by scan.
- **0x024D · clif_parse_Auction_register** — 12 bytes; positions `0,0`; outbound path not established by scan.
- **0x024E · clif_parse_Auction_cancel** — 6 bytes; positions `2,0`; outbound path not established by scan.
- **0x024F · clif_parse_Auction_bid** — 10 bytes; positions `0,0`; outbound path not established by scan.
- **0x0251 · clif_parse_Auction_search** — 34 bytes; positions `0,0`; outbound path not established by scan.
- **0x0254 · clif_parse_FeelSaveOk** — 3 bytes; positions `2,0`; outbound path not established by scan.
- **0x025B · clif_parse_Cooking** — 6 bytes; positions `0,0`; outbound path not established by scan.
- **0x025C · clif_parse_Auction_buysell** — 4 bytes; positions `2,0`; outbound path not established by scan.
- **0x025D · clif_parse_Auction_close** — 6 bytes; positions `2,0`; outbound path not established by scan.
- **0x0273 · clif_parse_Mail_return** — 30 bytes; positions `2,6,0`; outbound path not established by scan.
- **0x0288 · clif_parse_npccashshop_buy** — 10 bytes; positions `2,4,6,0`; outbound path not established by scan.
- **0x0292 · clif_parse_AutoRevive** — 2 bytes; positions `0,0`; outbound path not established by scan.
- **0x029F · clif_parse_mercenary_action** — 3 bytes; positions `2,0`; outbound path not established by scan.
- **0x02B6 · clif_parse_questStateAck** — 7 bytes; positions `0,0`; outbound path not established by scan.
- **0x02BA · clif_parse_Hotkey** — 11 bytes; positions `2,4,5,9,0`; outbound path not established by scan.
- **0x02C4 · clif_parse_PartyInvite2** — 26 bytes; positions `0,0`; outbound path not established by scan.
- **0x02C7 · clif_parse_ReplyPartyInvite2** — 7 bytes; positions `0,0`; outbound path not established by scan.
- **0x02C8 · clif_parse_PartyTick** — 3 bytes; positions `0,0`; outbound path not established by scan.
- **0x02CF · clif_parse_MemorialDungeonCommand** — 6 bytes; positions `2,0`; outbound path not established by scan.
- **0x02D6 · clif_parse_ViewPlayerEquip** — 6 bytes; positions `2,0`; constructor/profile reference found.
- **0x02D8 · clif_parse_configuration** — 10 bytes; positions `2,6,0`; outbound path not established by scan.
- **0x02DB · clif_parse_BattleChat** — -1 bytes; positions `2,4,0`; outbound path not established by scan.
- **0x02F1 · clif_parse_progressbar** — 2 bytes; positions `0,0`; outbound path not established by scan.
- **0x0436 · clif_parse_WantToConnection** — 19 bytes; positions `2,6,10,14,18,0`; outbound path not established by scan.
- **0x0437 · clif_parse_ActionRequest** — 7 bytes; positions `2,6,0`; constructor/profile reference found.
- **0x0438 · clif_parse_UseSkillToId** — 10 bytes; positions `2,4,6,0`; constructor/profile reference found.
- **0x0439 · clif_parse_UseItem** — 8 bytes; positions `2,4,0`; constructor/profile reference found.
- **0x0802 · clif_parse_PartyBookingRegisterReq** — 18 bytes; positions `2,4,6,0`; outbound path not established by scan.
- **0x0806 · clif_parse_PartyBookingDeleteReq** — 4 bytes; positions `2,0`; outbound path not established by scan.

## Remaining registered-handler review queue

Every registered opcode is retained in inventory.csv/json with its handler and source line. Their existence is only a dispatch-coverage fact. Handler/helper semantics, UI state transitions, lifecycle/removal, error paths, ordering, reconnect and actor bootstrap still require per-family review or wire replay. Examples already confirmed partial are storage passwords, show-EFST, guild permissions, group chat, mail and record-array validation above.

## Suggested order

1. Correct handshake/framing defects and outbound layout mismatches; replay concatenated and fragmented packet streams.
2. Implement currently discarded map, party, NPC, configuration and disconnect state notifications.
3. Complete guild permissions/notifications and group chat, storage passwords, NPC progress/crafting/repair responses, and quest state.
4. Finish pet/homunculus/mercenary, chat rooms, inbox/attachments, rental/binding, battleground/instance, cash-shop and auction packet families as actual features.
5. Bring later compatibility families into explicitly selected version profiles; avoid one global mixture of unrelated wire formats.

## Reproduction and files

- `python tools/audit_packets.py` regenerates the compiled/static inventory.
- `python tools/render_packet_audit.py` reapplies the reviewed findings and generates this report and the canvas source.
- `inventory.json` retains sender and constructor evidence, source hashes, uncertainty and final handler registration.
- `inventory.csv` is the filterable full inventory; `findings.json` is the reviewed implementation backlog.
- `historical-catalog.csv` catalogs all numeric definitions found across server versions; inactive entries are not counted as missing 2008 features.
- Intermediate probes and preprocessed server files are in the ignored build directory `build-mingw-preset/packet-audit/`.
