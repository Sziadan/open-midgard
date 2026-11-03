# rAthena-authoritative packet implementation plan

Created 2026-11-03. This is an execution specification, not a claim that any runtime finding has been fixed.

## Start here

Read this document, [STATUS](packets/STATUS.md), [HANDOFF](packets/HANDOFF.md), then the assigned card in [WORK_PACKAGES](packets/WORK_PACKAGES.md). The machine inventory is [coverage.csv](packets/coverage.csv); task metadata is [work-packages.json](packets/work-packages.json). The original evidence is [packet audit](packet-audit/README.md). Use the [research runbook](packets/RESEARCH_RUNBOOK.md) for concrete source-navigation and task-splitting examples. Do one numbered leaf step at a time. Every completed leaf step requires its own reviewed commit and a user-facing progress update. Never claim a package complete because its decoder compiles.

The process follows [the skill implementation plan](SKILL_SYSTEM_1TO1_IMPLEMENTATION_PLAN.md): bounded units, source comparisons, evidence, independent progress records and resumable checkpoints. Its original-executable precedence and UI exclusions do **not** apply here. Necessary Qt interfaces, user actions, replies, failures and state transitions are part of packet completion.

## Authority and information sources

The server is `C:/Users/sziad/source/repos/rathena-master`. Do not substitute old eAthena, online packet wikis, or Ref's bundled server. For every wire contract inspect:

1. `src/custom/defines_pre.hpp` and `src/config/packets.hpp`, including their include order and actual preprocessor output. The audit baseline is PACKETVER/PACKETVER_SAK_NUM **20080910**; fallback definitions are not the effective configuration. Record local server revision, source hashes and configuration. Source configuration alone does not prove a running server was rebuilt from it; record that uncertainty.
2. `src/map/clif_packetdb.hpp`: evaluate all version guards and registrations in order; the last applicable assignment wins. Copy accepted opcode, fixed/variable length and parser field positions. A parser named elsewhere is insufficient.
3. `src/map/packets.hpp`, `packets_struct.hpp`, `src/common/packets.hpp`: obtain actual packed layout, optional fields, integer widths, signedness and bounds. Compile an independent sizeof/offsetof probe against the server headers where practical.
4. `src/map/clif.cpp`: follow the actual emitter/parser and every result branch. Follow its called feature module (locate with `rg --files src/map` and `rg -n`), such as `trade.cpp`, `guild.cpp`, `party.cpp`, `vending.cpp`, `pet.cpp`, `storage.cpp`, `mail.cpp`. Determine call-site reachability, ordering, resets and rejection behavior; a declaration or compiled emitter alone does not prove live applicability.
5. For login/character stages inspect `src/login/loginclif.cpp`, `src/char/char_clif.cpp` and common headers. Treat raw account-ID echoes as handshake data, not opcode registrations.

Ref is `<workspace>/Ref`. Begin with `GameModePacket.cpp`, `packet.h`, `Session.cpp`, `Session2.cpp`, `GameMode.cpp`, then locate the corresponding actor/UI functions with `rg`. Follow functions through their branches; record exact symbols and source lines/hashes. Ref defines intended client presentation and lifecycle where compatible with this server; rAthena always wins lengths, opcodes, offsets, enums and protocol behavior. A Ref conflict must be written down, not silently copied. Do not infer a missing asset or algorithm from a name. Inspect the actual asset and reference call path. Keep Ref ignored by Git and read-only; add no exceptions for it, JsGranny, server source, credentials or installed game data.

Client anchors: `src/network/GronPacket.cpp`, `MapSendProfile.cpp/.h`, `Connection.cpp`, `PacketQueue.*`, `Packet.h`; `src/gamemode/LoginMode.cpp`, `GameModePacket.cpp/.h`, `GameMode.cpp`, `Commerce.cpp/.h`; `src/session/Session.cpp/.h`; `src/core/GuildState.h`; `src/qtui/QtUiStateAdapter.cpp`, `QtCommerce.cpp`, `QtChatInput.cpp`, `qml/RoCommerce.qml`, `RoGuild.qml`, `GameOverlay.qml`; relevant `src/world` and `src/ui` code. Locate current code before editing; line numbers in the audit are historical anchors.

## What counts as coverage

The baseline has 990 numeric-opcode union rows, 603 final server map registry entries, 377 client table/router entries and 136 indexed map handler functions. These are discovery counts, not implemented-feature counts. The 240 registered entries are **not certified complete**. The 195 missing-handler and 182 missing-framing candidates need applicability review; respectively 119 and 106 have stronger sender/layout evidence. The 177 accepted map request rows include only 49 constructor/profile matches from a limited scan; the other 128 are unproven, not automatically absent. The historical catalog has 1,384 numeric definitions across versions.

`coverage.csv` gives every discovery row a primary work package, a reason and secondary reviews. Text-based routing is a provisional research assignment, never proof of protocol identity or feature completeness. The canonical identity must be `(connection stage, direction, active profile/date, opcode, layout variant)`. Split numeric rows when needed. Every split contract retains its discovery parent. Review all historical entries in PK47, including entries absent from the 990-row union. Account-ID prefixes receive separate handshake contracts.

Valid final classifications: applicable and fully implemented/verified; inactive for the selected configuration with reproducible preprocessor/call-site evidence; C2S-only with verified send path or explicitly documented unavailable server feature; alias mapped to a verified canonical contract. Unknown, intentionally ignored, log-only, stubbed, guessed inactive and registered-only are **open**. A C2S-only row is not a missing receive handler, but its user action/request still needs review. Adding a receive size merely prevents stream loss; it does not implement the feature. Newer features need individually proven version gating; this plan does not require upgrading the server or fabricating unsupported 2008 requests.

## Agent assignment rules

- **S — strong agent required.** Ambiguous protocol/version inference, streaming/handshake, concurrent state/lifetimes, security-sensitive algorithms, transaction synchronization, actor/resource rebuilding and broad parity certification. Do not assign to a weaker agent.
- **M — experienced implementation agent.** A bounded feature with source-proven contract; strong review required for contract approval and completion.
- **L — suitable for a weaker agent only after approval.** Fixed-layout leaf decoder, simple state update or UI presentation with exact contract, independent byte oracle and explicit allowed files. The package card identifies candidates; never delegate its whole package as an L task.

A weaker agent may not resolve source disagreements, invent return meanings, choose an encryption algorithm, guess render flags or broaden scope. Stop that leaf, record the exact unresolved question in BLOCKED and request a strong-agent decision; continue only independently approved work. Every package has a strong contract gate even when its implementation tier is M/L.

## Mandatory execution cycle for every numbered step

1. Read HANDOFF, STATUS, state.json and the relevant contracts. Record branch, HEAD, dirty paths, baseline commit and source/config hashes. Claim exactly one step; verify dependencies have completion evidence. Do not overwrite another agent's files. The current runtime baseline is dirty and not preserved by the planning commit: PK00 must resolve this before implementation claims can be reproducible.
2. Create/approve the exact contract using CONTRACT_TEMPLATE. Explain server/Ref disagreements. Specify the maximum bounded set of packets, UI actions and files in a copied WORK_ITEM_TEMPLATE. Split a step further if it spans independent state machines or cannot be reviewed in one commit. Allocate permanent child IDs before starting and update dependencies/coverage. Package-wide scaffolding must not substitute for these leaf implementations.
3. Implement the actual behavior, including success, failure, cancellation, repeats, reordered updates, actor disappearance and map/reconnect teardown as applicable. Decode into validated temporary values before mutating live state. No silent discard, generic placeholder UI, fake success, TODO-as-completion, arbitrary sleeps/retry loops, or guessed packet scanning. Unsupported server requests must not be sent. Preserve server-confirmed truth rather than optimistic permanent state.
4. Verify against independent server-derived fixtures through production framing, routing and outbound builders, plus state/UI tests. Do not use the same client struct/constant to generate expected bytes. Check every field/branch, partial headers/bodies, concatenated packets, invalid lengths/counts, zero/max values, string encoding/termination, array atomicity, reject/cancel/reconnect. Tailor cases to the contract; no meaningless tests mirroring assignments. Check UI layering/input and actual replies for interactive features. Cross-check Ref's branches/constants and document deliberate Qt adaptation.
5. Run a fresh build before citing tests against that binary. Record exact commands, exit codes and new binary timestamp/hash; stale binaries and source scans do not prove runtime behavior. Follow EVIDENCE_TEMPLATE; code-only verification cannot certify visible behavior. If live evidence is necessary and unavailable, label that gate pending rather than inventing results.
6. Update state.json and STATUS, append PROGRESS, update HANDOFF with exact next step, files, commands, evidence and blockers. Keep plan specification stable; do not fill it with session diaries. Record a stable commit subject in the files; after commit, resolve its SHA with git log. Do not endlessly amend just to embed the commit's own hash.
7. Review `git diff` and staged diff. Stage only this step's owned files (explicit paths, never `git add .`), including its progress/evidence. Commit `packet(PKxx.nn): concrete outcome`. Preserve unrelated work. If commit fails, state remains READY_TO_COMMIT/IN_PROGRESS; do not report completed. A blocked research checkpoint may be committed but remains blocked. No push unless requested.
8. Send a progress update: step ID, concrete result, verification/remaining limits, commit SHA and next exact step. Completion is valid only with this checkpoint and a real commit. Another agent must be able to resume from the repository without reading this chat.

## Build and test safety

Use the repository's actual MinGW Qt presets. The preset enables automatic developer deployment, so explicitly disable it:

```powershell
cmake --preset mingw-qt-x64 -DRO_ENABLE_DEV_DEPLOY=OFF
cmake --build --preset build-mingw-qt-x64 -j 6
```

Verify `RO_ENABLE_DEV_DEPLOY:BOOL=OFF` in `build-mingw-preset/CMakeCache.txt` before building. Confirm compiler/Qt paths from the preset rather than assuming another machine has the same install. PK03 must document confirmed focused test commands/targets; do not invent a CTest target or claim an old VS test tree passed. Existing starting points include `tools/tests/check_commerce.py`, `check_guild_window.py`, `check_send_queue.py`, `tools/tests/replay/CMakeLists.txt`, `UseSkillToPosInfoPacketTests.cpp`. Inspect each script first: old text-skill tests can encode the incorrect Ref layout and must be corrected with a server oracle.

Use build-directory fixtures and localhost mock servers, installed assets read-only. Do not deploy to or run `D:/Spel/OldRO/open-midgard.exe`, kill the user's client, restart/reconfigure the server, or transact on live accounts as an automated test. Native mouse fixtures must run serially. Obtain separately authorized live validation when required. External rAthena modifications belong to an explicitly authorized separate server patch, never an incidental client fix.

## Schedule and exit gates

Work in dependency order from WORK_PACKAGES. PK00–PK03 establish a reproducible baseline, applicability model and independent replay oracle. PK04–PK09 address stream-corrupting findings before UI-heavy features. Then prioritize guild/trade/party and existing-state faults before new feature families. Packages may be scheduled independently after their dependencies, but a single agent owns a leaf and its files. Shared router/session/Qt changes need serialization.

All 21 headline findings have explicit owners in finding-ownership.json. All ignored registrations have explicit owners in ignored-ownership.json. Full coverage additionally requires PK45–PK47 to close existing semantic, outbound and historical blind spots; fixing the 21 findings alone is insufficient. PK48 reviews all contracts independently and PK49 produces the final user acceptance handoff. No blanket 'handled' allowlist, no fabricated 100% from router coverage, and no omitted open rows in the final report.

Run `python tools/check_packet_plan.py` after plan/tracker changes. To refresh discovery, inspect and run `python tools/audit_packets.py`, then `python tools/render_packet_audit.py`; the first generator overwrites review annotations and the second reapplies the current ones. Preserve canonical contracts, ownership decisions and evidence separately. After regeneration use `python tools/generate_packet_plan.py --refresh-coverage` to route discovery rows without resetting progress; inspect all changes and manually adjudicate new/reused identities. Keep generated audit/probe outputs distinct from semantic completion claims.
