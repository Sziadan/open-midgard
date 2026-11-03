# Bounded packet work packages

Read ../PACKET_IMPLEMENTATION_PLAN.md first. Every numbered step below requires its own commit and progress update. All steps start NOT_STARTED; generated cards are specifications, not current status. IDs are stable; appended PK50 is scheduled by dependencies, not numeric order.

Common source anchors for every card: rAthena src/map/clif_packetdb.hpp, packets.hpp, packets_struct.hpp, clif.cpp and the parser/emitter call tree; Ref/GameModePacket.cpp, packet.h and corresponding Session/actor/UI methods. Exact matching audit symbols/opcodes are listed in coverage.csv and inventory.json. Before implementation, write exact symbol/guard/field evidence into a contract. If a card is too broad, enumerate permanent child IDs before coding; each child gets a separate commit.

## PK00 — Reconcile and preserve the implementation baseline

Tier: **S**. Dependencies: none.

### Bounded steps

- **PK00.01 [S]** Define baseline ownership boundaries and safe snapshot strategy from git status, HEAD and all tracked/untracked runtime dependencies; no packet contract or runtime test is needed for this research step. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK00.02 [S]** Record HEAD and every tracked/untracked runtime dependency without changing them. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK00.03 [S]** Agree an owned baseline snapshot or existing commits and record exact reproduction instructions. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK00.04 [S]** Verify baseline artifacts reproduce the intended runtime sources and have no missing untracked dependencies; commit baseline provenance and resumable handoff. Record any build gate honestly. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

Do not stage all current work. Planning commit alone does not capture runtime dependencies. If ownership is unclear block runtime work, not source research.

### Discovery ownership

0 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

## PK01 — Resolve active protocol identities

Tier: **S**. Dependencies: PK00.

### Bounded steps

- **PK01.01 [S]** Approve this package's bounded research/harness/review specification, exact source inputs, output schema and evidence gate. For runtime contracts encountered, require independent strong approval before implementation. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK01.02 [S]** Reproduce effective server defines and final registration order. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK01.03 [S]** Establish canonical stage/direction/profile/layout identity rules and approve the initial P0 packet applicability contracts. Allocate per-row reachability research leaves to feature owners and PK47; do not require all historical classification before framing work. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK01.04 [S]** Independently verify configuration probes, override order, initial P0 identities and full ownership coverage; commit the source-research checkpoint. Runtime feature tests are not a prerequisite to this infrastructure step. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

Profiles22/23 currently share a table: determine their supported contract, do not assume equality is intended. Record server revision/hash and unresolved deployed-binary provenance.

### Discovery ownership

0 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

## PK02 — Create server contract manifest and byte oracles

Tier: **S**. Dependencies: PK01.

### Bounded steps

- **PK02.01 [S]** Approve this package's bounded research/harness/review specification, exact source inputs, output schema and evidence gate. For runtime contracts encountered, require independent strong approval before implementation. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK02.02 [S]** Build sizeof/offsetof and registry probes using actual server guards. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK02.03 [S]** Create independently derived raw byte fixtures and schema validation for the initial P0 contracts; define how feature owners add their own source-backed oracles incrementally. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK02.04 [S]** Independently compare initial oracle offsets and sizes with effective server source, check manifest reproducibility, and commit the infrastructure verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

Expected request offsets must come from final server parser registration. No client struct as test oracle.

### Discovery ownership

0 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

## PK03 — Production replay and test harness

Tier: **S**. Dependencies: PK02.

### Bounded steps

- **PK03.01 [S]** Approve this package's bounded research/harness/review specification, exact source inputs, output schema and evidence gate. For runtime contracts encountered, require independent strong approval before implementation. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK03.02 [S]** Exercise actual Connection framing and router with fragmented concatenated streams. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK03.03 [S]** Exercise outbound builders and session/Qt transitions with local mock peers. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK03.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

Document confirmed MinGW test commands. Detect stale binaries, use deployment OFF, no live installed client. Add regression ratchets without treating registered handlers as implemented.

### Discovery ownership

0 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

## PK04 — Raw account-ID handshake

Tier: **S**. Dependencies: PK03.

### Bounded steps

- **PK04.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK04.02 [S]** Model character connection raw-prefix state and remove fake opcode assumptions. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK04.03 [S]** Validate fragmented account echo before allowing framed packets. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK04.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

P01: AIDs whose low words resemble real opcodes, mismatch, disconnect, reconnect and every split of four bytes. Follow char_clif.cpp send-back-account_id path.

### Discovery ownership

2 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

Finding P01: Account-ID handshake is represented as fake opcodes.

- Source anchor: `D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/network/GronPacket.cpp`:323; search `SetPacketSize(0x8482`. Recheck current line/guard.
- Source anchor: `D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/LoginMode.cpp`:1935; search `case 0x8482`. Recheck current line/guard.
- Source anchor: `C:/Users/sziad/source/repos/rathena-master/src/char/char_clif.cpp`:850; search `// send back account_id`. Recheck current line/guard.

## PK05 — Framing and unknown-opcode behavior

Tier: **S**. Dependencies: PK04.

### Bounded steps

- **PK05.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK05.02 [S]** Replace destructive two-byte skipping with profile-derived bounded framing. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK05.03 [S]** Define explicit protocol-error/connection cleanup and malformed-frame reporting. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK05.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

P02: preserve alignment for valid concatenated packets. Never resynchronize by scanning payload. Known but missing handlers stay open feature tasks. Unknown size cannot safely be guessed.

### Discovery ownership

0 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

Finding P02: Unknown packet recovery can destroy stream alignment.

- Source anchor: `D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/network/Connection.cpp`:533; search `if (knownSize == 0)`. Recheck current line/guard.

## PK06 — Guild member login notice 01F2

Tier: **M**. Dependencies: PK05.

### Bounded steps

- **PK06.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK06.02 [M]** Add exact 20-byte active layout and membership state update. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK06.03 [M]** Propagate online/member status to guild UI and map lifecycle. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK06.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

P03: clif_guild_memberlogin_notice and ZC_UPDATE_CHARSTAT. Newer016D/01EB do not replace it. L candidate: approved fixed-layout decoding only.

### Discovery ownership

1 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

Finding P03: Guild member status is still unframed and unhandled.

- Source anchor: `C:/Users/sziad/source/repos/rathena-master/src/map/clif.cpp`:8682; search `void clif_guild_memberlogin_notice`. Recheck current line/guard.
- Source anchor: `D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/network/GronPacket.cpp`:25; search `void FillPacketSizeTable`. Recheck current line/guard.

## PK07 — Pet 01A2 framing repair

Tier: **M**. Dependencies: PK05.

### Bounded steps

- **PK07.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK07.02 [M]** Select 35-byte layout for active20080910 profile. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK07.03 [M]** Decode all pet status fields and connect state through pet package. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK07.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

P04: extra job word only from20081126. Test next packet remains aligned. This step alone does not certify pet feature completion; PK27 owns lifecycle.

### Discovery ownership

1 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

Finding P04: Pet status length is wrong for the configured server.

- Source anchor: `D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/network/GronPacket.cpp`:260; search `SetPacketSize(0x01A2, 37)`. Recheck current line/guard.
- Source anchor: `C:/Users/sziad/source/repos/rathena-master/src/map/packets_struct.hpp`:5441; search `struct PACKET_ZC_PROPERTY_PET`. Recheck current line/guard.

## PK08 — Ground skill text request contract

Tier: **S**. Dependencies: PK05.

### Bounded steps

- **PK08.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK08.02 [S]** Replace mismatched0190 request with active server MoreInfo contract. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK08.03 [S]** Correct production serializer and independent old test expectations. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK08.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

P05: verify007E 102 bytes offsets5,9,12,20,22 against active parser.0190 is19-byte action request offsets5,18. Do not generalize these values to other profiles.

### Discovery ownership

2 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

Finding P05: Ground-skill text request uses the wrong active contract.

- Source anchor: `D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/network/Packet.h`:444; search `struct PACKET_CZ_USESKILLTOPOSINFO`. Recheck current line/guard.
- Source anchor: `D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/network/MapSendProfile.cpp`:942; search `bool BuildActiveUseSkillToPosInfoPacket`. Recheck current line/guard.
- Source anchor: `C:/Users/sziad/source/repos/rathena-master/src/map/clif_packetdb.hpp`:531; search `parseable_packet(0x007e,102`. Recheck current line/guard.

## PK09 — Inventory/cart receive alias ownership

Tier: **S**. Dependencies: PK05.

### Bounded steps

- **PK09.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK09.02 [S]** Resolve01EF duplicate router registration per profile. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK09.03 [S]** Verify active normal/equipment/cart routes independently. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK09.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

P06: active2008 equipment02D0.01EF compatibility ambiguity must not cause cart clearing. Assert unique final dispatch ownership.

### Discovery ownership

1 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

Finding P06: Duplicate registration hides an advertised inventory handler.

- Source anchor: `D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/network/MapSendProfile.cpp`:215; search `profile.equipInventoryListTimed = 0x01EF`. Recheck current line/guard.
- Source anchor: `D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/GameModePacket.cpp`:8765; search `for(u16 id:{0x0122`. Recheck current line/guard.

## PK10 — Connection transitions and confirmations

Tier: **S**. Dependencies: PK05.

### Bounded steps

- **PK10.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK10.02 [S]** Verify login/char failures and map connection confirmation including0283. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK10.03 [S]** Implement server-directed disconnect02D5 and quit/reconnect state cleanup. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK10.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

P12: verify reachability separately. Validate echoed AID. Do not preserve fake8482/8483 aliases. Every pending modal/action must unwind once.

### Discovery ownership

2 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

Finding P12: Server-directed disconnect is ignored.

- Source anchor: `D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/GameModePacket.cpp`:8842; search `router.Register(0x02D5`. Recheck current line/guard.
- Source anchor: `D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/Ref/GameModePacket.cpp`:2384; search `CGameMode::Zc_Isvr_Disconnect`. Recheck current line/guard.

## PK11 — Guild incremental permissions and menu state

Tier: **M**. Dependencies: PK06.

### Bounded steps

- **PK11.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK11.02 [M]** Reconcile0156/0160/0166/0174 with016C local rank/permissions. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK11.03 [M]** Apply014E menuFlags and current permission gates to Qt controls. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK11.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

P15/P16: downgrades revoke controls immediately, promotions restore only server-granted actions. Do not guess rank-to-permission meanings.

### Discovery ownership

5 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

Finding P15: Guild rank changes do not refresh local authority.

- Source anchor: `D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/GameModePacket.cpp`:5261; search `void HandleGuildWindowPacket`. Recheck current line/guard.
- Source anchor: `D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/qtui/QtCommerce.cpp`:39; search `m_localGuild.permissions&1`. Recheck current line/guard.

Finding P16: Guild menu permissions are stored but not applied.

- Source anchor: `D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/core/GuildState.h`:47; search `if(op==0x14e)`. Recheck current line/guard.
- Source anchor: `D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/qtui/QtUiStateAdapter.cpp`:3820; search `const auto& guild=g_session.m_localGuild;const auto& info=`. Recheck current line/guard.
- Source anchor: `D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/qtui/qml/RoGuild.qml`:50; search `model: ["Overview"`. Recheck current line/guard.

## PK12 — Guild management requests and responses

Tier: **S**. Dependencies: PK11.

### Bounded steps

- **PK12.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK12.02 [S]** Implement expulsion015B and disband015D with result/state paths. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK12.03 [S]** Implement alliance0170 opposition0180 and relation removal0183. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK12.04 [S]** Implement guild-member actions and all rejection/cancel paths in Qt. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK12.05 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

P17: verify guild IDs versus account/character IDs, fixed string encoding, master/permission checks, confirmation and server notice order. Guild chat belongs toPK17.

### Discovery ownership

45 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

Finding P17: Guild receive support is not complete guild functionality.

- Source anchor: `D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/qtui/QtUiStateAdapter.cpp`:3851; search `QObject::connect(m_state,&QtUiState::guildActionRequested`. Recheck current line/guard.
- Source anchor: `C:/Users/sziad/source/repos/rathena-master/src/map/clif.cpp`:14584; search `void clif_parse_GuildMessage`. Recheck current line/guard.

## PK13 — Trade asymmetry and pending transactions

Tier: **S**. Dependencies: PK03, PK10.

### Bounded steps

- **PK13.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK13.02 [S]** Document trade_tradeack acceptance-distance result delivery to both peers. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK13.03 [S]** Implement client pending-state lifecycle and honest timeout/cancel handling. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK13.04 [S]** Prepare separately authorized minimal server correction with independent two-client replay. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK13.05 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

P21: current server notifies only acceptor then clears both trades. A client cannot invent a packet for uninformed requester. No external edit/rebuild/restart without server-patch authorization. Do not mark server bug fixed by timeout.

### Discovery ownership

2 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

Finding P21: Server trade distance failure notifies only one peer.

- Source anchor: `C:/Users/sziad/source/repos/rathena-master/src/map/trade.cpp`:108; search `void trade_tradeack`. Recheck current line/guard.
- Source anchor: `D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/Commerce.cpp`:38; search `void UpdateTradeContext`. Recheck current line/guard.

## PK14 — Party map positions

Tier: **M**. Dependencies: PK05.

### Bounded steps

- **PK14.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK14.02 [M]** Implement0107 by account ID with source-proven units/sentinels. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK14.03 [M]** Render/update/remove party minimap markers through existing party position state. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK14.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

P07: removal(-1) semantics, remote-map movement, roster loss, reconnect. L candidate after contract approval: marker state update.

### Discovery ownership

1 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

Finding P07: Party positions are discarded.

- Source anchor: `D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/GameModePacket.cpp`:8801; search `router.Register(0x0107`. Recheck current line/guard.
- Source anchor: `C:/Users/sziad/source/repos/rathena-master/src/map/clif.cpp`:8065; search `void clif_party_xy(`. Recheck current line/guard.

## PK15 — Party lifecycle and configuration

Tier: **M**. Dependencies: PK14, PK10.

### Bounded steps

- **PK15.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK15.02 [M]** Audit roster invitations leave leader and HP transitions. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK15.03 [M]** Implement party invite settings02C8/02C9 and party loot02B8. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK15.04 [M]** Remove incorrect receive assumption for0100 and verify its leave-party send path. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK15.05 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

Local/remote bars and names must share layering and anchor rules. Reject/reinvite, cross-map and double notices cannot leave stale membership. New leader07FC requires version proof.

### Discovery ownership

26 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

Finding P11: Equipment/privacy and party-invite settings are discarded.

- Source anchor: `D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/GameModePacket.cpp`:8832; search `router.Register(0x02D9`. Recheck current line/guard.
- Source anchor: `C:/Users/sziad/source/repos/rathena-master/src/map/clif.cpp`:17719; search `void clif_parse_configuration`. Recheck current line/guard.

## PK16 — Privacy settings and social state

Tier: **M**. Dependencies: PK05.

### Bounded steps

- **PK16.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK16.02 [M]** Implement02D8/02D9/02DA request/ack server-confirmed settings. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK16.03 [M]** Audit friends whispers blocklists user counts and their failure notices. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK16.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

P11: UI must reflect a rejected change and server restore after reconnect. Friend snapshots additionally requirePK34 atomic validation. L candidate: approved boolean display ack.

### Discovery ownership

18 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

Finding P11: Equipment/privacy and party-invite settings are discarded.

- Source anchor: `D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/GameModePacket.cpp`:8832; search `router.Register(0x02D9`. Recheck current line/guard.
- Source anchor: `C:/Users/sziad/source/repos/rathena-master/src/map/clif.cpp`:17719; search `void clif_parse_configuration`. Recheck current line/guard.

## PK17 — Party and guild chat transport

Tier: **M**. Dependencies: PK12, PK15.

### Bounded steps

- **PK17.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK17.02 [M]** Add distinct guild channel state and receive filtering for017F. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK17.03 [M]** Wire0108 party and017E guild text sends through real Qt input paths. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK17.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

P18: no sending to public channel, preserve encoding/message limits/permissions, no duplicate optimistic echo. Test guild-only and party-only tabs plus combined history copy behavior.

### Discovery ownership

3 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

Finding P18: Group chat transport and filtering are incomplete.

- Source anchor: `D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/GameMode.cpp`:4736; search `bool SendGlobalChatMessage`. Recheck current line/guard.
- Source anchor: `D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/GameModePacket.cpp`:7217; search `if(op==0x17f)`. Recheck current line/guard.
- Source anchor: `D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/ui/UINewChatWnd.cpp`:142; search `kChatFilterLabels`. Recheck current line/guard.

## PK18 — Dynamic map cells

Tier: **S**. Dependencies: PK05.

### Bounded steps

- **PK18.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK18.02 [S]** Implement0192 updates in collision/pathfinding and rendered cell state. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK18.03 [S]** Reset and reapply changes safely during map reload. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK18.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

P08: terrain type semantics and world coordinate units must come from server and Ref; test walking/pathfinding across changed cells and map teardown.

### Discovery ownership

1 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

Finding P08: Map cell and map-property changes are discarded.

- Source anchor: `D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/GameModePacket.cpp`:8811; search `router.Register(0x0192`. Recheck current line/guard.
- Source anchor: `C:/Users/sziad/source/repos/rathena-master/src/map/clif.cpp`:5363; search `void clif_changemapcell`. Recheck current line/guard.

## PK19 — Map properties and restrictions

Tier: **S**. Dependencies: PK18.

### Bounded steps

- **PK19.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK19.02 [S]** Implement0199/01D6/02E7 per active layout. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK19.03 [S]** Propagate map rules to relevant UI/actions and presentation. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK19.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

P08: no guessed global night/light flags. Preserve bit namespaces and old/new variants. Restrictions apply to actual actions, not only labels; inactive variants need proof.

### Discovery ownership

3 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

Finding P08: Map cell and map-property changes are discarded.

- Source anchor: `D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/GameModePacket.cpp`:8811; search `router.Register(0x0192`. Recheck current line/guard.
- Source anchor: `C:/Users/sziad/source/repos/rathena-master/src/map/clif.cpp`:5363; search `void clif_changemapcell`. Recheck current line/guard.

## PK20 — NPC class and resource replacement

Tier: **S**. Dependencies: PK05.

### Bounded steps

- **PK20.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK20.02 [S]** Implement01B0 distinct from01D7 and rebuild actor representation. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK20.03 [S]** Verify sprite/GR2 resource ownership picking and effects during replacement. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK20.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

P09: visible-to-invisible-to-visible, missing actor, repeated change, map teardown and resource release. No special guessed NPC class exceptions.

### Discovery ownership

1 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

Finding P09: NPC class-change notification is discarded.

- Source anchor: `D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/GameModePacket.cpp`:8799; search `router.Register(0x01B0`. Recheck current line/guard.
- Source anchor: `C:/Users/sziad/source/repos/rathena-master/src/map/clif.cpp`:1471; search `void clif_class_change`. Recheck current line/guard.

## PK21 — NPC progress interaction

Tier: **S**. Dependencies: PK10, PK25.

### Bounded steps

- **PK21.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK21.02 [S]** Implement02F0/02F2 progress start/abort state and02F1 reply. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK21.03 [S]** Wire input locks cancellation expiration and map/disconnect teardown. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK21.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

Use server units/timers and Ref lifecycle; no guessed completion delay or duplicate ack. Verify active guard before registering.

### Discovery ownership

3 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

## PK22 — Extended status and ShowEFST

Tier: **S**. Dependencies: PK05.

### Bounded steps

- **PK22.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK22.02 [S]** Port028A branches and flag lifetimes beyond level99/Assumptio. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK22.03 [S]** Decode043F remaining value fields only under proven layout. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK22.04 [S]** Apply per-status start refresh end and actor teardown semantics. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK22.05 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

P13/P14: compare each Ref branch and server status enum. Do not OR unrelated bit namespaces or merely store unexplained fields. Newer043F is not default2008 proof.

### Discovery ownership

2 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

Finding P13: Show-EFST notification is only partially implemented.

- Source anchor: `D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/GameModePacket.cpp`:599; search `void HandleNpcShowEfstUpdate`. Recheck current line/guard.
- Source anchor: `D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/Ref/GameModePacket.cpp`:4595; search `int showEFST)`. Recheck current line/guard.

Finding P14: Extended status values are parsed only as icon timing.

- Source anchor: `D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/GameModePacket.cpp`:1109; search `void HandlePacket043F`. Recheck current line/guard.

## PK23 — Existing skill packet semantic audit

Tier: **S**. Dependencies: PK08, PK22.

### Bounded steps

- **PK23.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK23.02 [S]** Enumerate every skill handler branch and connect to skill plan ownership. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK23.03 [S]** Verify damage cast target ground effect duration cancel and rejection semantics per contract. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK23.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

All registered skill handlers need field/branch evidence. Existing skill plan cannot override server wire truth. Break into named handler leaf steps before coding, avoid generic effect fallbacks.

### Discovery ownership

33 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

## PK24 — Stats resources and actor state invariants

Tier: **S**. Dependencies: PK05.

### Bounded steps

- **PK24.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK24.02 [S]** Audit signed widths HP/SP EXP stats and status increments. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK24.03 [S]** Verify actor state transitions and lifecycle ordering with extreme values. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK24.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

Do not truncate HP or use overflow-prone percentage arithmetic. Source distinguishes display from authoritative resources. Hallucination display must not mutate actual damage state.

### Discovery ownership

44 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

## PK25 — NPC dialogue and script messages

Tier: **M**. Dependencies: PK10.

### Bounded steps

- **PK25.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK25.02 [M]** Audit message menu input next close and shop handoffs. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK25.03 [M]** Implement missing compass0144 map notifications0189 and script choices. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK25.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

Honor server selection indexes, cancel values, string limits and input lifetime. L candidate: already specified text-only result notice. Merchant shop and vending are separate protocols.

### Discovery ownership

21 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

## PK26 — Special skill information and choices

Tier: **M**. Dependencies: PK23, PK25.

### Bounded steps

- **PK26.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK26.02 [M]** Implement memo011E monster estimate018C and special skill choices. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK26.03 [M]** Audit StarGladiator020E/0253 and source-gated choice requests. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK26.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

Selections need server-valid values and failure UI. Do not synthesize unsupported skills from Ref-only branches.

### Discovery ownership

4 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

## PK27 — Complete pet lifecycle

Tier: **S**. Dependencies: PK07, PK20.

### Bounded steps

- **PK27.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK27.02 [S]** Implement capture019E/01A0 and feed/name/status01A3/01A4/01A6/01AA. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK27.03 [S]** Implement pet inventory/dialogue and follow/actor synchronization. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK27.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

Inspect every pet emitter/parser including failure cases, IDs/egg linkage/hunger/intimacy and cleanup. A35-byte framing fix is not completion.

### Discovery ownership

9 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

Finding P04: Pet status length is wrong for the configured server.

- Source anchor: `D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/network/GronPacket.cpp`:260; search `SetPacketSize(0x01A2, 37)`. Recheck current line/guard.
- Source anchor: `C:/Users/sziad/source/repos/rathena-master/src/map/packets_struct.hpp`:5441; search `struct PACKET_ZC_PROPERTY_PET`. Recheck current line/guard.

## PK28 — Homunculus lifecycle

Tier: **S**. Dependencies: PK23, PK24.

### Bounded steps

- **PK28.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK28.02 [S]** Implement022E/022F/0230 status and skill0235/0239 contracts. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK28.03 [S]** Wire summon death rename feeding skill UI and actor reset paths. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK28.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

Enumerate actual active family from server rather than assuming listed seeds exhaustive. Test owner/companion IDs and HP/SP ordering.

### Discovery ownership

6 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

## PK29 — Mercenary and elemental lifecycle

Tier: **S**. Dependencies: PK23, PK24.

### Bounded steps

- **PK29.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK29.02 [S]** Implement active mercenary029B/02A2 and skill family. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK29.03 [S]** Classify elemental and newer companion variants then implement applicable branches. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK29.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

No blanket exclusion based on feature age; prove guards/call sites. Different companions cannot share guessed status layouts.

### Discovery ownership

6 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

## PK30 — Marriage adoption and relationship state

Tier: **S**. Dependencies: PK16, PK25.

### Bounded steps

- **PK30.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK30.02 [S]** Implement01E6/01EA/01F6/01F8/0205/0216 applicable events. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK30.03 [S]** Wire request/consent/rejection and relationship state transitions. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK30.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

01F8 Ref arms adoption skill: consuming bytes is insufficient. Verify active server meaning before copying Ref behavior.

### Discovery ownership

7 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

## PK31 — Player chat rooms

Tier: **S**. Dependencies: PK10, PK16.

### Bounded steps

- **PK31.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK31.02 [S]** Implement00D5–00E3 room list create join owner/password/membership flows. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK31.03 [S]** Implement room bubble click entry removal capacity and disconnect cleanup. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK31.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

Not vending shops. Verify room IDs versus owner IDs and rejected password/join behavior. No fake room state while awaiting server.

### Discovery ownership

12 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

## PK32 — Legacy mail complete workflow

Tier: **S**. Dependencies: PK03, PK10.

### Bounded steps

- **PK32.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK32.02 [S]** Implement inbox0240 read0242 and new-mail024A. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK32.03 [S]** Implement attachment0245/0255 claim and zeny/item state. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK32.04 [S]** Implement delete0257 return0274 and full compose/send results. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK32.05 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

P19: requests023F/0241/0243/0244/0247/0273 verified individually. Prevent duplicate claim/send and handle full inventory, stale mail IDs, denial and cancel. Composer0248/0249 alone is not done.

### Discovery ownership

18 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

Finding P19: Legacy mail is compose/send only.

- Source anchor: `D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/Commerce.cpp`:146; search `if(c.action=="mail")`. Recheck current line/guard.
- Source anchor: `C:/Users/sziad/source/repos/rathena-master/src/map/clif.cpp`:16070; search `void clif_Mail_refreshinbox`. Recheck current line/guard.

## PK33 — Rental binding and broken equipment

Tier: **M**. Dependencies: PK09, PK24.

### Bounded steps

- **PK33.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK33.02 [M]** Implement0298/0299 rental notices and expiry state. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK33.03 [M]** Implement02D3 bind-on-equip and02BB damaged gear lifecycle. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK33.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

No removing equipment optimistically without server semantics. Verify units and item identity/index conventions. L candidate: approved expiry notice rendering.

### Discovery ownership

4 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

## PK34 — Atomic inventory storage and friend snapshots

Tier: **S**. Dependencies: PK09.

### Bounded steps

- **PK34.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK34.02 [S]** Validate full normal/equipment/cart/storage/friend arrays before mutation. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK34.03 [S]** Replace snapshots atomically and preserve state on malformed input. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK34.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

P20: exact header/count/record size, remainder rejection, bounds and maximum counts. Never clear state before validation. Test partial trailing record and subsequent valid snapshot.

### Discovery ownership

11 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

Finding P20: Snapshot parsers accept partial record arrays.

- Source anchor: `D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/GameModePacket.cpp`:2204; search `void HandleNormalInventoryList`. Recheck current line/guard.
- Source anchor: `D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/GameModePacket.cpp`:2242; search `void HandleEquipInventoryList`. Recheck current line/guard.
- Source anchor: `D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/GameModePacket.cpp`:7673; search `void HandleFriendsList`. Recheck current line/guard.

## PK35 — Craft refine repair and cooking

Tier: **S**. Dependencies: PK25, PK34.

### Bounded steps

- **PK35.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK35.02 [S]** Implement0188/018F refine and018D selection contracts. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK35.03 [S]** Implement01FC/01FE repair item choices/results. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK35.04 [S]** Implement025A cooking and remaining active crafting branches. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK35.05 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

Source-prove list record sizes, slots/card options, selection indexes and material consumption authority. Rejection/cancel paths release modal locks without inventing items.

### Discovery ownership

6 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

## PK36 — Storage password challenge

Tier: **S**. Dependencies: PK03, PK10, PK34.

### Bounded steps

- **PK36.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK36.02 [S]** Determine023A/023B/023C exact challenge and password algorithm. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK36.03 [S]** Implement secure entry/retry/change/unlock state and server failure branches. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK36.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

P10: no guessed encryption, plaintext shortcut, logged passwords or UI-only pretend unlock. Source ambiguity is a strong-agent blocker, not permission to disable authentication.

### Discovery ownership

3 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

Finding P10: Storage password flow is a message/log stub.

- Source anchor: `D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/GameModePacket.cpp`:2416; search `void HandleStoragePasswordRequest`. Recheck current line/guard.
- Source anchor: `D:/Spel/RoRebuild/Ragnarok___Win32_HighPriest2008_Release/src/gamemode/GameModePacket.cpp`:2427; search `void HandleStoragePasswordResult`. Recheck current line/guard.

## PK37 — Existing commerce and trade consistency

Tier: **S**. Dependencies: PK13, PK34.

### Bounded steps

- **PK37.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK37.02 [S]** Audit NPC shops cart transfers and vending setup/bubbles/buying lists. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK37.03 [S]** Audit trade both peers lock accept cancel results and transaction ownership. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK37.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

Verify every production sender/server parser, partial purchases, quantities/prices, stale indices, insufficient funds, capacity and teardown. Server confirms mutations; UI drag/drop must use verified request paths.

### Discovery ownership

39 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

## PK38 — Item use and equipment action parity

Tier: **M**. Dependencies: PK33, PK34.

### Bounded steps

- **PK38.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK38.02 [M]** Audit use equip unequip drop pickup and storage transfers. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK38.03 [M]** Implement missing card identification and selection result handling. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK38.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

Per-action source-derived requests/results, inventory indices and quantities. Verify rejection and rapid repeat do not duplicate or silently delete items.

### Discovery ownership

52 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

## PK39 — MVP rankings PvP manner and messages

Tier: **M**. Dependencies: PK05, PK24.

### Bounded steps

- **PK39.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK39.02 [M]** Implement MVP010A–010D and rankings0219/021A/021B/021C/0224/0226/0236/0238. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK39.03 [M]** Implement014A/014B/01AB manner and02C1 colored messages. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK39.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

Units, ID/name encoding, color and duration from sources. L candidates: approved single-result notices. Do not reuse guild/party chat channels for global results.

### Discovery ownership

27 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

## PK40 — Auction cash shop and banking

Tier: **S**. Dependencies: PK03, PK34.

### Bounded steps

- **PK40.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK40.02 [S]** Implement active0250/0252/0256/025D/025F auction branches. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK40.03 [S]** Implement0287/0289 cash shop state and balances. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK40.04 [S]** Classify09A6 and related bank requests then implement applicable contracts. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK40.05 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

Each financial flow is a separate strong-reviewed child step. No modern bank request on2008 without proof. Reject overflows, stale quotes, duplicate responses and malformed lists.

### Discovery ownership

31 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

## PK41 — Battleground identity and state

Tier: **S**. Dependencies: PK14, PK19, PK24.

### Bounded steps

- **PK41.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK41.02 [S]** Implement02DD team/emblem and02DE/02DF/02E0 markers/HP/scores. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK41.03 [S]** Implement actual team chat membership and teardown paths. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK41.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

Distinguish battleground IDs and guild emblems. Full server-family enumeration required, not only listed seeds.

### Discovery ownership

11 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

## PK42 — Instance metadata lifecycle

Tier: **S**. Dependencies: PK10, PK25.

### Bounded steps

- **PK42.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK42.02 [S]** Implement02CB–02CE creation state queue/wait and result branches. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK42.03 [S]** Wire instance UI entry restriction expiry and map/reconnect cleanup. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK42.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

Inspect active server guards/call sites first; no invented dungeon launch or unsupported requests. Timers and ownership must be source-backed.

### Discovery ownership

4 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

## PK43 — Buying store search and party booking

Tier: **S**. Dependencies: PK03, PK37.

### Bounded steps

- **PK43.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK43.02 [S]** Classify/implement0810-family buying stores including0814/0816 board lifecycle. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK43.03 [S]** Classify/implement0837-family search and0803-family party booking. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK43.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

Per-contract configuration evidence; modern opcode presence in client table is not server support. Inspect legacy vending separately. Pending unsupported features stay open applicability work, not silent discard.

### Discovery ownership

20 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

## PK44 — Later feature and login-character applicability

Tier: **S**. Dependencies: PK01, PK02.

### Bounded steps

- **PK44.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK44.02 [S]** Classify clans0988 achievements0A23 roulette0A1A attendance0AF0 dressroom0A02 guild storage09DA reputation0B8D and related variants. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK44.03 [S]** Audit login/char rename028E/0290 delete PIN slots billing and other stage variants. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK44.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

For every row prove active/inactive/alias, then split applicable features into approved implementation leaf IDs. Do not blanket-dismiss newer-looking opcodes or conflate login/map reuse.

### Discovery ownership

59 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

## PK45 — All existing actor and receive-handler semantics

Tier: **S**. Dependencies: PK20, PK22, PK24.

### Bounded steps

- **PK45.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK45.02 [S]** Audit every actor spawn move action name and resource lifecycle handler. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK45.03 [S]** Audit every remaining registered handler field/branch through state/UI/replies. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK45.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

240 registered entries and136 indexed handler functions need explicit contract references, not a global pass assertion. Expand per handler/variant into numbered leaves; sitting/desync/picking/cart depth are regression scenarios when relevant.

### Discovery ownership

52 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

## PK46 — Exhaustive outbound parser parity

Tier: **S**. Dependencies: PK02, PK03.

### Bounded steps

- **PK46.01 [S]** Approve the exact source contract and bounded leaf specification for this package; enumerate all applicable packet variants and branch checks. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK46.02 [S]** Trace all177 accepted map requests through profile/raw-buffer/Qt user actions. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK46.03 [S]** Verify login/char outbound requests and unsupported action gates. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK46.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

49 constructor matches do not certify correctness;128 unproven need real trace. Compare opcode length every offset to final server parser, including failure/cancel payloads. No send-only packet requires fake receive handler.

### Discovery ownership

0 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

## PK47 — Residual and historical classification closure

Tier: **S**. Dependencies: PK01, PK02.

### Bounded steps

- **PK47.01 [S]** Approve this package's bounded research/harness/review specification, exact source inputs, output schema and evidence gate. For runtime contracts encountered, require independent strong approval before implementation. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK47.02 [S]** Review all unresolved990-row discoveries and1384 historical definitions. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK47.03 [S]** Adjudicate remaining collisions aliases inactive guards and source gaps with explicit evidence. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK47.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

No catch-all task completion without per-row contracts/classification. Add newly discovered packets/feature leaves and their dependencies; residual routing is provisional ownership, never approval to ignore.

### Discovery ownership

370 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

## PK48 — Independent certification review

Tier: **S**. Dependencies: PK00, PK01, PK02, PK03, PK04, PK05, PK06, PK07, PK08, PK09, PK10, PK11, PK12, PK13, PK14, PK15, PK16, PK17, PK18, PK19, PK20, PK21, PK22, PK23, PK24, PK25, PK26, PK27, PK28, PK29, PK30, PK31, PK32, PK33, PK34, PK35, PK36, PK37, PK38, PK39, PK40, PK41, PK42, PK43, PK44, PK45, PK46, PK47, PK50.

### Bounded steps

- **PK48.01 [S]** Approve this package's bounded research/harness/review specification, exact source inputs, output schema and evidence gate. For runtime contracts encountered, require independent strong approval before implementation. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK48.02 [S]** Cross-check every contract against independent server bytes and Ref behavior. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK48.03 [S]** Run fresh build production replay negative lifecycle and UI acceptance suites. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK48.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

Requires all applicable feature leaves across packages complete or explicitly recorded pending gates. Reviewer must not bless self-generated fixtures. No unsupported100% claim; counts by evidence level and open items.

### Discovery ownership

0 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

## PK49 — Final reproducible handoff and acceptance

Tier: **S**. Dependencies: PK48.

### Bounded steps

- **PK49.01 [S]** Approve this package's bounded research/harness/review specification, exact source inputs, output schema and evidence gate. For runtime contracts encountered, require independent strong approval before implementation. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK49.02 [S]** Prepare exact build/test/source manifest and user-run live scenarios. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK49.03 [S]** Record acceptance results unresolved limitations and release readiness. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK49.04 [S]** Independently review source/field/branch parity, run relevant production replay and UI lifecycle checks, close only evidence-backed rows, and commit the verification checkpoint. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

No automatic deployment/server restart. Final report lists every unresolved gate and commits. Live validation pending means live parity not certified.

### Discovery ownership

0 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.

## PK50 — Quest journal and mission lifecycle

Tier: **S**. Dependencies: PK03, PK10.

### Bounded steps

- **PK50.01 [S]** Approve02B1/02B2/02B3/02B4/02B5/02B7 and every related active request/emitter contract, identifying mission record versions. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK50.02 [M]** Implement complete quest list and mission snapshots atomically, preserving IDs, counts, target types and time units. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK50.03 [M]** Implement add/remove/status/kill progress updates and Qt journal/actions with map and reconnect lifecycle. **Checkpoint: evidence + tracker + separate commit + user progress update.**
- **PK50.04 [S]** Independently verify quest ordering, expiry, duplicate updates, malformed arrays, completion and rejection paths, then commit evidence. **Checkpoint: evidence + tracker + separate commit + user progress update.**

### Required acceptance and traps

No quest packet may remain a consume-only handler. Full snapshots and incremental updates must converge. Enumerate remaining family members from active server guards; do not assume seeds exhaustive.

### Discovery ownership

22 provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.
