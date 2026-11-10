# Packet implementation progress — append only

## Planning checkpoint — 2026-11-03

Reviewed the packet audit findings and skill-plan process. Created rAthena-authoritative execution rules,51 work packages, per-step checkpoints, complete discovery ownership and contract/evidence/delegation templates. All implementation steps remain NOT_STARTED. Existing runtime working tree remains untouched and requires PK00 baseline reconciliation. No runtime build/test result is claimed by this planning checkpoint.

Planning validation passed:51 packages,211 numbered steps,990 discovery owners,21 findings,19 ignored registrations; dependency graph, trackers, checkpoints and relative links valid. Four audit/planning Python sources parse successfully. Regeneration is idempotent and preserves state. No runtime build was run for this documentation-only checkpoint. Stable planning commit subject: `docs: add rAthena-authoritative packet implementation plan`.

Future entry format: date; leaf ID; baseline and source/config; exact behavior delivered; contract/evidence paths; exact test results/limits; stable commit subject; next leaf/blocker. Include every checkpoint, including blocked research; blocked is not completed.

## 2026-11-03 — PK00.01

Baseline `af41f6484cb7ff31825296babe4b913aa36c4c67` on clean `main`, tracking `origin/main`. Handoff's prior HEAD/dirty-path description is stale: the current HEAD includes the guild/commerce work and the tree has no staged, unstaged, or non-ignored untracked paths. Defined explicit ownership and preservation boundaries; ignored dependencies remain unclassified and must be read-only inventoried in PK00.02. Evidence: `evidence/PK00.01.md`. No runtime tests/build run or claimed. Stable subject: `packet(PK00.01): define baseline ownership boundaries`. Next: PK00.02.

## 2026-11-03 — PK00.02

At HEAD `8c81549bb9d4bf89ac59f53b99827ce07ccc3377`, recorded 1,198 tracked file hashes plus ignored source/reference inventory and all 35,865 GRF asset paths/sizes. Ignored build dirs have mixed deploy settings; `build-mingw-preset` is the only reviewed candidate here with deploy OFF, but no existing build is certified. The authoritative rAthena path exists but Git commands are denied by filesystem permissions, so its revision/status/hash remain open for PK01. No runtime code or test changed. Evidence: `evidence/PK00.02.md` and `evidence/PK00.02-manifest.json`. Stable subject: `packet(PK00.02): inventory packet baseline dependencies`. Next: PK00.03.

## 2026-11-03 — PK00.03

Selected existing runtime commit `af41f6484cb7ff31825296babe4b913aa36c4c67` at `origin/main`; only `docs/packets/*` differs in this evidence branch. Recorded exact MinGW Qt configure/build commands with deploy explicitly OFF, compiler/Qt paths, and prerequisite cache check. No build was run. Server source remains inaccessible and is carried to PK01; PK00.04 will establish the local client build gate. Evidence: `evidence/PK00.03.md`. Stable subject: `packet(PK00.03): select reproducible client baseline`. Next: PK00.04.

## 2026-11-03 — PK00.04

Reconfigured and clean-built the selected client baseline using the MinGW Qt preset with deploy OFF; both configure and clean build exited 0. Fresh output is `build-mingw-preset/open-midgard.exe` (19,655,065 bytes; SHA-256 `0EC74E399EBB3CC8CB7F7EB27139D5CF299911E78B77D1497D2B3A6688FAC7BA`, timestamp 2026-11-03T19:06:37.7481701Z). Qt deployment stayed in the build directory; `D:/Spel/OldRO/open-midgard.exe` timestamp predates the build and has a different hash. CMake/MinGW warnings are recorded in evidence. No tests or client runtime were run. External rAthena checkout remains inaccessible, so server contracts remain gated. Evidence: `evidence/PK00.04.md`. Stable subject: `packet(PK00.04): verify reproducible client baseline`. Next: PK01.01.

## 2026-11-03 — PK01.01

Specified exact authoritative server/client/reference inputs, canonical contract identity fields, independent oracle outputs, and approval/evidence gates. Corrected `source-baseline.json` to the selected client commit and added current hashes from PK00.02 while clearly marking historical server hashes unverified. PK01.01 approves the research structure only; no packet contract is approved. The rAthena checkout remains inaccessible, so PK01.02 and server-derived contracts are gated. Evidence: `evidence/PK01.01.md`; blocker recorded in `BLOCKED.md`. Stable subject: `packet(PK01.01): specify active protocol research evidence`. Next: PK01.02.

## 2026-11-03 — PK01.02

Authorized read-only source access succeeded. Reproduced the effective 20080910 config via actual include order/preprocessor and compiled the active `clif_packetdb.hpp` registry: 886 assignments, 603 final opcodes (last assignment wins). The refreshed packet audit reproduced all baseline counts and rendered review annotations back unchanged. Saved line-aware assignment output, final registry CSV, effective macro transcript, and hashes for 11 packet/config source inputs. The source directory has no `.git` metadata; deployed-server provenance remains unresolved. Evidence: `evidence/PK01.02.md` and the paired output files. Stable subject: `packet(PK01.02): reproduce effective server packet registry`. Next: PK01.03.

## 2026-11-03 — PK01.03 applicability approval

Strong reviewer `/root/pk01_p0_review` approved applicability only and confirmed permanent child identities for raw character account-ID handshake, cross-stage framing, map S2C 0x01F2 and 0x01A2, and distinct map C2S 0x0190, 0x007E, and profile-23 0x0437. Corrected 0x8482/0x8483 stage/direction and moved 0x0437 ownership from PK50 to PK46. Added eight contract records and linked discovery rows; classifications remain UNREVIEWED. Semantic implementation, historical variant reachability, independent fixtures, and server provenance remain open. Evidence: `evidence/PK01.03-applicability-approval.md` and `contracts/`. Stable subject: `packet(PK01.03): approve P0 applicability identities`. Next: PK01.04 independent verification of config, registry assignment order, contracts, and 990 coverage rows.


## 2026-11-03 — PK01.04 independent verification

Independent strong review reproduced the effective 20080910 preprocessor configuration and validated all 886 ordered registrations against the source, then reduced them to the same 603 final assignments. All 15 source hashes matched. All 990 unique discovery rows match the audit inventory, have valid owners, and all eight populated contract links resolve; classifications remain UNREVIEWED. Corrected stale source-access/count statements and reviewer-report provenance. This closes configuration/override/P0 applicability plus structural ownership only, not semantic packet validation. Evidence: `evidence/PK01.04-independent-verification.md`. Stable subject: `packet(PK01.04): independently verify packet identities and coverage`. Next: PK02.01 specification.


## 2026-11-03 — PK02.01 specification

Specified exact hash-pinned server inputs, active P0 applicability set, deterministic JSON manifest schema, independent fixture schema, validation rules, reproducibility expectations, and strong-review gate. Kept framing as a state machine and excluded the later pet layout from the active 20080910 fixtures. No probe or runtime behavior is certified by this planning leaf. Evidence: `evidence/PK02.01-specification.md`. Stable subject: `packet(PK02.01): specify server manifest and byte oracle`. Next: PK02.02 actual-guard registry and sizeof/offsetof probes.


## 2026-11-03 — PK02.02 probes

Recompiled the active guarded registry and reproduced 886 assignments/603 final registrations. Compiled `tools/packet_contract_layout_probe.cpp` using the actual PACKETVER-guarded server packet declarations: guild status is 20 bytes and pet status is 35 bytes with source-backed offsets. Generated a seven-contract manifest after checking all 15 pinned server source hashes. Evidence includes probe commands/output and hashes. This does not certify emitter reachability or runtime semantics; byte fixtures remain PK02.03. Stable subject: `packet(PK02.02): probe guarded server packet layouts`. Next: PK02.03.


## 2026-11-03 — PK02.03 fixtures

Created six synthetic raw fixtures for the handshake, 0x01F2, 0x01A2, 0x0190, 0x007E, and 0x0437. Added a generator/validator that checks contract links and manifest field offsets, exact lengths, hashes, field slices, bounds, overlap, and synthetic-data marking; its negative corruption check rejects invalid expectations. Framing is explicitly a state machine, not a fabricated fixed packet. These bytes are independent source-derived expectations, not production path tests. Evidence: `evidence/PK02.03-fixtures.md` and its JSON/hex files. Stable subject: `packet(PK02.03): create server-derived byte fixtures`. Next: PK02.04 independent review.


## 2026-11-03 — PK02.04 independent verification

Independent strong review reran manifest generation, fixture generation/negative validation, and the plan checker. The reviewer confirmed fresh probe compilation, source-hash checks, rebuilt generated header, effective macro values, measured layout comparisons, C2S parser offsets/lengths, fixture field derivation and deterministic regeneration. Initial validator gaps were corrected: manifest/opcode/profile/length/schema/suite/provenance checks now reject the reviewed bad cases; explicit opcode/padding fields and unresolved semantic metadata are recorded. All evidence hashes/output have been refreshed. This approves only the bounded manifest/byte-oracle infrastructure. Evidence: `evidence/PK02.04-independent-review.md`. Stable subject: `packet(PK02.04): independently verify contract byte oracles`. Next: PK03.01.


## 2026-11-03 — PK03.01 specification

Mapped production receive, queue, profile, send and UI entry points; specified a loopback-only harness, deterministic fragmentation/concatenation/error cases, evidence JSONL and opt-in deploy-off CMake commands. Flagged the existing 0x0190 helper test as an incompatible client-derived oracle (profile 23/90 bytes) against the server contracts; it remains a PK08 correction gate. No tests or runtime files changed in this specification leaf. Evidence: `evidence/PK03.01-replay-specification.md`. Stable subject: `packet(PK03.01): specify production replay harness`. Next: PK03.02.


## 2026-11-03 — PK03.02 production stream replay

Added opt-in deploy-off loopback CTest for production `Connect`/`RecvPacket` and the exact `TryReadPacket` boundary decoder; extracted the unchanged decoder into an isolated production source so the test avoids unrelated app startup globals. Added bounded pumps, queue/state assertions and 28 per-case JSONL outcomes for fixed/variable splits, coalescing, EOF and reconnect. Strong review approved; final CTest passed 1/1 with replay ON/deploy OFF. Ordinary sandbox loopback was denied with WSA 10013; elevated local-only replay passed. The first fixture prototype exposed that client `GetPacketSize(0x01F2)` is unknown despite server emit layout; owner PK06. Unknown/malformed scanning is logged as baseline defect characterization for PK05, not approved recovery. Router handler dispatch, raw handshake, stage/profile and legacy buffer safety remain open. Evidence: `evidence/PK03.02-replay.md` and `PK03.02-replay.jsonl`. Stable subject: `packet(PK03.02): replay production packet stream framing`. Next: PK03.03.

## 2026-11-03 — PK03.03 partial outbound transport capture

Added a loopback mock-peer case calling production `CRagConnection::SendPacket`, then strong review approved a bounded profile-23 0x0437 sender sub-leaf. Extracted `SendActionRequestPacket` unchanged into shared production source so replay invokes the same helper used by GameMode. Four source-derived frames, builder failure paths, zero-target attack/repeat rejection and closed-connection behavior passed. Replay target and `ro_gamemode` built; elevated local-only CTest passed 1/1. The package remains IN_PROGRESS: reviewer found return-to-character refusal 0x018B has no client route and pending disconnect-action cleanup is incomplete, so Session/Qt transition assertions need a separately reviewed contract. Evidence: `evidence/PK03.03-0437-contract-review.md`, `evidence/PK03.03-outbound-transport.md` and `.jsonl`.

PK03.04 audit inventory: verified the current deploy-off build/cache, 30 PASS JSON records, executable SHA/timestamp and the strong-approved sender scope. Identified that PK03.02 never dispatches to `CGameModePacketRouter`, PK03.03 has no UI lifecycle replay, and completion requires a different strong reviewer than the PK03.03 contract reviewer. This is not a completion review and closes no rows. Evidence: `evidence/PK03.04-replay-audit.md`.

## 2026-11-03 — PK03.04 evidence refresh (blocked)

Found the replay build cache had `RO_ENABLE_DEV_DEPLOY=ON`, contrary to the earlier audit note; no build was run until reconfiguring through `mingw-qt-x64-ci` with deployment OFF and packet replay ON. Force-rebuilt `packet_stream_replay_checks` and `ro_gamemode`; CTest passed 1/1 and all 30 emitted JSON records passed. The source-path audit confirms receive framing still bypasses `CGameModePacketRouter::Dispatch`; the 0x0437 sender replay does not cover Qt or Session transitions, and the 0x018B lifecycle remains without an approved contract. Requested an independent review from a separate code-review agent, but it could not inspect repository files and gave no approval. PK03.04 remains BLOCKED; no discovery rows closed. Evidence: `evidence/PK03.04-replay-audit.md`. Stable checkpoint subject: `packet(PK03.04): refresh deploy-off replay audit`. Next: obtain an independent strong review with access to actual workspace paths.

## 2026-11-03 — PK44.01 applicability scope draft (blocked)

Because PK44 depends only on completed PK01/PK02, recorded an independent scope draft for its 59 provisional primary parents (48 login/character, 11 map) while PK03.04 remains blocked. Reverified all 15 pinned server-source hashes and traced the visible later guards for roulette, achievements/title, attendance and clan emitters; did not classify or close any row, and left dressroom, guild storage, reputation and all login/character variants open. A separate code-review agent could not access the workspace/source files and gave no approval. PK44.01 remains BLOCKED. Evidence: `evidence/PK44.01-scope.md`. Stable checkpoint subject: `packet(PK44.01): scope later applicability review`.

## 2026-11-03 — PK03.04 disconnect refusal source trace

Traced server `0x00B2` restart type 1 and `0x018A` quit requests to their common `0x018B` reply. The `0x018B` result is request-context dependent: a type-1 return-to-character refusal is one case, while the separate quit-to-Windows action also receives this reply. Confirmed Ref has a receive route and branch behavior, while the rebuilt client currently frames/routes only `0x00B3`; the pending-action flow therefore needs a reviewed contract covering both requests and response branches. This source trace is not independent approval and closes no row. Evidence: `evidence/PK03.04-replay-audit.md`.

Stable checkpoint subject: `packet(PK03.04): trace disconnect refusal branches`.

## 2026-11-03 — PK03.04 independent review and contract draft (blocked)

A newly launched independent strong reviewer accessed the repository and reviewed PK03.04 source/branch mapping, fixture/cache evidence, production entry points, router coverage, lifecycle trace, and failure coverage. It did not approve completion: receive replay bypasses `CGameModePacketRouter`, and Session/Qt lifecycle plus the context-dependent `0x018B` route remain unverified. Created `contracts/PK03.04.DISCONNECT-ACK.md` as a source-backed DRAFT describing the `0x018B` wire contract and proposed pending-action matrix. Three separate attempts to obtain independent contract approval could not inspect the draft and pinned sources; no runtime behavior was changed, no rows were closed, and no approval is claimed. The deploy-off replay results are the prior checkpoint only; no new build/test was run for this draft. Stable checkpoint subject: `packet(PK03.04): draft disconnect acknowledgement contract`. Next: obtain accessible strong contract review; do not implement until approved or revised.

## 2026-11-03 — PK03.04 committed-contract review access blocker

After committing the DRAFT contract at `d463063b`, one more independent code-review attempt was made against the tracked file and source tree. It could not read the repository contents, so it provided no approval. Four contract-review attempts have now failed to inspect the draft/source; the earlier independent completion reviewer did inspect the code and did not approve PK03.04. The contract remains DRAFT, runtime behavior remains unchanged, and no rows are closed. No build/test was run. Stable checkpoint subject: `packet(PK03.04): record contract review access blocker`. Next: obtain an authorized reviewer/channel that can read the local contract and pinned sources.

## 2026-11-03 — PK03.04 default-router dispatch replay (blocked)

Added a replay case that sends synthetic receive-profile-sized `0x01F8` and `0x01B0` framing samples together over the existing local loopback; these are not evidence of server emission. The production receive path frames both, the default packet handlers are registered, and a local `CGameModePacketRouter` dispatches both no-op handlers. An unregistered opcode is rejected. Reconfigured with deploy OFF and replay tests ON; target build passed, CTest passed 1/1, and 31 JSON records were emitted, including the new PASS record. Fresh executable SHA-256: `9ADF2D11CFC769813115A1BEDC7CF2B259445D76B31076CC5777D6D784BF84A6` (19,711,385 bytes; 2026-11-03T22:05:26.8231020Z). This does not execute `CGameMode::OnUpdate`/the global router or assert feature semantics. An independent review attempt could not inspect the changed files. A fifth contract-review attempt likewise could not inspect the committed `0x018B` draft/source; lifecycle implementation remains blocked, no packet rows are closed, and no runtime packet behavior changed. Stable checkpoint subject: `packet(PK03.04): add default-router dispatch replay`. Next: obtain accessible strong review for the contract and replay, resolve the exact `OnUpdate` dispatch path if required, then address the approved lifecycle scope.

## 2026-11-03 — PK03.04 rAthena-only contract revision (blocked)

Followed `docs/PACKET_IMPLEMENTATION_PLAN.md` and verified the current local rAthena source at `C:\Users\sziad\source\repos\rathena-master`; the server source directory has no Git metadata, so its revision remains unknown and the deployed binary is not matched. Reverified SHA-256 for `clif.cpp`, `clif_packetdb.hpp`, and `chrif.cpp`; confirmed `0x018B` length 4, its result layout, the rAthena result/flush/optional-EOF order, and the `chrif_charselectreq` no-ack path. The user clarified rAthena is the sole server authority; removed the historical eAthena server comparison from the draft and retained Ref/dhxj only as the original-client reference. An independent rAthena-only contract review returned REVISE. Revision 4 of the DRAFT now assigns buffered-frame extraction before flush/receive to PK03.04 (no PK05 dependency), defines status precedence for LoginMode initialization, polling, CharSelect and connection-failure paths, and explicitly records the intentional result-0 divergence from Ref. No runtime source changed, no build/test was run, no rows were closed, and no contract approval is claimed. Next: independently review revision 4 against the local rAthena source only; do not implement before approval.

## 2026-11-03 — PK03.04 disconnect contract approval

An independent strong review restricted to the local rAthena server sources returned **APPROVE** for contract revision 4. It verified the rAthena request/response lengths and result layout, the result/flush/optional-EOF ordering, the character-server no-ack branch, the documented `0x00B3` value and packet structure, and client/reference behavior. Non-blocking implementation notes are recorded in the approved contract and evidence: pre-pass must fall through to flush/receive if no complete frame is buffered; use a named injectable `nowTick` seam; ground the rebuilt client's `0x00B3` length in the packed rAthena struct; record the `chrif.cpp` source hash. The contract is approved for implementation; PK03.04 remains blocked on production dispatch/lifecycle replay and independent completion review. No runtime code changed or was built, and no rows were closed. Next: implement the approved bounded scope without server edits/deploy and without claiming deployed-binary parity.

## 2026-11-03 — PK03.04 implementation and replay verification (completion review pending)

Implemented the approved revision 4 contract in client code. `CRagConnection::RecvPacket` now returns buffered complete frames before socket flush/receive can observe EOF; `0x018B` is framed and dispatched separately from `0x00B3`; request-specific result behavior, duplicate/send-failure handling, the 15-second drained-receive timeout, teardown/reconnect cleanup, timeout status precedence and chooser behavior are wired. The production `CGameMode::OnUpdate` and replay share `ProcessGameModePacketLoop`.

Configured `cmake --preset mingw-qt-x64-ci -DRO_ENABLE_DEV_DEPLOY=OFF -DRO_BUILD_PACKET_REPLAY_TESTS=ON`; verified replay tests ON and deployment OFF. `cmake --build build-mingw-preset --target packet_stream_replay_checks open_midgard --parallel 4` passed. `ctest --test-dir build-mingw-preset -R "^packet_stream_replay_checks$" --output-on-failure` passed 1/1 in 0.27 seconds; direct replay emitted 36 parseable JSON records, 36 PASS and 0 FAIL. Coverage includes request bytes, duplicates and failed sends, the acknowledgement result matrix and production handlers, orphan/`0x00B3` isolation, buffered acknowledgement before EOF, timeout drain-vs-budget, tick zero/wrap, and one-shot login/CharSelect status precedence. `python tools/check_packet_plan.py` and `git diff --check` passed.

Fresh artifacts: `build-mingw-preset/packet_stream_replay_checks.exe` SHA-256 `2547E4206BE53A796CAA481E43E80A416DE1DAAB86A23C6B6E3DBE41B4E6DBB8` (19,737,897 bytes); full `open-midgard.exe` SHA-256 `284D2EAA09B456DD640F3CB1ADE0A3B8D04F59197BDC8B7C9AD9CBB9E713FD34` (19,659,957 bytes). The replay was configured from the working tree and reports its dirty-source marker. No server was modified/deployed, no live account was used, and no discovery rows were closed. Fresh direct review against the approved contract found no mismatch; a separate independent implementation-completion review remains required, so PK03.04 remains `BLOCKED`. Next: obtain that review, resolve any findings, and commit the completion checkpoint.

## 2026-11-04 — PK03.04 global-router refusal dispatch replay

Advanced the local replay without requesting another completion review. The production packet-loop helper now reports dispatched/unhandled counts, and `CGameMode::OnInit`, `OnExit`, and `OnUpdate` access the same global router through one accessor. Replay drives that global router through the shared production helper and verifies 2/2 registered handlers dispatch, zero unhandled packets, drained receive, and no queued bytes. Added production `0x018B` result-1 dispatch cases for both pending return-to-character and exit-to-Windows; each verifies pending-action cleanup, the exact chat event visible in `UIWindowMgr`, and an open map connection. The three persistent records are `evidence/PK03.04-global-router-refusal.jsonl`.

Deploy-off build command: `cmake --build build-mingw-preset --target packet_stream_replay_checks open_midgard --parallel 4` (exit 0). CTest command: `ctest --test-dir build-mingw-preset -R "^packet_stream_replay_checks$" --output-on-failure` (1/1 passed); direct replay: 38 JSON records, 38 PASS / 0 FAIL. Cache: `RO_ENABLE_DEV_DEPLOY=OFF`, `RO_BUILD_PACKET_REPLAY_TESTS=ON`. Replay executable SHA-256 `208C409C51DF02C703197DA1AEB30E86FB87A9B66FE1BCEF8455A0CFCCFEADA9` (19,738,500 bytes; 2026-11-03T23:12:07Z); client SHA-256 `4D34AE38CEDD79FD52A981FCA3ECAEAE02AB61121ED18B66423590807E01FDE2` (19,660,014 bytes; 2026-11-03T23:12:10Z). Build used HEAD `271c99587c095037bf1499f198650f6144cea34d` with a dirty source tree. No deploy/server/live-account activity. Full `CGameMode::OnUpdate`, Login/chooser UI, and broader Session/Qt transitions remain unverified; separate implementation-completion review remains blocked per user direction. No discovery rows closed. Next local work: assess a safe production Login/chooser UI replay.

## 2026-11-04 — PK03.04 UI harness blocker recorded

Inspected the existing replay/test infrastructure and production Login/chooser paths. The current packet replay cannot safely exercise `CLoginMode::OnInit` or actual chooser activation: LoginMode initializes audio/assets/window state, while chooser requests are routed through the active `CModeMgr`; no isolated headless UI lifecycle harness exists. Did not initialize the application or make a live connection. Recorded this as a separate blocked gate in `BLOCKED.md` and the session todo; PK03.04's independent completion review remains blocked per the user's prior direction. PK04 cannot start because it depends on all of PK03, and no other dependency-ready todo is available. Next when unblocked: add/use a controlled UI harness or obtain authorized independent completion review, then resume dependency order.

## 2026-11-04 — PK03.04 acknowledgement fragmentation and chooser activation

Revisited the updated packet implementation rules: S/M/L labels are complexity guidance, independent review is deferred to the later review pass, and a review that is pending does not block eligible implementation. Reconciled the current STATUS, HANDOFF, BLOCKED, contract template, work-item template, runbook and generated package actions with that rule. PK03.04 is IN_PROGRESS because technical UI/mode-lifecycle evidence remains, not because review is pending. `tools/check_packet_plan.py` passes for all 51 packages, 211 steps and 990 discovery owners.

Added production-path replay for all three split boundaries of the four-byte 0x018B refusal frame. Each prefix stays buffered without dispatch; completing the frame dispatches once and queues the expected refusal message. Added headless chooser activation through `UIChooseWnd::OnKeyDown`, `CModeMgr::SendMsg` and `CGameMode::SendMsg`: Character Select and Exit To Windows are exposed, both options keep the chooser open on failed sends and close it after successful sends, and captured request bytes match `b20001` and `8a010000`. Corrected the chat-event evidence name to distinguish queued text from native/Qt rendering. Fixed a moved-from string passed by `UIWindowMgr::PushChatEvent` to an attached chat window; the offline replay does not claim that attached-window rendering was exercised.

Rebuilt `packet_stream_replay_checks` and `open_midgard` with `RO_ENABLE_DEV_DEPLOY=OFF` and `RO_BUILD_PACKET_REPLAY_TESTS=ON`. CTest passed 1/1 with local loopback enabled outside the restricted sandbox; direct execution emitted 42 parseable JSON records, all PASS. Seven scoped durable records are in `evidence/PK03.04-global-router-refusal.jsonl`. Replay executable SHA-256 `8651B7576081D9FF181174E2FD5D864B6DBF65B04092C54CEA93C32704267C1D` (19,744,968 bytes; 2026-11-03T23:36:14.733757Z); client executable SHA-256 `006EB7CCF235FBBE2F5E8C1E4BD07578E11DE7CF0AF5A0B8BBB86E2371793145` (19,660,014 bytes; 2026-11-03T23:31:53.778913Z). Configure source HEAD `271c99587c095037bf1499f198650f6144cea34d`, dirty source tree. No server/deployed client/live account activity; no rows closed.

Next: implement deterministic evidence for the timeout-driven mode switch/status and the LoginMode status handoff, then cover connection-failure/dialog preservation, transient PollNetwork precedence and teardown/reconnect. Independent completion review remains pending for the later review pass and is not a prerequisite.

## 2026-11-04 — PK03.04 EOF timeout fallback and LoginMode status adoption

Extracted `ApplyDisconnectTimeoutFallback` from `CGameMode::OnUpdate` and `AdoptPendingLoginStatus` from `CLoginMode::OnInit`; production now calls those same helpers exercised by replay. Added a no-ack EOF case proving the pending return action survives disconnect until the drained 15-second deadline, then clears and requests Login mode with the return-to-character flag. Added exit-timeout behavior across unsigned tick wrap, ensuring it clears the return flag, and tested one-shot timeout-status adoption over normal ready/server-selection status. This does not instantiate either full mode frame.

Deploy-off build of `packet_stream_replay_checks` and `open_midgard` succeeded; loopback CTest passed 1/1. Direct run emitted 43 parseable PASS records; eight durable focused JSONL records parse and pass. Replay executable SHA-256 `F4A10633983944C1745CA09F9A177370FBACCA39166298F76CFF722141656E51` (19,746,736 bytes; 2026-11-03T23:39:55.257150Z); client executable SHA-256 `EC5C7E1F4099FF2683F2F3FDD0C96ACF96DD5F22B50DDFCB6AA1E984772974DA` (19,660,191 bytes; 2026-11-03T23:39:57.810163Z). No server/live client activity or discovery-row closures.

PK03.04 remains IN_PROGRESS for connection-failure/dialog and transient status precedence, teardown/reconnect, attached chat rendering, and full-frame coverage. Independent implementation review is deferred and does not block eligible work.

## 2026-11-04 — PK03.04 LoginMode failure fallback and status precedence

Extended the production replay with controlled access to the existing LoginMode status writer and deferred notice. Representative transient statuses pass through `CLoginMode::SetLoginStatus` while the timeout notice remains pending. A six-byte synthetic ignored `0x0283` packet also traverses the actual `CLoginMode::PollNetwork` receive/status path and leaves the notice pending; the fixture claims no server emission or packet semantics. Stable Login and CharSelect precedence still consume the notice once. A loopback-refusal replay executes `CLoginMode::OnChangeState(ConnectChar)`, reaches the existing connection-failure dialog call, verifies failure status/socket closure/return scheduling, then invokes the stable Login transition and confirms the deferred timeout notice is displayed and consumed once. The test-only access is a friend declaration matching the existing replay-access pattern; no runtime status policy changed. It does not instantiate `CLoginMode::OnInit`, capture the dialog visually, exercise response-specific `PollNetwork` handlers, or instantiate the full `CGameMode::OnUpdate` frame.

`cmake --build build-mingw-preset --target packet_stream_replay_checks open_midgard --parallel 4` passed with cache `RO_ENABLE_DEV_DEPLOY=OFF`, `RO_BUILD_PACKET_REPLAY_TESTS=ON`. A sandboxed CTest attempt failed because localhost mock-peer access was denied; with local-loopback permission, `ctest --test-dir build-mingw-preset -R "^packet_stream_replay_checks$" --output-on-failure` passed 1/1. Direct replay emitted 45 parseable records: 45 PASS, 0 FAIL. `git diff --check` passed. Fresh replay binary SHA-256 `9CD0AC00FCCA1C958BCEE1A39A25C53A8DD3DD4CCE76A53E10F2A1A641E3C4C1` (19,753,574 bytes, 2026-11-03T23:54:43Z). The client binary is SHA-256 `B3485607CB389922F78C5DE06CC49EDA49766F92A2F1DDD59EBD3749C376E6E1` (19,660,191 bytes, 2026-11-03T23:49:40Z). Configure source was `271c99587c095037bf1499f198650f6144cea34d`, with a dirty tree. Durable focused evidence now has eleven PASS records in `evidence/PK03.04-global-router-refusal.jsonl`. No server, installed client or live account was used; no discovery rows were closed. Stable subject: `packet(PK03.04): replay LoginMode failure fallback`. Independent review remains deferred to PK48. Next: assess whether a controlled seam can exercise the timeout check inside the full `CGameMode::OnUpdate` receive loop and teardown ordering without initializing application rendering/assets; record any remaining unsafe full-frame validation precisely.
`cmake --build build-mingw-preset --target packet_stream_replay_checks open_midgard --parallel 4` passed with cache `RO_ENABLE_DEV_DEPLOY=OFF`, `RO_BUILD_PACKET_REPLAY_TESTS=ON`. A sandboxed CTest attempt failed because localhost mock-peer access was denied; with local-loopback permission, `ctest --test-dir build-mingw-preset -R "^packet_stream_replay_checks$" --output-on-failure` passed 1/1. Direct replay emitted 45 parseable records: 45 PASS, 0 FAIL. `git diff --check` passed. Fresh replay binary SHA-256 `9CD0AC00FCCA1C958BCEE1A39A25C53A8DD3DD4CCE76A53E10F2A1A641E3C4C1` (19,753,574 bytes, 2026-11-03T23:54:43Z). The client binary is SHA-256 `B3485607CB389922F78C5DE06CC49EDA49766F92A2F1DDD59EBD3749C376E6E1` (19,660,191 bytes, 2026-11-03T23:49:40Z). Configure source was `271c99587c095037bf1499f198650f6144cea34d`, with a dirty tree. Durable focused evidence now has eleven PASS records in `evidence/PK03.04-global-router-refusal.jsonl`. No server, installed client or live account was used; no discovery rows were closed. Stable subject: `packet(PK03.04): replay LoginMode failure fallback`. Independent review remains deferred to PK48. Next: assess whether a controlled seam can exercise the timeout check inside the full `CGameMode::OnUpdate` receive loop and teardown ordering without initializing application rendering/assets; record any remaining unsafe full-frame validation precisely.


## 2026-11-04 — PK03.04 shared timeout preamble seam

Factored the packet-loop timeout decision into `ApplyDisconnectTimeoutAfterPacketLoop`, now called directly by `CGameMode::OnUpdate` immediately after `ProcessGameModePacketLoop` and before the map-loading early return. Replay sends the actual drained result from the production receive loop through the same helper; it confirms EOF does not expire early, the 15-second deadline applies the fallback, unsigned wraparound works, and an exhausted packet budget at the same nominal deadline does not apply the fallback. When the helper returns true, its production call site returns from `OnUpdate` before actor, map-loading or render work. This seam tests the packet-loop decision and fallback, while source inspection verifies call-site ordering; the full `CGameMode::OnUpdate` frame remains uninstantiated.

Build cache remained `RO_ENABLE_DEV_DEPLOY=OFF`, `RO_BUILD_PACKET_REPLAY_TESTS=ON`. `cmake --build build-mingw-preset --target packet_stream_replay_checks open_midgard --parallel 4` passed; CTest passed 1/1 with local-loopback permission; direct replay emitted 45 parseable records, 45 PASS / 0 FAIL. The replay binary is SHA-256 `A62F90CE08A4A781C2BF7ADB4FA3240885EFF684456BADF56FCD97C9E93A17CC` (19,754,175 bytes, `2026-11-03T23:59:14Z`); the client binary is SHA-256 `F19D218814002E559D5EC22FB56741F687CF5FA09792E94271569805717545E4` (19,659,768 bytes, `2026-11-03T23:59:16Z`). No server, installed client or live account was used; no discovery rows were closed. The unresolved full `OnUpdate`/`OnInit` frames, `CGameMode::OnExit` cleanup path and visible/Qt rendering have broad world/UI lifecycle dependencies and remain recorded for later review rather than being claimed verified by this seam. PK03.04 remains `IN_PROGRESS`; the next active eligible leaf is PK44.01, which will define child contracts without reusing the old independent-review gate. Stable subject: `packet(PK03.04): exercise shared timeout preamble`.


## 2026-11-04 — PK44.01 permanent applicability research children

Replaced the stale strong-review access gate with a bounded PK44.01 scope checkpoint. Added seven permanent PK44.02 map-family research children and ten PK44.03 login/character stage-family children. The scope enumerates all 59 provisional PK44 primary discovery parents exactly once (48 login/character and 11 map); it keeps each discovery row open and unclassified. Attendance has no current PK44 primary parent; guild storage `0x09DA` remains PK12 primary and reputation `0x0B8D` remains PK47 primary pending source-backed cross-review. The child IDs are research boundaries, not final canonical contract or implementation IDs.

Updated work-package metadata, state and generated cards. `python tools/check_packet_plan.py` passed: 51 packages, 228 numbered steps, 990 discovery owners, 21 findings and 19 ignored registrations. `git diff --check` passed after removing Markdown trailing whitespace. No runtime source changed, no build/test was run, no discovery row was closed, and no independent review is claimed; PK48 owns that later review. Stable subject: `packet(PK44.01): allocate applicability research children`. Next: `PK44.02.001`.


## 2026-11-04 — PK44.02.001 clan source-date applicability

Traced clan map packet identities against the pinned rAthena snapshot. The packet header enum for `0x0988`, `0x0989` and `0x098E` is guarded by `PACKETVER >= 20120716`; their online-count, leave and chat emitters are compiled only at `PACKETVER >= 20131223`. Related clan info `0x098A` requires MAIN 20130626 / RE 20130605 / ZERO, and incoming clan chat `0x098D` is registered and parsed only at 20131223+. Under the reproduced 20080910 MAIN configuration these guards are false, so this source build emits none of the three S2C parent packets and registers no clan-chat parser. Traced clan join/leave/message callers in `src/map/clan.cpp`; they do not overcome the disabled packet bodies. Exact later-variant layouts, source/config hashes, Ref-search limits and deployed-binary caveat are recorded in `contracts/PK44.CLAN-MAP.md`.

Classified only `AUDIT-0x0988`, `AUDIT-0x0989` and `AUDIT-0x098E` as `INACTIVE_FOR_SELECTED_SOURCE_CONFIG`, each linked to its canonical identity; preserved PK44 primary ownership. Targeted searches found no matching clan packet handlers in the client or `Ref/dhxj`; this does not prove behavior for every original profile. No runtime source changed, no build/test was run, no deployed-server parity is claimed, and no independent review is claimed (deferred to PK48). Stable subject: `packet(PK44.02.001): classify clan source-date applicability`. Next: `PK44.02.002`.


## 2026-11-04 — PK44.02.002 roulette direction and source-date gates

Traced roulette registrations, emitters and all direct open call sites in the pinned server source. The 20141016+ `clif_packetdb.hpp` entries for `0x0A1A`, `0x0A1C`, `0x0A20`, and `0x0A22` use null parsers for same-number C2S input framing, while `clif.cpp` separately emits same-number S2C packets. Updated each discovery parent to `both/reused` and linked separate direction-specific canonical IDs. The direct atcommand and script open paths also reject PACKETVER below 20141022; the map request parser family is absent at PACKETVER 20080910, and the item-result emitter has its own 20141016 guard. Captured distinct 16-bit/32-bit item-ID and 20180511 reward-entry layout variants. The selected source configuration therefore has no reachable roulette request/emission path for these parents. Later-profile null-parser C2S inputs remain open, not semantically supported.

Classified the four parents inactive for the selected source config, preserving PK44 ownership. Resolved `0x0A22`'s S2C emitter identity while also retaining its separate C2S null-parser identity. Contract: `contracts/PK44.ROULETTE-MAP.md`; the current client/Ref searches found no matching roulette receivers. Source hashes match the hash-pinned baseline for packetdb/clif/layouts; atcommand/script hashes are recorded in the contract. No runtime code changed, no build/test was run, and no deployed-server match or independent review is claimed (review deferred to PK48). Stable subject: `packet(PK44.02.002): trace roulette direction and source-date gates`. Next: `PK44.02.003`.


## 2026-11-04 — PK44.02.003 achievement/date and feature gates

Traced the achievement and title registration block at `PACKETVER >= 20150513`, both directions for discovery parents `0x0A23`, `0x0A24`, and `0x0A26`, the title request/ack neighbors `0x0A2E/0x0A2F`, and the achievement inter-server load/reward paths. The default feature config enables achievements, but `battle.cpp` forcibly disables `feature_achievement` for `PACKETVER < 20150513`; the 20080910 profile therefore does not request achievement state from the character server or reach the feature's response-driven emitters. The title parser is absent and title ack sender has the same date guard.

Classified the three PK44 parents inactive for the selected source configuration, linking separate C2S null-parser framing and S2C output contracts for each reused numeric opcode. Recorded exact later-profile record layouts and found a source issue to retain open: `clif_achievement_update`'s null-achievement branch zeroes only 40 bytes starting at offset 16 of its 66-byte packet, leaving the final ten bytes unassigned. No exact fixture for that branch or runtime implementation is claimed. Client/Ref searches found no corresponding handlers. No runtime source changed, no build/test was run, deployed-server parity is not established, and independent review remains deferred to PK48. Stable subject: `packet(PK44.02.003): trace achievement date and feature gates`. Next: `PK44.02.004`.

## 2026-11-04 — PK44.02.004 attendance applicability and ownership

Traced the attendance request/reply family in the pinned server snapshot. The registrations for C2S `0x0AEF` and the reused numeric `0x0AF0` framing entry are guarded at `PACKETVER >= 20180307`. The S2C `0x0AF0` emitter writes a 10-byte response, while the same opcode has a separate null-parser C2S framing identity. Although `feature.attendance` is on in `feature.conf` and defaults on, `battle.cpp` forcibly disables it below 20180307; under selected 20080910 config, login UI opening and reward claiming cannot reach the attendance response path.

Classified the existing `AUDIT-0x0AF0` parent inactive for the selected source config, linked its C2S and S2C canonical identities, and preserved PK24 primary ownership with PK44 secondary review. `0x0AEF` is related later-profile input and has no discovery parent in the baseline inventory; no synthetic row was created. Evidence and source hashes: `contracts/PK44.ATTENDANCE-MAP.md`. No runtime code changed and no build/test was run; deployed-server provenance and later-profile client behavior remain open. Independent review is deferred to PK48. Stable subject: `packet(PK44.02.004): trace attendance date and ownership`. Next: `PK44.02.005`.

## 2026-11-04 — PK44.02.005 dressroom source-date guard

Traced `AUDIT-0x0A02` to the map-stage S2C dressroom-open notification. Its packed `PACKET_ZC_DRESSROOM_OPEN` layout is four bytes: signed 16-bit opcode at offset 0 and unsigned 16-bit view at offset 2; the sender sets view to zero (commented ignored). `clif_dressing_room` emits only for PACKETVER >= 20140212, and the only located caller, the `opendressroom` script builtin, is guarded at the same date and fails on older profiles. There is no inbound parser registration. The effective 20080910 source build cannot reach the emission path.

Classified `AUDIT-0x0A02` inactive for the selected source configuration and linked `PK44.DRESSROOM.0A02-S2C-OPEN`. Targeted client and Ref searches found no receive handler; that does not establish behavior for every historical executable. Contract and hashes: `contracts/PK44.DRESSROOM-MAP.md`. No runtime code changed, no build/test was run, and deployed-server provenance remains open. Independent review is deferred to PK48. Stable subject: `packet(PK44.02.005): trace dressroom source-date guard`. Next: `PK44.02.006`.

## 2026-11-04 — PK44.02.006 guild-storage log cross-review

Cross-reviewed `AUDIT-0x09DA`, preserving PK12 primary ownership and adding PK44 as a secondary reviewer. The packet is `ZC_ACK_GUILDSTORAGE_LOG`, an S2C usage-history response, not normal guild-storage contents or transfer traffic. Its source sender and `guildopenstorage_log` script entry both require PACKETVER >= 20140205, so the selected 20080910 configuration does not emit it. The response has an 8-byte header and variable records: 83 bytes with 16-bit item/card IDs, expanding to 93 bytes under the MAIN/RE/ZERO 2018 width gates. The entry fields, result branches, database call path, official-storage macro boundary, source hashes and client/Ref search limits are in `contracts/PK44.GUILD-STORAGE-LOG-MAP.md`.

Classified the existing parent inactive for the selected source configuration and linked `PK44.GSTORAGE.09DA-S2C-USAGE-LOG`; no ownership transfer was warranted. No runtime code changed, no build/test was run, and deployed-server provenance plus later-profile client behavior remain open. Independent review is deferred to PK48. Stable subject: `packet(PK44.02.006): trace guild storage log identity`. Next: `PK44.02.007`.

## 2026-11-04 — PK44.02.007 reputation identity cross-review

Cross-reviewed `AUDIT-0x0B8D`, preserving PK47 primary ownership and adding PK44 as a secondary reviewer. The packet has two source-confirmed S2C shapes: a variable full reputation list (`5 + 16*count` bytes) emitted on map-entry synchronization and a fixed one-entry 21-byte update emitted by `set_reputation_points`/`add_reputation_points`. Both packed senders write `success=true` and are guarded by RE >= 20211103 or MAIN >= 20220330; the selected MAIN 20080910 source cannot emit them. No inbound registration exists. Client/Ref searches found no receive route; later-profile list bounds, byte order and client behavior remain open. Contract and hashes: `contracts/PK44.REPUTATION-MAP.md`.

Classified the existing parent inactive for the selected source config and linked both shape-specific canonical IDs without changing its primary owner. No runtime source changed and no build/test was run; deployed-server provenance remains unknown. Independent review is deferred to PK48. Stable subject: `packet(PK44.02.007): trace reputation identity and guards`. Next: aggregate the seven completed children in `PK44.02`.

## 2026-11-04 — PK44.02 map-family aggregate

Closed the PK44.02 aggregate after verifying each of its seven child checkpoints is committed and its evidence contract exists. The children classified fourteen assigned discovery parents inactive for the selected PACKETVER=20080910 source configuration, including cross-reviews that preserve PK24 (`0x0AF0`), PK12 (`0x09DA`) and PK47 (`0x0B8D`) as primary owners. The source-backed row/contract mapping and unresolved limits are summarized in `evidence/PK44.02-map-family.md`. No implementation or deployed-server parity is claimed. Stable subject: `packet(PK44.02): aggregate map-family applicability evidence`. Next: `PK44.03.001`.

## 2026-11-04 — PK44.03.001 account login request registration trace

Traced all ten assigned login-stage C2S parents against the common packed structures and the server's `LoginPacketDatabase`. All ten forms are registered in `loginclif.cpp` without PACKETVER guards, so source-date alone cannot classify these request forms inactive for the selected 20080910 config. Captured fixed schemas for raw login `0x0064`, salt request `0x01DB`, MD5 login `0x01DD/0x01FA/0x027C`, ignored connect-info `0x0200`, executable hash `0x0204`, PCBang raw login `0x0277`, channel raw login `0x02B0`, and dynamic SSO `0x0825`. The `0x01DB` request's separate `0x01DC` salt reply belongs to PK44.03.002.

Only `0x0064` raw login and `0x0200` connect-info keepalive have verified Ref send sites and are classified C2S_ONLY. The other eight remain UNREVIEWED: several are registered server variants without a complete client negotiation/send path in the available Ref source. The MD5 response handler exists, but no `HEADER_CA_REQ_HASH` sender was found. The Ref `0x027C` builder leaves its declared MAC field unassigned, and the dynamic SSO packet-length path has a signed lower-bound concern requiring security-focused review. These exact questions are recorded in `contracts/PK44.LOGIN-REQUESTS.md` and `BLOCKED.md`; they block only those variants, not the next response-stage trace. No runtime code changed, no build/test was run, and deployed-server provenance remains unknown. Stable subject: `packet(PK44.03.001): map login request registrations and variants`. Next: `PK44.03.002`.


## 2026-11-04 — PK44.03.002 account authentication/status reply trace

Traced login-server S2C `0x0069` success, `0x006A` refusal, `0x0081` operational refusal/disconnect, and `0x01DC` salt response against packed declarations, senders, and current client handlers. Selected source layouts: `0x0069` 47-byte prefix plus 32-byte server records; `0x006A` 23 bytes; `0x0081` 3 bytes; `0x01DC` length-prefixed with a 20-byte salt (24 bytes total). `0x0069` success requires live char server, allowed group, and acceptable online-session state; `0x0081` result 1 and 8 represent operational refusal branches. `0x006A` source codes and password-ban behavior are recorded.

Corrected the PK44.01 provisional family routing: `0x006A` is emitted by the login server and now belongs to reply-stage `.002`, not character-stage `.004`; each parent remains routed once. Source comparison found current-client mismatches: accepted-login offset 12 is named/stored as userLevel though server sends login_id2, and refusal codes 1/3 do not match server meanings. Recorded these and hash-response profile behavior in `BLOCKED.md` for PK48. Coverage rows link canonical contracts but remain UNREVIEWED. No runtime code changed; no build/test was run. Deployed-server provenance and Ref behavior remain unresolved. Stable subject: `packet(PK44.03.002): trace account authentication replies`. Next: `PK44.03.003`.


## 2026-11-04 — PK44.03.003 raw character account-ID handshake trace

Traced the character-server `CA_ENTER` request and raw echo. Server waits for 17 bytes, reads account ID/login IDs/sex, consumes the request, and unconditionally emits four raw AID bytes before running-state/authentication decisions or normal framed responses. The echo has no opcode.

The current receive path interprets the first two bytes as an opcode. It consumes four bytes only for fake IDs `0x8482/0x8483`, silently without comparing the AID; other AIDs cause two-byte unknown-header drops and stream misalignment. Documented the handshake and concrete client gap in `contracts/PK44.CHAR-RAW-AID-HANDSHAKE.md`; retained PK04 as primary implementation owner and did not change discovery coverage. No runtime code changed; no build/test was run. Deployed-server provenance remains unknown. Stable subject: `packet(PK44.03.003): trace raw character account handshake`. Next: `PK44.03.004`.

## 2026-11-04 — PK03.03 lifecycle boundary

Rechecked PK03.03 against the current plan: the production action sender already covers profile-23 attack/repeat/sit/stand bytes, and the headless chooser replay drives production activation/mode-message handling with local-peer request-byte and send-result assertions. The remaining complete LoginMode-to-chooser-to-Session acknowledgement handoff, Qt input delivery, and visible UI rendering cannot be verified in the existing replay without initializing coupled audio/assets/window/world/renderer state. Recorded exact coverage and limits in `evidence/PK03.03-boundary.md`; marked only that boundary `BLOCKED` and advanced the active step to PK03.04, whose self-review/replay can proceed independently. No discovery rows were closed and no runtime code changed. Plan consistency validation and PK03.04 production replay are next.


## 2026-11-04 — PK44.03.004 character list, selection and entry trace

Traced ten assigned identities against character-server senders/parser registration and current client handlers. Selected legacy flow: fixed 3-byte C2S `0x0066` selects a slot; S2C `0x006B` is variable with a 24-byte pre-2010 prefix followed by character records; `0x006C` is a fixed 3-byte refusal; successful character selection yields a fixed 28-byte `0x0071` zone endpoint. Server validates slot, account ownership/deletion, bans/loading state and map availability; pre-2010 path falls back to an accessible major city, while successful map selection creates temporary auth state.

Separated later paths: `0x082D`/`0x09A0` are emitted only at PACKETVER >= 20130000; accessible-map `0x0840/0x0841` is guarded >= 20100714; `0x09A1` is registered without a date guard and requests paged variable `0x099D`, but client reachability is not established. Current client only has established receive registrations for legacy forms. Character-record historical layouts and later variants remain UNREVIEWED; blocker details are scoped to those implementations. Coverage links all ten rows to the source trace without closing them. No runtime code changed; no build/test was run; deployed-server and Ref behavior remain unresolved. Stable subject: `packet(PK44.03.004): trace character list and entry`. Next: `PK44.03.005`.


## 2026-11-04 — PK44.03.005 character creation trace and scope correction

Traced selected legacy creation C2S `0x0067` (fixed 37-byte form: 24-byte name, six one-byte stats, slot, hair color/style) and S2C `0x006D` accepted record / `0x006E` refusal. The current client sends the legacy layout and handles both responses. Server returns refusal codes 0=name collision, 255=creation disabled, 1=underage, and 3=ineligible slot; current UI's code 2/3/255 labels do not fully match and are documented for PK48. Newer create opcodes `0x0970/0x0A39` are profile variants outside assigned parent rows.

Corrected provisional scope after symbol evidence showed `0x01FB` is `CH_DELETE_CHAR`, while `0x0827`–`0x082C` are delete3 reserve/accept/cancel requests and replies. Moved all seven rows to PK44.03.006, revised its work-package action, and preserved unique parent ownership. This deletion family is not yet traced. No runtime code changed; no build/test was run; deployed-server provenance and complete character-record compatibility remain open. Stable subject: `packet(PK44.03.005): trace character creation`. Next: `PK44.03.006`.


## 2026-11-04 — PK44.03.006 character deletion transaction trace

Traced legacy request `0x01FB` (selected source date; fixed 56 bytes), success `0x006F` (2 bytes), refusal `0x0070` (3 bytes), and scheduled deletion reserve/confirm/cancel request-reply family `0x0827`–`0x082C` (fixed 6/14/12/10/6/10-byte forms). Recorded configured key checks, server status mappings, pending deletion timestamp semantics, party/guild restrictions, confirmation/cancel behavior and PACKETVER guards. All extended identities are registered in selected server source, but current-client path is unverified.

Found selected peer mismatch: active current client profile sends opcode `0x0068`, while the selected 20080910 server registers `0x01FB`; payload struct size is the same, but opcode identity differs. Recorded this and scheduled-delete reachability as variant-scoped PK48 follow-up. Corrected scope in prior `.005` commit moved `0x01FB`/`0x0827`–`0x082C` to deletion; each discovery row now links this contract and remains UNREVIEWED. No runtime code changed; no build/test was run. Deployed-server and interactive behavior remain open. Stable subject: `packet(PK44.03.006): trace character deletion flows`. Next: `PK44.03.007`.


## 2026-11-04 — PK44.03.007 character rename protocol trace

Traced selected legacy `0x028D` (34-byte account/CID/name validation request), `0x028E` (4-byte validity result), `0x028F` (6-byte CID commit request), and `0x0290` (4-byte rename result). The server validates AID/session ownership and name, saves normalized name in session state, then a second request commits it with configured rename-count, guild/party, duplicate-name and SQL checks. Newer request/reply `0x08FC/0x08FD` are later variants outside assigned rows.

Current client has no registered or dispatched path for any of the four packets. The server's second request carries only CID while the candidate name is session state from the prior request; ordering/lifetime and profile reachability are recorded for PK48. Rows link to the source trace but remain UNREVIEWED. No runtime code changed; no build/test was run; deployed/ref parity unknown. Stable subject: `packet(PK44.03.007): trace character rename protocol`. Next: `PK44.03.008`.


## 2026-11-04 — PK44.03.008 PIN applicability trace

Traced assigned character-server PIN identities `0x08B8`, `0x08B9`, `0x08BA`, `0x08BE`, and `0x08C5`, their packed fields and later-profile call semantics. The authoritative config header defines `PACKETVER_SUPPORTS_PINCODE` as `PACKETVER >= 20110309`; every handler registration and dispatch gate is inside this macro. Effective selected PACKETVER=20080910 compiles the transaction out, independent of runtime PIN settings. All five discovery rows are now `INACTIVE_FOR_SELECTED_SOURCE_CONFIG` and link the applicability contract. The later-profile enum contradicts a stale nearby code comment; table uses the enum/call sites. No runtime changes/build/tests. Deployed-server provenance and later profile security remain outside this step. Stable subject: `packet(PK44.03.008): trace PIN profile guards`. Next: `PK44.03.009`.


## 2026-11-04 — PK44.03.009 character-slot move trace

Traced unconditionally registered C2S `0x08D4` (8 bytes: before slot, after slot, client remaining count) and selected S2C `0x08D5` (8 bytes: length, result, remaining moves; newer profile uses `0x0B70`). Server validates feature/count/source/destination limits, updates or swaps SQL slots subject to config, decrements finite move count and resends character list on success. Current client has no sender or response registration; rows remain UNREVIEWED.

Found concrete malformed-input issue: when client `slot_before >= MAX_CHARS`, server calls reply helper with that index, and helper reads `sd.char_moves[index]` unchecked. Recorded as PK48 security gate before hostile-input replay or runtime implementation. No runtime code changed; no build/test run; deployed-server reachability unknown. Stable subject: `packet(PK44.03.009): trace character slot move`. Next: `PK44.03.010`.


## 2026-11-04 — PK44.03.010 character block/status applicability trace

Traced S2C variable `0x020D`: 4-byte header followed by 24-byte CID/unblock-time records, normally sent after char-list and repeated every 10 seconds while a future block remains. The selected `PACKETVER_SAK_NUM=20080910` defines `PACKETVER_SAK_LEGACY_CHARINFO`; the sender returns immediately under that macro before packet construction, sending, expiry cleanup or timer scheduling. Classified the discovery row `INACTIVE_FOR_SELECTED_SOURCE_CONFIG`; deployed provenance remains unknown. No runtime changes/build/tests. Stable subject: `packet(PK44.03.010): trace character block status`. Next: aggregate PK44.03 after checking all ten child checkpoints.


## 2026-11-04 — PK44.03 login/character family aggregate

Verified all ten numbered research children are COMPLETED in `state.json`, their evidence paths exist, and each stable commit subject is present in Git history. Reconciled 48 unique login/character discovery parents across children after the `0x006A` login-stage and deletion-family routing corrections. Coverage now has 2 C2S_ONLY (`0x0064`,`0x0200`), 6 source-config-inactive (five PIN identities and `0x020D`), and 40 UNREVIEWED; no rows were closed. PK44.02/03 total provisional primary ownership remains 59.

Aggregate evidence: `evidence/PK44.03-login-character-family.md`. Scope-specific client mismatches and the slot-move unchecked array read remain explicit for PK48; raw handshake implementation stays PK04-owned. No runtime code changed or replay/build was run. Stable subject: `packet(PK44.03): aggregate login and character traces`. Next: `PK44.04` self-review and production replay.


## 2026-11-04 — PK44.04 self-review correction

While beginning the PK44.04 source/field parity review, traced the full accepted-login value path: `OnAcceptLogin` reads bytes 12–15 into a member named `m_userLevel`; `ConnectChar` copies those bytes unchanged into `PACKET_CA_ENTER.UserLevel` at bytes 10–13; the server reads bytes 10–13 as `login_id2`. Earlier `.002`/aggregate/STATUS/BLOCKED notes incorrectly described this as a wire semantic mismatch. Corrected those current-state artifacts and retained only a naming observation. Confirmed the `0x006A` refusal mapping is materially shifted for codes 0–4 and remains an open result/UI issue; 01DC hash-order profile behavior is also unresolved.

PK44.04 remains IN_PROGRESS: it calls for production replay and UI lifecycle checks across a broad set of 48 parents, several with unsafe or unverified paths. The available PK03 replay covers transport/lifecycle behavior, not these PK44 packet contracts. No PK44-specific independent byte fixtures or bounded implementation leaves exist yet, so that replay cannot honestly close the requested gate. Preserve this scope for subsequent bounded implementation/replay work and PK48 review; continue only with independent plan work when dependencies permit. No runtime changes or tests were run in this self-review checkpoint.


PK44.04 self-review checkpoint (2026-11-04) is recorded in `evidence/PK44.04-self-review.md`; its status is IN_PROGRESS. The 0x0069 `userLevel` concern was only a local naming mismatch: those bytes are copied unchanged into the character-enter login_id2 slot. Corrected the live contract/aggregate/blocker/status; historical progress remains annotated with this correction. The 0x006A code-map mismatch remains confirmed. PK44.04 still lacks PK44-specific server-derived fixtures, semantic production replay, bounded implementation leaves and applicable UI lifecycle evidence, so it cannot honestly close this checkpoint. PK47 is independently eligible from completed PK01/PK02; next: PK47.01.


## 2026-11-04 — PK47.01 residual/historical review specification

Specified authoritative inputs, baseline/hash provenance, canonical identity, row dispositions, historical-ledger schema, per-row evidence thresholds and the boundary between 990 discovery rows and 1,384 historical definitions. Split the original broad PK47.02 into permanent `.001` discovery-ledger reconciliation and `.002` historical-catalog reconciliation children because their keys and proof requirements differ. No classifications or runtime code changed. Stable subject: `packet(PK47.01): specify residual review scope`. Next: `PK47.02.001`.


## 2026-11-04 — PK47.02.001 discovery ownership and evidence-link reconciliation

Reconciled all 990 unique discovery IDs against primary/secondary owner assignments, canonical contract links, owner step statuses, assignment-basis file references and existing classifications. The ledger contains 990 rows across 43 primary owners. It reports 62 rows with resolvable contract links and 928 without canonical links; the latter remain open for their existing owners. All 48 assignment-basis file references resolve. Existing classifications remain 968 UNREVIEWED, 20 INACTIVE_FOR_SELECTED_SOURCE_CONFIG and 2 C2S_ONLY; no row was closed, and no runtime behavior was inferred. Per-owner status summaries count all numbered package steps. Evidence: `evidence/PK47.02.001-discovery-reconciliation.md` and `.csv`. Plan metadata check and whitespace check passed. Stable subject: `packet(PK47.02.001): reconcile discovery ownership and evidence links`. Next: `PK47.02.002` historical-catalog reconciliation.


## 2026-11-04 — PK47.02.002 historical catalog reconciliation

Crosswalked all 1,384 unique historical opcode rows against the 990-row discovery union, selected final registry, and current inventory. 939 catalog rows map to a discovery parent; 445 sit outside the union and have no discovery owner. Registry membership agrees with the catalog flag for all 1,384 rows: 596 present and 788 absent. The catalog groups multiple definition names for 105 opcodes without per-name profile dates, so all entries retain UNRESOLVED disposition; no absent row was called inactive and no discovery rows were manufactured. Evidence: `evidence/PK47.02.002-historical-reconciliation.md` and `.csv`. No runtime code changed. Stable subject: `packet(PK47.02.002): reconcile historical catalog identities`. Next: aggregate both PK47.02 child ledgers in `PK47.02`.


## 2026-11-04 — PK47.02 reconciliation aggregate

Verified the child ledgers reconcile exactly: 990 unique discovery IDs and 1,384 unique historical opcode IDs. Of the historical rows, 939 map to discovery parents and 445 remain outside the union without discovery-package owners; PK47 retains historical-review ownership. All 1,384 catalog registry flags match selected final registry membership (596 present, 788 absent), but all historical rows remain UNRESOLVED. Discovery classifications were preserved (968 UNREVIEWED, 20 INACTIVE_FOR_SELECTED_SOURCE_CONFIG, 2 C2S_ONLY); 62 have canonical contract links and 928 remain unlinked/open under existing owners. No runtime source changed. Evidence: `evidence/PK47.02-aggregate.md`. Validation: exact key-set checks, `python tools/check_packet_plan.py`, and `git diff --check`. Stable subject: `packet(PK47.02): aggregate discovery and historical reconciliations`. Next: `PK47.03` source-backed adjudication.


## 2026-11-04 — PK47.03.001 guarded map registration audit

Corrected the catalog provenance: `historical-catalog.csv` is generated by scanning four packet-definition files in one rAthena source snapshot (including profile-guarded definitions), not by merging multiple server revisions. Among 445 catalog-only opcodes outside the 990 discovery union, 232 have 297 active source lines in `clif_packetdb.hpp`; all have explicit preprocessor condition context and none appears in the selected final registry. Of the registrations, 212 use `parseable_packet` with a named parser and 85 use `packet` with no parser; one opcode has both. All 232 remain `UNRESOLVED` because packet identity, handler semantics and client sender paths are not established by registration evidence. Full row ledger and hashes are in `evidence/PK47.03.001-guarded-map-registration-audit.md` and `.csv`. No runtime code changed. Validation rechecked all 297 referenced source lines and selected-registry absences, plus `python tools/check_packet_plan.py` and `git diff --check`. Stable subject: `packet(PK47.03.001): trace guarded catalog-only map registrations`. Next: continue PK47.03 source adjudication for the remaining 213 catalog-only entries without active map packetdb lines.


## 2026-11-04 — PK47.03.002 correct comment-derived catalog entries

Found four comment-only false positives in the prior historical catalog scan. Updated `tools/audit_packets.py` to mask comments/string literals and renamed the field to `definitions_in_source_snapshot`, accurately reflecting one version-gated source snapshot. Removed `0x0086` (block-commented in `packets.hpp`) and `0x0926`, `0x092E`, `0x095A` (line-commented registrations). Regenerated the catalog to 1,380 unique source opcodes and the historical crosswalk to 1,380 rows: 938 parents, 442 outside the 990 union, 596 registry-present, 784 absent, 0 membership mismatches. Updated the PK47.03.001 ledger count to 234 opcodes / 299 guarded registrations; 208 catalog-only source declarations remain open. No semantic rows were closed and no runtime source changed. Evidence: `evidence/PK47.03.002-catalog-comment-scan.md`. Stable subject: `packet(PK47.03.002): exclude comments from packet definition catalog`. Next: record the remaining PK47.03 source-path review limits and resume the next eligible implementation step.


## 2026-11-04 — PK47.03.002 comment-filtered catalog and crosswalk

Completed the snapshot-local catalog correction. The prior scanner matched four comment-only occurrences; `audit_packets.py` now masks comments and string literals and emits `definitions_in_source_snapshot`. Rebuilt the catalog and per-opcode crosswalk: 1,380 source opcodes, 938 mapped to discovery IDs, 442 without discovery parents; selected registry membership is 596 present / 784 absent with zero mismatches. PK47.03.001 is reconciled to 234 catalog-only opcodes and 299 guarded registrations; 208 other catalog-only declarations remain. The 234 registration rows and 208 declaration rows stay unresolved pending full parser/emitter/client-path traces; their scope is documented in `BLOCKED.md` for a later stronger review. No semantic classification was closed; no runtime code changed. Evidence: `evidence/PK47.03.002-catalog-comment-scan.md`. Validation: source-row/key checks, `python tools/check_packet_plan.py`, and `git diff --check`. Stable subject: `packet(PK47.03.002): exclude comments from packet definition catalog`. Next eligible plan step: resume `PK03.03`; PK04 depends on completion of PK03.

## 2026-11-04 — PK03.04 deploy-off replay rerun

Both `packet_stream_replay_checks` and `open_midgard` build targets succeeded; CTest initially could not open the local mock-peer path in the default sandbox, then passed 1/1 with local-loopback permission (2.33s). Verbose output confirmed PASS for framing/router/refusal, the 0x0437 sender, chooser activation, timeout and LoginMode cases. The replay executable hash matches the previously recorded binary and its timestamp follows the replay source timestamp; no runtime source changed. Updated `evidence/PK03.04-replay-audit.md`; full application update/init/exit frames and visible Qt rendering remain open. Stable subject: `packet(PK03.04): rerun deploy-off lifecycle replay`.

## 2026-11-04 — PK47.03 source-adjudication aggregate

Aggregated the completed PK47.03.001 guarded-registration scan and PK47.03.002 comment-filtered catalog correction. The corrected catalog has 1,380 identities; 234 catalog-only opcodes have 299 guarded map registrations, and another 208 have declarations without a matched active registration. All 442 identities remain `UNRESOLVED`; no discovery rows or semantic findings were closed. Added `evidence/PK47.03-aggregate.md` with counts, evidence links, source limitations, and validation scope. The PK03 lifecycle checks remain blocked at the full application/visible Qt boundary, while independent PK47 work continues. Stable subject: `packet(PK47.03): aggregate historical identity adjudication`. Next: PK47.04 self-review.

## 2026-11-04 — PK47.04 per-identity evidence boundary

Self-reviewed the PK47 aggregate against its two source-audit children and the corrected catalog. Aggregate counts reconcile, but registration/declaration inventories do not establish per-identity client reachability, parser/emitter semantics or byte oracles. The existing PK03 replay is unrelated to those packet identities. Recorded the per-identity work needed in `evidence/PK47.04-boundary.md`, kept all 442 catalog-only identities unresolved, and moved to the dependency-ready PK44.04 self-review. This is an evidence boundary, not a strong-agent approval gate. Next: PK44.04.

PK44.04 added a 79-byte independent `0x0069` AC_ACCEPT_LOGIN fixture and invoked production `CLoginMode::OnAcceptLogin`. The replay verified session identifiers, regional-sex normalization, all fields of one 32-byte character-server record, account-handshake reset, accepted status, first-server selection and transition to ConnectChar. A fresh deploy-off build succeeded and focused CTest passed 1/1 in 2.42s with local-loopback permission. The replay executable is SHA-256 `CA91EA857453126BEA8EC9F3A63A6C8BBDEA5B97D81D5B1B58B331150360D213` (19,774,136 bytes, `2026-11-04T02:23:44Z`); the existing `open-midgard.exe` is SHA-256 `FD3DC6A4BFAC43183DC2B938E07733693CDD05EA07F484F65F7EC33CAA8E8ABB` (19,664,043 bytes, `2026-11-04T02:11:27Z`). This direct handler test does not exercise framing/routing, ConnectChar socket connection, or visible UI; no discovery rows closed. Evidence: `evidence/PK44.04-login-accept-reply.md` and `.jsonl`. Next: continue the PK44.04 source-backed branch review and identify the next bounded response handler.

PK44.04 compared the selected rAthena 20080910 `CHARACTER_INFO` (size 108; `CharNum` u16@104, `hairColor` s16@106) with the current client's same-size but different terminal fields. Replaced the incorrect direct copies with an explicit layout conversion for pre-20090225 login dates in `0x006B` character-list and `0x006D` character-create success handlers. Two independent sentinel fixtures invoked the production handlers and verified slot 7, hair color 18, character selection transition and status. Build targets `packet_stream_replay_checks` and `open_midgard` passed; focused CTest passed 1/1 in 2.40s with local-loopback permission. Replay executable SHA-256 `450D469C17994C9DBEECBF35A60055D3FF3107D8B69BE86C7F50C5FCD71F0075` (19,778,798 bytes, `2026-11-04T02:32:52Z`); client executable SHA-256 `316BBEA7E640D8BEC37F00D4D7CF8371DF8813C10B880DEE522F30B3D70874E9` (19,665,067 bytes, `2026-11-04T02:32:55Z`). Direct handler calls do not verify framing/router dispatch or visible UI; other historical record layouts remain unverified. No discovery rows were closed. Evidence: `evidence/PK44.04-character-record-conversion.md` and `.jsonl`. Stable subject: `packet(PK44.04): translate legacy character reply fields`. Next: continue source-backed login/character branch review.

PK44.04 traced selected-source `0x006C` refusal sender `chclif_reject`, which zero-initializes the fixed three-byte packet and emits result 0 on the inspected rejection branches. A local-loopback replay invoked production `CLoginMode::OnRefuseChar` and verified the status, active connection closure, connected-flag reset and return-to-Login transition. The visible modal remains unverified. Focused CTest passed 1/1 in 2.46s; replay executable SHA-256 `12A07B2AE5B16784D18E50599F00F892BE0AE9B28E159C6AEFA2D3F7BD4DFDF4` (19,780,897 bytes, `2026-11-04T02:35:33Z`). The client executable remained at SHA-256 `316BBEA7E640D8BEC37F00D4D7CF8371DF8813C10B880DEE522F30B3D70874E9` (19,665,067 bytes, `2026-11-04T02:32:55Z`); this test-only change did not relink it. No discovery rows were closed. Evidence: `evidence/PK44.04-character-refusal.md` and `.jsonl`. Stable subject: `packet(PK44.04): verify character refusal cleanup`.

PK44.04 traced selected-source `0x0071` layout and sender byte order, then replayed the 28-byte fixture through production `CLoginMode::OnNotifyZonesvr` with a local char-server socket. Assertions cover character ID, `prontera.gat` normalization, 10.0.0.1/6121 endpoint, stale guild/window reset, char-server close, connected-state reset and ZoneConnect transition. Focused CTest passed 1/1 in 2.49s; replay executable SHA-256 `B5D611060C9A91568B733C40C0FA7916CD5B7496160BDE8DDAAFB44298F0C112` (19,793,661 bytes, `2026-11-04T02:37:52Z`); client executable remained at SHA-256 `316BBEA7E640D8BEC37F00D4D7CF8371DF8813C10B880DEE522F30B3D70874E9` (19,665,067 bytes, `2026-11-04T02:32:55Z`). Visible zone UI and map-server connection remain unverified; no discovery rows were closed. Evidence: `evidence/PK44.04-zone-notification.md` and `.jsonl`. Stable subject: `packet(PK44.04): replay zone-server notification`.

PK44.04 compared `0x006E` with `chclif_createnewchar_refuse`: selected-source results 0/1/3/255 mean name exists, underage, invalid slot and creation denied; result 2 is explicitly unused and result 11 is unassigned. Replaced guessed meanings for 2 and 11 with an unrecognized-code fallback and mapped 255 explicitly. The production handler replay checked all four emitted values, both unassigned fallbacks, and return to CharSelect. Fresh deploy-off build passed and focused CTest passed 1/1 in 2.50s. Replay executable SHA-256 `A5A3F2CCC32CAB9D05FB5ED6C3FA5E8E35FC561D632B275D44B971860D5E1728` (19,795,610 bytes, `2026-11-04T02:39:58Z`); client executable SHA-256 `B8AFDC9B89C79D2F378777585A236E3D8D877E85E9189A3902391D0D8E46B999` (19,664,555 bytes, `2026-11-04T02:40:00Z`). Visible modal remains unverified, and no discovery rows were closed. Evidence: `evidence/PK44.04-character-create-refusal.md` and `.jsonl`. Stable subject: `packet(PK44.04): correct character create refusal results`.

Supplementary PK44.04 `0x006B` coverage: the selected server emits a valid 24-byte header when the account has no characters. Added a direct production-handler fixture asserting zero character records, no-character status, and transition to CharSelect. The full deploy-off build passed; focused CTest passed 1/1 in 2.52s. Replay executable SHA-256 `DE2E27BC82E3C92F23A05E502E5F51AA34AD589A68668312F58776BE12731BBF` (19,796,634 bytes, `2026-11-04T02:42:04Z`); current client executable SHA-256 `B8AFDC9B89C79D2F378777585A236E3D8D877E85E9189A3902391D0D8E46B999` (19,664,555 bytes, `2026-11-04T02:40:00Z`). Evidence `evidence/PK44.04-character-record-conversion.jsonl` now contains populated list, empty list, and create-success records. No discovery row was closed. Stable subject: `packet(PK44.04): cover empty character list`.

## 2026-11-04 — PK44.04 route character refusal through receive loop

Extended the selected-source `0x006C` result-0 replay to send its 3-byte frame over the loopback socket and exercise production `CRagConnection::RecvPacket -> CLoginMode::PollNetwork -> OnRefuseChar`. The receive path framed and dispatched the packet; assertions still verify refusal status, connection closure, connected-flag reset and Login transition. Fresh deploy-off build passed; focused CTest passed 1/1 in 2.51s with local-loopback permission. Replay executable SHA-256 `2204F47D924D2ED3B9642FF468BF414A43FB0A5859365A971BF9680F537CAF33` (19,797,146 bytes, `2026-11-04T02:48:01Z`); client executable remained `B8AFDC9B89C79D2F378777585A236E3D8D877E85E9189A3902391D0D8E46B999` (19,664,555 bytes, `2026-11-04T02:40:00Z`). Visible modal rendering and other reply routes remain open; no discovery rows were closed. Stable subject: `packet(PK44.04): route character refusal through receive loop`.

## 2026-11-04 — PK44.04 correct login refusal results

Implemented the source-backed `0x006A` result mapping for codes 0–4 in the production `OnRefuseLogin` formatter. Added replay inputs using the selected 23-byte packet layout; the same production helper now verifies the five meanings, retains code 6's empty-date fallback, and preserves numeric output for unknown codes. Updated the login-replies contract, blocker/status records and PK44.04 evidence. No discovery rows were closed; codes 7+ user text, full modal/UI path and `0x01DC` hash behavior remain open. Validation: `cmake --build build-mingw-preset --target packet_stream_replay_checks ro_gamemode open_midgard --parallel 4` passed; CTest passed 1/1 with local-loopback permission. Stable subject: `packet(PK44.04): correct login refusal result labels`.

## 2026-11-04 — PK44.04 password hash challenge replay

Traced `0x01DC` emission and `OnAckHash` digest ordering. Both `g_passwordEncrypt2` concatenation branches match Ref. The production replay captures `0x01DB` from `SendPasswordHashRequest`, then uses a deterministic 20-byte salt, both settings, expected MD5 values, 47-byte `0x01DD` captures and the selected 20080910 client date. The production helper bytes and digest branches are verified; the profile gate and intended external clientinfo settings remain unresolved. Evidence: `evidence/PK44.04-login-hash-challenge.md` and `.jsonl`. The final deploy-off build passed; CTest passed 1/1 in 2.40s with local-loopback permission. No discovery rows closed. Stable subject: `packet(PK44.04): replay login hash challenge modes`.

## 2026-11-04 — PK44.04 source-backed login refusal results

Extended the `0x006A` formatter/replay from codes 0–4 to all source-documented result identities: 0–15 and 99–104. For 9–15 and 99–104, the selected source's default English `login_msg.conf` provided labels alongside `loginclif.cpp` result comments; codes without selected-source refusal meanings retain numeric fallback. Added 20 mapped-result assertions using 23-byte layouts, plus code 6's empty unblock-time fallback and code 16 numeric fallback. The production handler is also exercised for code 1, checking status and cleanup. Fresh deploy-off build passed; focused replay passed 1/1 in 2.39s with local-loopback permission. Custom deployed-server message configuration and modal/UI rendering remain unverified; no discovery rows were closed. Stable subject: `packet(PK44.04): cover documented login refusal results`.

## 2026-11-04 — PK44.04 operational login disconnect results

Traced selected-server `0x0081` send sites: result 1 represents closed/restricted login service or unavailable character server; result 8 means the previous session is still online. The documented result 2 has no active sender call site in this source snapshot. Routed `OnDisconnectMsg` text through a shared formatter, replayed emitted results 1/8 and preserved numeric fallback for result 2. The fresh deploy-off build passed; the final combined focused CTest passed 1/1 in 2.39s with local-loopback permission. Production-handler assertions cover status, handshake reset, return to Login, and connection close; visible modal rendering remains unverified because the replay uses AppMain_stub. No discovery rows were closed. Evidence: `evidence/PK44.04-login-disconnect-mapping.md` and `.jsonl`. Stable subject: `packet(PK44.04): map login refusal and disconnect results`.

## 2026-11-04 — PK44.04 route character list through receive loop

Changed populated and empty selected-source `0x006B` replay frames to travel through `CRagConnection::RecvPacket -> CLoginMode::PollNetwork -> OnAcceptChar`. Existing field-conversion, no-character status and CharSelect assertions pass through production framing and dispatch. The `0x006D` creation fixture remains a direct-handler call. Build targets were already fresh from the preceding replay implementation; focused CTest was rerun and passed 1/1 in 2.51s with local-loopback permission. Replay executable SHA-256 `804247931D11AE1DE78EDADD340408130EB48ADD8DC1F83CDAB13A29E17F4FD4` (19,797,790 bytes, `2026-11-04T02:49:21Z`); `open-midgard.exe` remains SHA-256 `B8AFDC9B89C79D2F378777585A236E3D8D877E85E9189A3902391D0D8E46B999` (19,664,555 bytes, `2026-11-04T02:40:00Z`). The visible character UI and other record profiles remain unverified; no discovery rows were closed. Stable subject: `packet(PK44.04): route character list through receive loop`. Next: route `0x006D` creation acceptance through production framing and routing.

## 2026-11-04 — PK44.04 route character creation acceptance through receive loop

Extended the character-record replay so the selected-source 110-byte `0x006D` creation acceptance travels through `CRagConnection::RecvPacket -> CLoginMode::PollNetwork -> OnAcceptMakeChar`, alongside the existing populated and empty `0x006B` replies. Slot/hair-color conversion, creation status, and CharSelect transition passed. Fresh deploy-off replay and client targets built; focused CTest passed 1/1 in 2.52s with local-loopback permission. Replay executable SHA-256 `40CF4B7380982D15CD3E1006F9810BB8423CB94A4F9F7BD358CE12CDEF2BBBBC` (19,797,790 bytes, `2026-11-04T03:38:08Z`); `open-midgard.exe` remains SHA-256 `B8AFDC9B89C79D2F378777585A236E3D8D877E85E9189A3902391D0D8E46B999` (19,664,555 bytes, `2026-11-04T02:40:00Z`). Visible character UI and other record profiles remain unverified; no discovery rows were closed. Stable subject: `packet(PK44.04): route character creation through receive loop`. Next: route `0x006A` login refusal through production framing/routing.

## 2026-11-04 — PK44.04 route login refusal through receive loop

The selected-source 23-byte `0x006A` code 1 rejection now travels through `CRagConnection::RecvPacket -> CLoginMode::PollNetwork -> OnRefuseLogin`. Production dispatch, mapped status, handshake cleanup, return to Login and socket close passed. Existing independent formatter fixtures retain selected-source outcomes for codes 0–15 and 99–104 plus numeric fallbacks. Fresh deploy-off replay and client targets built; focused CTest passed 1/1 in 2.54s with local-loopback permission. Replay executable SHA-256 `70C36EDEE1454BED66F10F332105A413C52953D5410569FE8E921F5B449FAD67` (19,798,302 bytes, `2026-11-04T03:39:46Z`); `open-midgard.exe` remains SHA-256 `B8AFDC9B89C79D2F378777585A236E3D8D877E85E9189A3902391D0D8E46B999` (19,664,555 bytes, `2026-11-04T02:40:00Z`). Visible modal rendering and discovery classification remain open. Stable subject: `packet(PK44.04): route login refusal through receive loop`. Next: route `0x0069` login acceptance through production framing/routing.

## 2026-11-04 — PK44.04 route login acceptance through receive loop

The selected-source 79-byte one-server `0x0069` acceptance now travels through `CRagConnection::RecvPacket -> CLoginMode::PollNetwork -> OnAcceptLogin`. Session identifiers, normalized sex, complete server record, account-handshake reset, acceptance status and ConnectChar transition passed. The replay does not attempt the subsequent char-server connection or visible UI. Fresh deploy-off replay and client targets built; focused CTest passed 1/1 in 2.49s with local-loopback permission. Replay executable SHA-256 `F2653746E6631CB3B7B5CFC01DEDF6D547950D02680EE2DF0A508332C533577C` (19,798,302 bytes, `2026-11-04T03:41:18Z`); `open-midgard.exe` remains SHA-256 `B8AFDC9B89C79D2F378777585A236E3D8D877E85E9189A3902391D0D8E46B999` (19,664,555 bytes, `2026-11-04T02:40:00Z`). No discovery rows were closed. Stable subject: `packet(PK44.04): route login acceptance through receive loop`. Next: route `0x0081` disconnect result through production framing/routing.

## 2026-11-04 — PK44.04 route login disconnect results through receive loop

Selected-source `0x0081` result 1 and 8 packets now travel through `CRagConnection::RecvPacket -> CLoginMode::PollNetwork -> OnDisconnectMsg`. The production replay verified the mapped status text, handshake reset, return to Login and socket close for both outcomes; code 2 remains a numeric fallback because the selected server has no active sender call site. Fresh deploy-off replay and client targets built; focused CTest passed 1/1 in 2.52s with local-loopback permission. Replay executable SHA-256 `AFEE234B417B2314A5B6B0F09FDC6C137DAF8B112CB21FA7F84022A4E706B521` (19,798,814 bytes, `2026-11-04T03:42:36Z`); `open-midgard.exe` remains SHA-256 `B8AFDC9B89C79D2F378777585A236E3D8D877E85E9189A3902391D0D8E46B999` (19,664,555 bytes, `2026-11-04T02:40:00Z`). Visible modal rendering and discovery classification remain open. Stable subject: `packet(PK44.04): route login disconnect through receive loop`. Next: route `0x006E` refusal results through production framing/routing.

## 2026-11-04 — PK44.04 route character-create refusal results through receive loop

All six selected-source `0x006E` result fixtures (0, 1, 3, 255, and unassigned 2/11) now travel through `CRagConnection::RecvPacket -> CLoginMode::PollNetwork -> OnRefuseMakeChar`. Source-mapped messages/fallbacks and return to CharSelect passed. Fresh deploy-off replay and client targets built; focused CTest passed 1/1 in 2.56s with local-loopback permission. Replay executable SHA-256 `63B69ED2EBC919B3DCA123FCB080923162E84F55DA5FF7146B777D68C63702A6` (19,799,326 bytes, `2026-11-04T03:43:55Z`); `open-midgard.exe` remains SHA-256 `B8AFDC9B89C79D2F378777585A236E3D8D877E85E9189A3902391D0D8E46B999` (19,664,555 bytes, `2026-11-04T02:40:00Z`). Visible modal rendering and unknown result semantics remain open. Stable subject: `packet(PK44.04): route remaining login-character replies`. Next: route `0x0071` zone notification through production framing/routing.

## 2026-11-04 — PK44.04 route zone notification through receive loop

The fixed 28-byte selected-source `0x0071` frame now travels through `CRagConnection::RecvPacket -> CLoginMode::PollNetwork -> OnNotifyZonesvr`. Character/map fields, IP/port conversion, local guild/window reset, char-server close and ZoneConnect transition passed. Fresh deploy-off replay and client targets built; focused CTest passed 1/1 in 2.52s with local-loopback permission. Replay executable SHA-256 `015F3014CF65047C1BA9E11CBBC03142AF96C94F75516A8C0294BFB83D78B04D` (19,799,447 bytes, `2026-11-04T03:44:56Z`); `open-midgard.exe` remains SHA-256 `B8AFDC9B89C79D2F378777585A236E3D8D877E85E9189A3902391D0D8E46B999` (19,664,555 bytes, `2026-11-04T02:40:00Z`). Follow-on map-server connection and visible zone UI remain open. Stable subject: `packet(PK44.04): route remaining login-character replies`. Next: route `0x01DC` challenge reception through `PollNetwork`.

## 2026-11-04 — PK44.04 route hash challenge through receive loop

The synthetic 24-byte selected-source `0x01DC` challenge now travels through `CRagConnection::RecvPacket -> CLoginMode::PollNetwork -> OnAckHash`; production captures still confirm the `0x01DB` helper request and both 47-byte `0x01DD` digest modes. The request gate and real challenge reachability remain unresolved. Fresh deploy-off replay and client targets built; focused CTest passed 1/1 in 2.56s with local-loopback permission. Replay executable SHA-256 `885008D1968ED2DAB45AB81C4093C427179EA1387ED52EAB5996A06355A33C20` (19,799,447 bytes, `2026-11-04T03:46:41Z`); `open-midgard.exe` remains SHA-256 `B8AFDC9B89C79D2F378777585A236E3D8D877E85E9189A3902391D0D8E46B999` (19,664,555 bytes, `2026-11-04T02:40:00Z`). No discovery rows were closed. Stable subject: `packet(PK44.04): route hash challenge through receive loop`. Next: review deletion-reply reachability, record the opcode mismatch, then continue with an applicable outbound/login action.

## 2026-11-04 — PK44.04 capture character selection request

Exercised the selected-source `0x0066` user-message path: `CLoginMode::SendMsg(LoginMsg_SelectCharacter)` accepted a seeded slot-7 character, queued `LoginSubMode_SelectChar`, and `OnChangeState(SelectChar)` sent the independent three-byte fixture `66 00 07`. This is production sender evidence; the full `OnRun` frame and Qt click/rendering remain open. Fresh deploy-off replay and client targets built; focused CTest passed 1/1 in 2.59s with local-loopback permission. Replay executable SHA-256 `CCF78BE0D7EA5E2B099CCC00FF6A9184F5F623D0829D4A4C80C70CCCD9567ADF` (19,801,552 bytes, `2026-11-04T03:49:40Z`); `open-midgard.exe` remains SHA-256 `B8AFDC9B89C79D2F378777585A236E3D8D877E85E9189A3902391D0D8E46B999` (19,664,555 bytes, `2026-11-04T02:40:00Z`). No discovery rows were closed. Evidence: `evidence/PK44.04-character-selection-request.md` and `.jsonl`. Stable subject: `packet(PK44.04): capture character selection request`. Next: capture `0x0067` character creation request from the production message/sender path.

## 2026-11-04 — PK44.04 capture character creation request

Drove the production character-creation message cases for slot 7, name `RuneA`, stats 1–6, hair color 18, and hair style 42. `CRagConnection::SendPacket` emitted the selected-source 37-byte `0x0067` fixture exactly, including the 16-bit tail fields; the request status also matched. Fresh deploy-off replay and client targets built; focused CTest passed 1/1 in 2.57s with local-loopback permission. Replay executable SHA-256 `D88D1A27D7EF65ECD636019BA9033BCEC22AC2293C3CE647A28D10482BE6AE66` (19,804,681 bytes, `2026-11-04T03:51:17Z`); `open-midgard.exe` remains SHA-256 `B8AFDC9B89C79D2F378777585A236E3D8D877E85E9189A3902391D0D8E46B999` (19,664,555 bytes, `2026-11-04T02:40:00Z`). Qt click/rendering and no discovery-row closure remain explicit. Evidence: `evidence/PK44.04-character-create-request.md` and `.jsonl`. Stable subject: `packet(PK44.04): capture character creation request`. Next: trace and replay reachable `0x0064` account-login request construction.

## 2026-11-04 — PK44.04 capture legacy account-login request

Set a synthetic America service profile to select the active legacy `0x0064` branch, populated credentials through the production message setters, and captured `SendPlainAccountLogin` over a local mock. The independent 55-byte fixture matched opcode, date 20080910, zero-padded username/password fields and client type 1. This does not test the ConnectAccount gate or establish deployed profile intent. Fresh deploy-off replay and client targets built; focused CTest passed 1/1 in 2.62s with local-loopback permission. Replay executable SHA-256 `BF211BD6567194B075A02E65797E3EFB2D2BCAA0CEFE5EAC4A91C9975CB24C72` (19,806,778 bytes, `2026-11-04T03:53:57Z`); `open-midgard.exe` remains SHA-256 `B8AFDC9B89C79D2F378777585A236E3D8D877E85E9189A3902391D0D8E46B999` (19,664,555 bytes, `2026-11-04T02:40:00Z`). No discovery rows were closed. Evidence: `evidence/PK44.04-account-login-request.md` and `.jsonl`. Stable subject: `packet(PK44.04): capture legacy account login request`. Next: inspect the `0x0200` send gate and reachability.

## 2026-11-04 — PK44.04 verify connect-info request gate

Drove both branches of `OnChangeState(ConnectAccount)` with an isolated temporary INI and localhost account listener. `SendConnectInfoChanged=0` captured only the 55-byte `0x0064`; setting it to 1 captured `0x0200` before `0x0064`. Both paths reached the production account-response wait status; fixture cleanup removed the temporary directory. Fresh deploy-off replay and client targets built; focused CTest passed 1/1 in 2.66s with local-loopback permission. Replay executable SHA-256 `24DBD074A4E048E645CC56CD33DCAFC2DB8884A72E3A8B1E9A8845010BECDF65` (19,821,053 bytes, `2026-11-04T03:57:25Z`); `open-midgard.exe` remains SHA-256 `B8AFDC9B89C79D2F378777585A236E3D8D877E85E9189A3902391D0D8E46B999` (19,664,555 bytes, `2026-11-04T02:40:00Z`). No discovery rows were closed. Evidence: `evidence/PK44.04-connect-info-gate.md` and `.jsonl`. Stable subject: `packet(PK44.04): verify connect info gate`. Next: inspect the optional `0x0204` hash-send gate and payload source.

## 2026-11-04 — PK44.04 verify executable-hash request gate

Extended the isolated production ConnectAccount replay to cover `0x0204`. Under the default America profile with no hash source, the stream omits the request; setting a synthetic `ClientHashOverrideMd5=00112233445566778899aabbccddeeff` emits the exact 18-byte `0x0204` before `0x0064`. The separate optional `0x0200` ordering also remains checked. No real credentials/server were used. Fresh deploy-off replay and client targets built; focused CTest passed 1/1 in 2.63s with local-loopback permission. Replay executable SHA-256 `99F574EE3A58C6FA9D464F42E1AD7648408B169B445B5619FB84DF625664BAA1` (19,821,571 bytes, `2026-11-04T03:59:50Z`). Built-in service hashes, executable-file hashing, deployed profile intent and discovery classification remain open. Evidence: `evidence/PK44.04-executable-hash-gate.md` and `.jsonl`. Stable subject: `packet(PK44.04): verify executable hash gate`. Next: capture a source-backed configured account-login request variant.

## 2026-11-04 — PK44.04 verify configured executable-hash source file

Created a temporary three-byte `abc` source file beside the isolated INI and set `ClientHashSourceExe=synthetic-client.exe`. Production ConnectAccount emitted `0x0204` with the independent known MD5 vector `900150983cd24fb0d6963f7d28e17f72`, followed by `0x0064`; temporary data was cleaned. This does not establish deployed hash policy or server enforcement. Fresh deploy-off replay/client targets built; focused CTest passed 1/1 in 2.69s with local-loopback permission. Replay SHA-256 `77000A8F5856B2E038DF41A7B019526B2820F5F8DFFA38A1BDDC8DF099137CEB` (19,832,713 bytes, `2026-11-04T04:10:22Z`). Evidence: `evidence/PK44.04-executable-hash-source-file.md` and `.jsonl`. Stable subject: `packet(PK44.04): verify executable hash source file`. Next: inspect the character-server entry (`0x0065`) transition for an isolated production ConnectChar replay.

## 2026-11-04 — PK44.04 capture character-server entry request

Drove production `OnChangeState(ConnectChar)` against a local listener after seeding a synthetic server endpoint and session tuple. The captured 17-byte `0x0065` matched the independent account/auth/sex fixture and reached “waiting for character list.” The listener sent no echo; raw AID reply consumption/alignment remains a PK04 gap and is not implied by this sender capture. Fresh deploy-off replay/client targets built; focused CTest passed 1/1 in 2.67s with local-loopback permission. Replay SHA-256 `D0D15E941B9F5CE12A8628DF0EBB19FF741A497D2CCB26649626D90662182222` (19,838,220 bytes, `2026-11-04T04:13:24Z`). Evidence: `evidence/PK44.04-character-server-entry-request.md` and `.jsonl`. Stable subject: `packet(PK44.04): capture character server entry request`. Next: inspect UI harnesses for production character-select/create actions; preserve the PK04 raw echo boundary.

## 2026-11-04 — PK44.04 exercise character-select UI action handlers

Added a headless source-extraction check for the actual `UISelectCharWnd` mouse-down/up, `SendMsg`, and action bodies. Occupied selection, empty-slot creation, eligible delete, cancel, hidden-window and outside-panel inputs routed to the expected login-mode messages. Native Qt event delivery, live `CModeMgr` integration and visible rendering remain open; this partial UI boundary does not block independent packet work. Verification: `python tools/tests/check_select_char_actions.py --cxx C:/Qt/Tools/mingw1310_64/bin/g++.exe` passed with output confirming all six cases. Evidence: `evidence/PK44.04-character-select-ui-actions.md` and `.jsonl`. Stable subject: `packet(PK44.04): exercise character select UI actions`. Next: apply the same headless production-handler check to `UIMakeCharWnd` creation actions.

## 2026-11-04 — PK44.04 exercise character-create UI action flow

Added a second headless source-body check for `UIMakeCharWnd` button display/hit-testing, mouse down/up, press/release pairing, and production `SendMsg`. A matched OK release submits `{1,2,3,4,5,6,18,42}` plus `RuneA` before `LoginMsg_CreateCharacter`; cancel returns to character select, and mismatched release sends nothing. Native Qt event delivery, live mode manager, widget-to-network send and rendering remain open. Verification: `python tools/tests/check_make_char_actions.py --cxx C:/Qt/Tools/mingw1310_64/bin/g++.exe` passed. Evidence: `evidence/PK44.04-character-create-ui-actions.md` and `.jsonl`. Stable subject: `packet(PK44.04): exercise character create UI actions`. Next: inspect Qt adapter event dispatch for a possible display-independent widget integration harness.

## 2026-11-04 — PK44.04 capture configured channel-login request

Set `AccountLoginPacket=channel` in isolated settings and captured the production `SendLoginChannel` path. The exact 85-byte selected-source `0x02B0` layout matched the independent fixture: date 20080910, padded synthetic credentials, client type 1, resolved local IPv4, fixed MAC string and zero Gravity flag. The server registers this layout, but Ref reachability and deployed profile intent remain unknown; discovery stays open. Fresh deploy-off replay/client targets built; focused CTest passed 1/1 in 2.63s with local-loopback permission. Replay SHA-256 `7241EB786C6DC8F8FD67B4AA03DC1D9D649AAB889AB364397C676F8CCF86ECF3` (19,828,316 bytes, `2026-11-04T04:04:38Z`). Evidence: `evidence/PK44.04-channel-login-request.md` and `.jsonl`. Stable subject: `packet(PK44.04): capture channel login request`. Next: inspect `0x0277` and keep `0x027C` pending its documented MAC/security review.

## 2026-11-04 — PK44.04 capture configured PC-bang login request

Set `AccountLoginPacket=pcbang` in isolated settings and captured the production `SendLoginPcBang` path. The exact 84-byte selected-source `0x0277` layout matched the fixture: date 20080910, padded synthetic credentials, client type 1, resolved local IPv4 and fixed MAC string. The server registers this raw-password request, but Ref reachability and deployed profile intent remain unknown; discovery stays open. Fresh deploy-off replay/client targets built; focused CTest passed 1/1 in 2.57s with local-loopback permission. Replay SHA-256 `8F1E8AA14D2A6D484E3B00D186B9FC86FE6FD0780AB6A05AC917D219C084C2BE` (19,829,510 bytes, `2026-11-04T04:06:44Z`). Evidence: `evidence/PK44.04-pcbang-login-request.md` and `.jsonl`. Stable subject: `packet(PK44.04): capture pcbang login request`. Next: replay the `0x01DB` gate using its default and explicit test setting; keep actual challenge reachability unverified.

## 2026-11-04 — PK44.04 verify password-hash request gate

Extended the isolated ConnectAccount local-mock replay: default `ForcePasswordHashRequest=0` sends the 55-byte `0x0064`, while explicit `=1` sends exactly `db 01` and enters “waiting for account hash challenge”. The test intentionally provides no challenge; external `passwordencrypt2`, Ref request reachability and deployed service behavior remain open. Fresh deploy-off replay/client targets built; focused CTest passed 1/1 in 2.70s with local-loopback permission. Replay SHA-256 `86474F4DB407AFC1468373CCFB1E0000BD36589ED7EBB3B4CAAA3320C692F2AB` (19,831,121 bytes, `2026-11-04T04:08:28Z`). Evidence: `evidence/PK44.04-password-hash-request-gate.md` and `.jsonl`. Stable subject: `packet(PK44.04): verify password hash request gate`. Next: exercise relative `ClientHashSourceExe` resolution with a synthetic file and independent digest.
## 2026-11-04 — PK44.04 reconcile verified login request directions

Reconciled `AUDIT-0x01DB`, `0x0204`, `0x0277`, and `0x02B0` to `C2S_ONLY`: each has a source-backed login-server C2S parser and a captured production current-client sender path under its stated gate/configuration. This follows the plan's verified-send-path criterion and is consistent with existing `0x0064`/`0x0200` classifications. It does not establish deployed profile intent, live compatibility, or downstream semantics; `0x01DD`, `0x01FA`, `0x027C`, and `0x0825` remain open. Also traced native character UI dispatch: it needs the Vulkan QWindow, legacy-message bridge and initialized global window-manager state, beyond the direct handler harness. PK44.04 remains IN_PROGRESS. `python tools/check_packet_plan.py` passed (51 packages, 232 steps, 990 discovery owners, 21 findings, 19 ignored registrations); `git diff --check` passed. Evidence: `evidence/PK44.04-login-request-direction-reconciliation.md`, `evidence/PK44.04-qt-event-dispatch-boundary.md`. Stable subject: `packet(PK44.04): reconcile verified login request directions`. Next: continue PK44.04 with another bounded row/handler reconciliation; do not treat the Qt integration boundary as a blanket stop.

## 2026-11-04 — PK44.04 reconcile challenge-response direction

The existing production replay proves `0x01DD` direction end to end within the bounded synthetic challenge path: it captures `0x01DB`, routes a 24-byte `0x01DC` through `RecvPacket` and `PollNetwork`, then captures the 47-byte `0x01DD` with both digest-order branches. Updated only `AUDIT-0x01DD` to `C2S_ONLY`. Real server challenge production and deployed clientinfo/profile selection remain unverified; `0x01FA`, `0x027C`, and `0x0825` remain open. No runtime changes or additional tests. Evidence: `evidence/PK44.04-login-hash-response-direction.md` and `evidence/PK44.04-login-hash-challenge.md`. Stable subject: `packet(PK44.04): reconcile challenge response direction`. Next: continue PK44.04 with bounded remaining identity/handler checks.

## 2026-11-04 — PK44.04 capture configured 0x027C login request

Added an isolated production `SendLogin4` capture for `AccountLoginPacket=login4`. The 60-byte request matched an independent fixture for `0x027C`, date `20080910`, synthetic username, MD5(`packet-pass`) `b5d24b16372774b2dc522d70491ef78f`, client type 1, and initialized `111111111111\0` MAC. The row is now `C2S_ONLY`; this is current-client evidence, not Ref parity or deployed-profile intent. Ref's separate unassigned MAC field remains a PK48 review item. Build of `packet_stream_replay_checks` passed; focused CTest passed 1/1 in 2.71 seconds with local loopback. `git diff --check` and `python tools/check_packet_plan.py` passed. Evidence: `evidence/PK44.04-login-027c-request.md` and `.jsonl`. Stable subject: `packet(PK44.04): capture configured 027c login request`. Next: continue source-backed PK44.04 row and branch checks; retain the Ref 0x027C and SSO security review limits.

## 2026-11-04 — PK44.04 reconcile legacy character request directions

Reconciled `AUDIT-0x0066` and `AUDIT-0x0067` to `C2S_ONLY` using the already committed selected-source production send captures and independent byte fixtures. This records request direction only; Qt click delivery, full mode lifecycle, profile/deployment intent and S2C outcomes remain open. No runtime code or tests changed. `python tools/check_packet_plan.py` and `git diff --check` passed. Evidence: `evidence/PK44.04-character-request-directions.md`, `evidence/PK44.04-character-selection-request.md` and `.jsonl`, and `evidence/PK44.04-character-create-request.md` and `.jsonl`. Stable subject: `packet(PK44.04): reconcile character request directions`. Next: continue PK44.04 with bounded handler/variant reconciliation.

## 2026-11-04 — PK44.04 exercise multiple character-server entries in 0x0069

Expanded the independent selected-source `0x0069` fixture from one to two 32-byte server records. The 111-byte response passed through production `RecvPacket -> PollNetwork -> OnAcceptLogin`; assertions verify all fields of both entries, preserve both entries, and confirm index 0 is selected for ConnectChar. The selected server contract derives the record count from packetLength and appends one record per available character server. Fresh deploy-off replay target build succeeded; focused CTest passed 1/1 in 2.70 seconds with local loopback. The subsequent socket connection and visible acceptance UI remain open. Evidence: `evidence/PK44.04-login-accept-reply.md` and `.jsonl`. Stable subject: `packet(PK44.04): exercise multiple character server entries`. Next: continue PK44.04 with bounded reply branches; do not infer live deployment or UI integration.

## 2026-11-04 — PK44.04 exercise multiple character records

Expanded the selected 20080910 character-list response fixture to two independent 108-byte records. Production framing and `PollNetwork` routing preserve both entries, translate each `u16` slot/hair-color tail correctly, and default-select index 0. The separate empty-list and `0x006D` creation cases still pass. Fresh deploy-off replay build succeeded; focused CTest passed 1/1 in 2.65 seconds with local loopback. Visible Qt list interaction and later record profiles remain open. Evidence: `evidence/PK44.04-character-record-conversion.md` and `.jsonl`. Stable subject: `packet(PK44.04): exercise multiple character records`. Next: continue PK44.04 with another bounded source-backed branch.

## 2026-11-04 — PK44.04 reject partial selected-profile character records

Added a 133-byte `0x006B` malformed-frame replay: after the 24-byte header and one full 108-byte record it carries one extra byte. Production framing dispatch reaches `TryResolveCharListLayout`, which rejects the non-integral record array, clears the character list, and returns to Login with the unsupported-layout status. This is malformed-input behavior; the selected server emitter produces whole records. Deploy-off replay build succeeded and focused CTest passed 1/1 in 2.78 seconds with local loopback. Evidence: `evidence/PK44.04-character-record-conversion.md` and `.jsonl`. Stable subject: `packet(PK44.04): reject partial character records`. Next: continue PK44.04 with bounded source-backed branches.

## 2026-11-04 — PK44.04 reject partial login server records

Validated the declared `0x0069` frame length, 47-byte fixed prefix, and integral selected-profile 32-byte server-record array before accepted-session mutation. An 80-byte production-framed input with one full record plus one trailing byte is rejected; server-list state is cleared, handshake state is reset, previously accepted auth credentials remain intact, and Login status is restored. Deploy-off replay target built; focused CTest passed 1/1 in 2.74 seconds with local-loopback permission. JSONL evidence is in `evidence/PK44.04-login-accept-reply.jsonl`. Stable subject: `packet(PK44.04): reject partial login server records`. No discovery rows were closed. Next: continue PK44.04 with a bounded source-backed branch; native Qt event delivery, raw AID echo alignment, deletion opcode mismatch, other profile behavior, and login acceptance UI/socket transition remain open.

## 2026-11-04 — PK44.04 reject over-capacity login server list

Removed the silent truncation of `0x0069` server lists beyond the client's 100-record array. A production-framed 3279-byte fixture with 101 complete records now produces an explicit error, clears list state, returns to Login, resets the account handshake, and preserves previously accepted credentials. This is a client storage limit, not a server-protocol maximum. Deploy-off replay target rebuilt; focused CTest passed 1/1 in 2.66 seconds with local-loopback permission. JSONL evidence is in `evidence/PK44.04-login-accept-reply.jsonl`. Stable subject: `packet(PK44.04): reject over-capacity login server list`. No discovery rows were closed. Next: continue PK44.04 with a bounded source-backed branch; native Qt event delivery, raw AID echo alignment, deletion opcode mismatch, other profile behavior, and full login acceptance UI/socket transition remain open.

## 2026-11-04 — PK44.04 reject over-capacity character list

`OnAcceptChar` previously truncated whole-record `0x006B` responses to the 12-entry client array. It now rejects 13 records, clears character-list state, and returns to Login. The independent server config uses `MAX_CHARS=9` at selected PACKETVER=20080910; that source maximum remains distinct from the client's 12-record storage bound. Fresh deploy-off replay target build succeeded; focused CTest passed 1/1 in 2.79 seconds with local-loopback permission. Evidence: `evidence/PK44.04-character-record-conversion.md` and `.jsonl`. Stable subject: `packet(PK44.04): reject over-capacity character list`. No discovery row was closed. Next: continue PK44.04 with another bounded source-backed branch; defer native Qt input/UI lifecycle, SSO negotiation, deletion opcode mismatch and rename/paged variants pending stronger review or reachability evidence.

## 2026-11-04 — PK44.04 record create-capacity recovery gap

Inspected the `0x006D` success handler: with all 12 client slots occupied it cannot append the server-confirmed character, yet reports success and transitions to the stale CharSelect list. Selected PACKETVER=20080910 `MAX_CHARS=9` is below that capacity, but safe cross-profile recovery requires a reconnect/refetch and verified LoginMode/UI lifecycle; growing storage without auditing consumers is unsafe. Recorded the exact gap in `evidence/PK44.04-character-create-capacity-gap.md` and BLOCKED.md; no runtime change or discovery closure. Next: add explicit production-helper coverage for selected-source `0x006A` result 5 in `tools/tests/replay/PacketStreamReplay.cpp`, then refresh `evidence/PK44.04-login-refusal-mapping.md` and `.jsonl`; run the replay target build and focused CTest.

## 2026-11-04 — PK44.04 cover login refusal result five

Added selected-source `0x006A` result 5 to the formatter replay with the version-rejection message, bringing mapped selected outcomes to 21. The fresh deploy-off replay target built and focused CTest passed 1/1 in 2.76 seconds with local-loopback permission. Refreshed `evidence/PK44.04-login-refusal-mapping.jsonl` from the executable. Binary SHA-256: `5C122DC5536B81BF9E57FF8CDC51D5C7941716813F64BC579B65E498DCC67BDF` (19,851,655 bytes; `2026-11-04T05:00:16Z`). This changes no production behavior or discovery classification; modal rendering remains unverified. Stable subject: `packet(PK44.04): cover login refusal result five`. Next: review whether `AUDIT-0x0081` can be closed from its two production-dispatched emitted results or whether the visible-modal requirement keeps the row open; inspect its contract/evidence and coverage classification before editing.

## 2026-11-04 — PK44.04 classify verified hash challenge response

Reconciled `AUDIT-0x01DC` from UNREVIEWED to `APPLICABLE_AND_FULLY_IMPLEMENTED_VERIFIED`. Evidence traces the selected source's 24-byte S2C challenge and the current client's production `RecvPacket -> PollNetwork -> OnAckHash` path; both password/salt MD5 orders match independent expected values, and each resulting 47-byte `0x01DD` packet is captured. The external clientinfo and deployed request-gate intent remain open as a separate reachability question; they do not change this response contract. No independent PK48 review is claimed. Plan checker passes. Stable subject: `packet(PK44.04): classify verified hash challenge response`. Remaining login/character UNREVIEWED rows: 31. Next: assess `AUDIT-0x0081` against its selected sender, production 1/8 handler replay and visible modal boundary; leave it open if required UI evidence is insufficient.

## 2026-11-04 — PK44.04 correct 0x0081 modal evidence boundary

Corrected the replay evidence: this Windows build links `src/main/WinMain.cpp`, where production `ErrorMsg` calls `MessageBoxA`; it does not link `AppMain_stub.cpp`. Refreshed the 0x006A/0x0081 JSONL rows from replay executable `0914985DA3634D6A951A5D5A700C8C0504BE3AB20504EE4EB2F3983F53CE0E52` (19,851,655 bytes, `2026-11-04T05:13:21Z`). Deploy-off build succeeded; focused CTest passed 1/1 in 2.81 seconds with local-loopback permission. Visible modal delivery remains unverified and `AUDIT-0x0081` remains UNREVIEWED. Stable subject: `packet(PK44.04): correct 0081 modal evidence boundary`. Next: continue with another bounded PK44.04 source-backed reply or state branch.

## 2026-11-04 — PK44.04 exercise accepted login endpoint transition

Extended `0x0069` replay with a second selected-source acceptance frame whose first server record points to a local mock char server. Production framing and `OnAcceptLogin` parse that endpoint, the production ConnectChar transition establishes the socket, and the peer captures the exact 17-byte `0x0065` `CA_ENTER` frame with accepted session fields. The client enters the character-list wait status. Fresh deploy-off build succeeded; focused CTest passed 1/1 in 2.69 seconds with local-loopback permission. Replay SHA-256: `909BFD6D54EB36466E7D6EF95392E57C549B0C0B48F2C0EEE1EB3DA875203CA8` (19,855,239 bytes, `2026-11-04T05:17:05Z`). Visible acceptance UI remains unverified; no discovery row was closed. Stable subject: `packet(PK44.04): exercise accepted login endpoint transition`. Next: select another bounded source-backed PK44.04 reply or state branch.

## 2026-11-04 — PK44.04 correct remaining modal linkage claims

Reconciled the active 0x0081 contract and 0x006C/0x006E evidence against the actual Windows replay link: `WinMain.cpp::ErrorMsg -> MessageBoxA`, not `AppMain_stub.cpp`. Visible modal delivery remains unverified. Updated the 0x0069 self-review to reflect the integrated endpoint-to-0x0065 loopback replay and retain the raw AID echo/list-response boundary as open under PK04. Historical progress notes remain unchanged. Stable subject: `packet(PK44.04): correct modal linkage evidence`.

## 2026-11-04 — PK44.04 implement explicit login3 hash request

Added `AccountLoginPacket=login3` for the selected-source 48-byte `0x01FA` MD5 request. The option automatically requests `0x01DB`; a production ConnectAccount loopback replay routes the 24-byte `0x01DC` challenge and captures all 48 response bytes. The fixture selects clientinfo profile index 1 and checks the independent salt+password MD5 vector. Fresh deploy-off build succeeded; focused CTest passed 1/1 in 2.72 seconds with local-loopback permission. Replay SHA-256: `42107B01A9635977C3C75491397308D83162550E411E4C12F9647ED76ABC1A9A` (19,867,221 bytes, `2026-11-04T05:35:45Z`). The >255 cleanup branch remains untested. `AUDIT-0x01FA` is now `C2S_ONLY`; external profile intent and Ref PC-bang `clientinfo+10` behavior remain open. Stable subject: `packet(PK44.04): implement explicit login3 hash request`. Next: continue PK44.04; keep raw AID echo/list receipt under PK04 and `0x0825` SSO length security under PK48.

## 2026-11-04 — PK04.01 specify raw account-ID handshake leaves

Promoted the existing applicability-only raw-AID contract into a bounded implementation specification with hash-pinned client/server sources, the selected 17-byte `0x0065` request, the unframed four-byte echo, exact required receive behavior, and separate PK04.02 state/API and PK04.03 production-replay boundaries. PK03.02 supplies production-framing dependency evidence; the blocked PK03 UI/full-frame seams are unrelated. No runtime code or discovery classification changed. Stable subject: `packet(PK04.01): specify raw account-id handshake leaves`. Next: PK04.02 model the character-server raw-prefix state and remove fake `0x8482/0x8483` opcode assumptions.

## 2026-11-04 — PK04.02 model raw account-ID receive state

Added a per-connection expected-AID prefix policy, armed only after ConnectChar succeeds and before `0x0065`; production receive waits for and compares all four bytes before generic framing. Connect/close hooks clear policy, and the fake `0x8482/0x8483` entries and no-op routes are removed. Fresh deploy-off build of replay, gamemode and app targets succeeded; existing production replay passed 1/1 in 2.79 seconds. This is regression evidence only; the newly added state still requires PK04.03's branch-by-branch production replay. No discovery rows were closed. Stable subject: `packet(PK04.02): model raw account-id receive state`. Next: PK04.03 add the mock-server raw echo cases in `tools/tests/replay/PacketStreamReplay.cpp` and verify every split, low-word collisions, mismatch, EOF and reconnect.

## 2026-11-04 — PK04.03 replay raw account-ID handshake

Added ConnectChar integration and production receive replays for ordinary and fixed/variable opcode-collision AIDs, each at all four split positions. Verified no premature packet delivery, complete suffix preservation, one-shot expectation clearing, mismatch close, EOF at zero through three bytes, and reconnect to ordinary framing. Fresh deploy-off build succeeded; focused CTest passed 1/1 in 2.93 seconds with local loopback. Nineteen JSONL records and exact binary hash/time are in `evidence/PK04.03-raw-aid-replay.md`/`.jsonl`. Fake discovery identities await PK04.04 reconciliation. Stable subject: `packet(PK04.03): replay raw account-id handshake`. Next: PK04.04 self-review and classify the now-verified raw handshake parents without claiming visible UI or deployed-server parity.

## 2026-11-04 — PK04.04 verify raw account-ID handshake

Self-reviewed the selected-source four-byte non-opcode handshake against PK01 applicability, PK04.02 implementation, and all 19 PK04.03 records. Added canonical implementation contract `PK04.RAW-AID`; classified fake receive-size parents 0x8482/0x8483 as `ALIAS` to that single verified handshake, with corrected stage/direction hints. Updated the PK44 cross-contract and index. The mismatch replay covers transport close/suffix suppression; full OnUpdate modal delivery and deployed-server provenance remain open. PK48 review is deferred. Stable subject: `packet(PK04.04): verify raw account-id handshake`. Next: PK05.01 establish the source-backed contract and leaf specification for unknown-opcode and invalid-length behavior, using PK04 as its completed framing dependency.

## 2026-11-04 — PK05.01 specify unknown receive boundaries

Reviewed the production `CRagConnection::RecvPacket` table lookup, unknown opcode branch, variable-length bounds and buffer transitions against the PK03.02 loopback record set and selected-source baseline. Documented that the current two-byte discard/rescan can reinterpret payload or malformed length bytes, while the source/TCP evidence provides no universal recovery rule. Added the bounded PK05.01 contract and work item; corrected stale “strong contract” wording in PK01.P02.FRAMING. No runtime code or discovery classifications changed, and no recovery policy is claimed. `git diff --check` and `python tools/check_packet_plan.py` are the planned checkpoint validations. Stable subject: `packet(PK05.01): specify unknown receive boundaries`. Next: PK05.02 bounded framing decision and implementation; PK05.03 error/cleanup contract follows. PK48 independent review pending, not a gate.

## 2026-11-04 — PK05.02 close on unknown framing boundaries

Replaced unknown-opcode and invalid-variable-length two-byte rescan with fail-closed disconnect, using the selected profile table as the only recognized boundary source. Added production loopback checks for unknown, under/oversized variable lengths, fragmented headers, valid suffix suppression and queue clearing. Existing framing/reconnect suite remains green. Fresh deploy-off configure/build completed; focused CTest passed 1/1 in 2.88 seconds. Executable SHA-256 `8811646DD87373F28D9DD014411F4E5BC461DBA8CB8341E0DFEEC52EC1CE8F80`; evidence in `evidence/PK05.02-replay.md` and `.jsonl`. No discovery rows close. Stable subject: `packet(PK05.02): close on unknown framing boundaries`. Next: PK05.03 typed error propagation and reporting, then PK05.04 self-review. PK48 review pending, not a gate.

## 2026-11-04 — PK05.03 report receive framing errors

Added `CRagConnection::ReceiveError` values for unknown opcode and invalid variable length, retained them through disconnect, and reset on connect. LoginMode now selects precise status/dialog text for these failures while ordinary EOF keeps its prior path. The invalid-variable diagnostic says the connection is closed, not that bytes are merely dropped. Replay asserts five typed cases and reconnect reset followed by a valid frame. Deploy-off build succeeded; focused CTest passed 1/1 in 2.97 seconds. Binary SHA-256 `B9537A6471555841AA326F8E22675057C0D32DC1BA8F3534FF5CA4F4730CD6C6`; visible modal and full OnUpdate lifecycle remain unverified. Evidence: `evidence/PK05.03-protocol-error-replay.md` and `.jsonl`. Stable subject: `packet(PK05.03): report receive framing errors`. Next: PK05.04 self-review; PK48 review pending, not a gate.

## 2026-11-04 — PK05.04 self-review receive framing

Reviewed PK05.01–PK05.03 source branches, profile-table boundary, typed error persistence/reset, LoginMode error mapping, disconnect queue cleanup, log text and replay. Self-review found and corrected stale “dropping” diagnostics to accurately say disconnecting; fresh target rebuild and focused CTest passed 1/1 in 2.81 seconds. Corrected executable SHA-256 `735168B2032B6ABFBD186B2E6F368B6ED41A405DC6A15FDBDD8E047685E75C33`. Profile 22 runtime replay, full GameMode/LoginMode lifecycle and visible modal remain open; no discovery rows are assigned or closed. PK48 independent review is pending and not a gate. Evidence: `evidence/PK05.04-self-review.md`. Stable subject: `packet(PK05.04): self-review receive framing changes`. Next: PK06.01 specify the selected-source guild member login notice contract.

## 2026-11-04 — PK06.01 specify guild member status notice

Traced selected rAthena `guild_recv_memberinfoshort` → `clif_guild_memberlogin_notice`, `PACKET_ZC_UPDATE_CHARSTAT` layout/guard, and the client receive table, state parser, production guild router and QML member status. The selected config defines PACKETVER, selecting packed S2C `0x01F2`/20-byte `ZC_UPDATE_CHARSTAT2`, with documented status 0 offline/1 online and three appearance fields. It is distinct from legacy `0x016D`/14-byte and `0x01EB` map position. Client has only 016D framing/handler; 01F2 currently hits PK05 fail-closed. No runtime change or coverage classification. Source hashes and next implementation split are recorded. Stable subject: `packet(PK06.01): specify guild member status notice`. Next: PK06.02 add exact framing, decode/state and production dispatch; PK06.03 UI/map propagation. PK48 is pending, not a gate.

## 2026-11-04 — PK06.02 decode guild member status notice

Added 01F2/20-byte receive framing, exact layout decode and production router dispatch. The state parser accepts only documented status 0/1, validates size before mutation, updates a matching account+character member and stores gender/hair values; no member is invented. Literal online/offline frames passed the production loopback/framing/router path, and short/unknown-status inputs were rejected atomically. Focused CTest passed 1/1 in 2.95 seconds; executable SHA-256 `E8010B4F67632AC521A451C873F8BFF6921CD0ED8906C40DB19F7D3EE25B6DEA`. QML/map propagation remains PK06.03. No discovery row closed. Stable subject: `packet(PK06.02): decode guild member status notice`. Next: PK06.03 UI/map propagation; PK48 review pending, not a gate.

## 2026-11-04 — PK06.03 propagate guild member presence

Added a production QVariant row mapper for online/gender/hair fields and used it from `PopulateGuildWindow`; QML binds the row's online value. Offline 01F2 now clears the matching AID's map-position marker, while 01EB remains the source of coordinates. Production replay exercised online state → position marker → offline cleanup, row values, and unrelated-member preservation. Build succeeded; focused CTest passed 1/1 in 2.92 seconds. Executable SHA-256 `6314633E597340C33BCDF478E7D91B3BDF5D261D17E152EE222BB4224C50B194`. Visible Qt window rendering remains unverified. Evidence: `evidence/PK06.03-presence-propagation.md` and `.jsonl`. Stable subject: `packet(PK06.03): propagate guild member presence`. Next: PK06.04 self-review; PK48 review pending, not a gate.

## 2026-11-04 — PK06.04 self-review guild member status

Self-reviewed selected 01F2 source layout and send branches against framing, member decode, production dispatch, Qt row mapping, and map marker lifecycle. All source-derived online/offline and malformed cases are covered; offline 01F2 removes the member's prior 01EB position marker. The deployed client view is not rendered in this harness; profile 22 and deployment parity remain unverified. Focused CTest and tracker checks pass. No discovery rows were closed; PK48 review is pending and does not gate progress. Evidence: `evidence/PK06.04-self-review.md`. Stable subject: `packet(PK06.04): self-review guild member status`. Next: PK07.01 specify the selected-source pet status profile and layout.

## 2026-11-04 — PK07.01 specify pet status contract

Established selected-source map S2C `0x01A2` as a packed 35-byte pet status snapshot for effective profile 20080910; the 2-byte `job` extension is guarded at PACKETVER >= 20081126 and remains a separate PK47 identity. Documented exact offsets, sender field sources/rename branch, SELF recipient, status-refresh caller families, and absence of actor identity. Client currently frames 37 bytes. PK27 retains lifecycle ownership; no code or coverage classification changed. No build run for this contract checkpoint. Stable subject: `packet(PK07.01): specify pet status contract`. Next: PK07.02 active-profile framing correction. PK48 is pending, not a gate.

## 2026-11-04 — PK07.02 select active pet status framing

Changed selected-profile `0x01A2` framing from 37 to 35 bytes and replayed a literal source-layout sample followed by fixed `0x006C` through production `Connect`/`RecvPacket`; both packets arrived intact and the queue emptied. Deploy-off build succeeded; focused CTest passed 1/1 in 2.91 seconds. Binary SHA-256 `26B6DDC5C72B766B292D7E074209A404FE46F44EE98F845608E31DC0316B7F15`. Evidence in `evidence/PK07.02-replay.md`/`.jsonl`. This does not decode pet state or close discovery. Stable subject: `packet(PK07.02): select active pet status framing`. Next: PK07.03 trace and implement pet status state integration; PK27 owns lifecycle.

## 2026-11-04 — PK07.03 decode pet status snapshot

Added exact-size/opcode 35-byte parser for fixed name and signed modified/level/fullness/relationship/accessory fields. Production route updates a dedicated session snapshot only after complete validation; no actor identity is inferred. Replay covers each field, malformed size/opcode atomicity against prior state, signed wire values, and the production TCP/router path. Deploy-off build succeeded; focused CTest passed 1/1 in 2.97 seconds. Binary SHA-256 `51728B3B0FF5A56E01C3E08526CC08807C467606A5470F7F99D212F23AA2DB32`. Visible pet UI and owner/lifecycle transitions remain PK27. Stable subject: `packet(PK07.03): decode pet status snapshot`. Next: PK07.04 self-review; PK48 pending, not a gate.

## 2026-11-04 — PK07.04 self-review pet status implementation

Self-review reconciled the selected 35-byte source layout, sender branches, signed field parser, session snapshot, production dispatch and replay. Caught that packet-size tables are separate but shared initialization had changed both profile 23 and 22; corrected profile 22 to retain its preexisting 37-byte behavior pending PK47 date/layout evidence. Profile 23 production replay passed; focused CTest 1/1 in 2.87 seconds; binary SHA-256 `88850AFBE65A2B1B79431DC526B37FE84C75516D616991BF7F34C64F36ADAFF5`. No pet actor/lifecycle/UI or discovery classification claimed. PK48 review pending. Stable subject: `packet(PK07.04): self-review pet status implementation`. Next: PK08.01 ground-skill text request source contract.

## 2026-11-04 — PK08.01 specify ground skill text contract

Established the effective map C2S MoreInfo request as `0x007E`/102 bytes with parser offsets 5,9,12,20,22; distinguished it from selected 19-byte `0x0190` and 7-byte `0x0437` ActionRequests and older guarded variants. Traced all active sender checks, player versus homunculus/mercenary branches, 80-byte bounded text copy, later skill-unit use, and existing client builder/test mismatch. The client send method has no caller; no UI reachability is claimed. No code or discovery classification changed. Stable subject: `packet(PK08.01): specify ground skill text contract`. Next: PK08.02 select the server-authoritative request contract, then PK08.03 serializer/independent test fix. PK48 pending, not a gate.

## 2026-11-04 — PK08.02 select ground skill text contract

Switched profile-23 text-ground-skill opcode to selected server `0x007E` and replaced the 90-byte compact struct with a packed 102-byte layout matching offsets 5,9,12,20,22. Added size/offset assertions, signed coordinate types and 79-byte NUL-terminated text bound. Deploy-off `ro_gamemode` build passed; static library SHA-256 `A10836EF3CA9DCC1549CF2B9D5C0EE79A8FB5235C2CF0991FC7E7A97848F4E64`. The old runtime test still asserts 90-byte/0190 and is intentionally replaced in PK08.03 before runtime test claims. No in-repo UI caller exists. Stable subject: `packet(PK08.02): select ground skill text contract`. Next: PK08.03 replace client-derived test oracle with independent fixture and production sender replay. PK48 pending, not a gate.

## 2026-11-04 — PK08.03 verify ground skill request bytes

Replaced the stale 90-byte/0x0190 expectations with the selected profile-23 102-byte/0x007E MoreInfo layout. The production builder output now matches all bytes of independent fixture `P05-skill-ground-text-synthetic.hex` (decoded-byte SHA-256 `6964f423166f13b756fc9b47180d16243873390f38e5c9aef2a912738c3efbd6`); parser offsets, max accepted text bound and undersized output rejection are checked. The focused deploy-off target built and CTest passed 1/1; binary SHA-256 `410EC20FAC03AA2C04553EA7B425D69AB00B67831B7BD78B27612F39C82B5E2D`. This is serializer/helper verification only: no socket send capture, UI caller, discovery closure, deployed-server parity or PK48 approval. Evidence: `evidence/PK08.03-serializer-fixture.md`. Stable subject: `packet(PK08.03): verify ground skill request bytes`. Next: PK08.04 self-review and production send-path replay.

## 2026-11-04 — PK08.04 record send-path review boundary

Self-reviewed the production `0x007E` send helper: it validates skill and coordinates, builds into a zeroed 102-byte buffer, propagates the builder length to `CRagConnection::SendPacket`, and returns send success. PK08.03 independently verifies serializer bytes, but there is no production transport capture seam and no current UI caller. A safe replay would require broader game-mode/connection test architecture, so this S-complexity gap is recorded for PK48/stronger-agent review. No rows closed and no fresh binary or UI behavior claimed. Evidence: `evidence/PK08.04-send-path-review.md`. Stable subject: `packet(PK08.04): record send path review boundary`. Next: PK09.01 bounded inventory/cart alias contract scope.

## 2026-11-04 — PK09.01 bound inventory/cart alias review

Defined PK09 scope for the sole assigned parent `AUDIT-0x01EF` and its selected `0x02D0` equipment interaction. Identified variable-length framing and profile-based shared equipment-list handler landmarks; retained final server registration/layout and parser semantics as open work for PK09.02/09.03. Specified profile22/23 variants, duplicate route ownership, cart-clear invariant, state teardown and validation boundaries. Scope-only checkpoint; no runtime behavior or discovery row certified. Evidence: `evidence/PK09.01-scope.md`. Stable subject: `packet(PK09.01): bound inventory cart alias review`. Next: PK09.02 resolve 0x01EF duplicate router registration per profile.

## 2026-11-04 — PK09.02 document 0x01EF ownership conflict

Traced a duplicate client registration: profile routing assigns `0x01EF` to equipment inventory, then Commerce registration overwrites it; Commerce parses it as 18-byte cart entries. Because the effective 2008 server identity and active profile binding are not yet resolved, changing either route would risk corrupting cart/equipment state. Recorded the exact source paths and required resolution evidence. PK09.02 is blocked; PK09.03 remains dependent. No runtime change or discovery closure. Evidence: `evidence/PK09.02-01ef-ownership-conflict.md`. Stable subject: `packet(PK09.02): document 01ef ownership conflict`. Next independent task: PK10.01.

## 2026-11-04 — PK10.01 bound connection transition review

Specified source inputs, packet families, branch/lifecycle checks and bounded split for `0x0283` confirmation and `0x02D5` server disconnect. PK10 owns two primary discovery parents; framing lengths are treated only as discovery. PK03.04 partial disconnect replay is explicitly limited to its covered boundaries. Scope checkpoint only; no runtime behavior or rows certified. Evidence: `evidence/PK10.01-scope.md`. Stable subject: `packet(PK10.01): bound connection transition review`. Next: PK10.02 source-trace map confirmation and login/character failure branches.

## 2026-11-04 — PK10.02 record 0x0283 stage boundary

Confirmed selected server `0x0283` is six bytes and carries `sd->id` at offset 2 before `chrif_authreq`; the earlier raw four-byte form is gated to PACKETVER < 20070521. Client LoginMode currently ignores this opcode, and PK03.04's synthetic ignored-packet replay proves framing only. Full map transition/authentication and login/character failure UI branches are S-complex and not safely certified by current replay. Recorded exact facts and stronger-review gap. No code change or row closure. Evidence: `evidence/PK10.02-0283-transition-boundary.md`. Stable subject: `packet(PK10.02): record 0283 stage boundary`. Next independent task: PK11.01.

## 2026-11-04 — PK11.01 bound guild permission review

Scoped five primary discovery parents `0x014E/0x0156/0x0160/0x0166/0x0174`, with `0x016C` retained as a cross-review dependency for local rank/permissions. Split incremental state reconciliation from Qt menu/action gating and end-to-end visible review. Existing `GuildState.menuFlags` and router landmarks are discovery only. No runtime behavior or rows certified. Evidence: `evidence/PK11.01-scope.md`. Stable subject: `packet(PK11.01): bound guild permission review`. Next: PK11.02 reconcile incremental updates with 0x016C state.

## 2026-11-04 — PK11.02 reconcile guild position permissions

Implemented local guild title/permission reconciliation from the current member position and position mode after guild-window packets. Server evidence identifies 0156 member position rows, 0160 permission-position rows, 0166 position names, 0174 position change rows, and 016C local mode. Added fixture assertions for 0156 and 0174 updates. Deploy-off full app target and packet replay target linked; aggregate build returned 1 because the separate skill packet runtime test target has unresolved `HandleQtUiRuntimeWindowMessage`. Focused `check_guild_window.py` passed, including Qt fixture using fresh app binary (SHA-256 `39B56D7359E6824D742E76A5797013206E54CA8E66EC49B6C92E68323DCFA79C`). No discovery row closed; PK48 pending. Evidence: `evidence/PK11.02-guild-permission-reconciliation.md`. Stable subject: `packet(PK11.02): reconcile guild position permissions`. Next: PK11.03 apply menu flags/current permission gates to Qt controls.

## 2026-11-04 — PK11.03 apply verified guild menu gates

Propagated 0x014E menu flags into Qt state, gated known tabs and outbound notice/position/skill/member-position actions, and added matching QML control states. Added action tests for flag-required/denied paths and seeded the local preview with server role masks 0xD7/0x57. Fresh deploy-off `open_midgard` target built; `check_guild_window.py` passed all parser/action/screenshot cases. The custom Relations tab remains unresolved against server 0x40 `GMENUFLAG_ALLGUILDLIST`, so PK11.03 is blocked for stronger review; no discovery rows closed. Evidence: `evidence/PK11.03-menu-flags.md`. Stable subject: `packet(PK11.03): apply verified guild menu gates`. Next independent package: PK14.01.

## 2026-11-04 — PK14.01 bound party position contract

Traced the sole parent `AUDIT-0x0107`: 10-byte S2C packet with AID and signed 16-bit map-cell X/Y; server sends `(-1,-1)` to remove the dot, broadcasts only among same-map party members, and invokes removal on map departure. Specified session update, roster/map/reconnect teardown, and minimap-rendering leaf boundaries. Scope only; no runtime rows closed. Evidence: `evidence/PK14.01-party-position-scope.md`. Stable subject: `packet(PK14.01): bound party position contract`. Next: PK14.02 implement account-ID state and sentinel lifecycle.

## 2026-11-04 — PK14.02 implement party position state

Replaced the ignored 0x0107 route with exact 10-byte AID/signed-cell decoding. Added AID-only roster updates, atomic (-1,-1) removal, bounds/unknown-member rejection, same-map refresh preservation, and remote-map invalidation. Standalone focused state cases pass; fresh deploy-off `open_midgard` target compiles production handler/session integration. Production framing-router replay and minimap rendering remain PK14.04/03. No row closed. Evidence: `evidence/PK14.02-party-position-state.md`. Stable subject: `packet(PK14.02): implement party position state`. Next: PK14.03 render markers through existing minimap state.
## 2026-11-04 — PK14.03 render party minimap markers

Native minimap drawing and Qt marker publication now consume valid party positions from session roster state, retain the existing coordinate transform/color/size, and include party marker state in minimap redraw invalidation. Deploy-off `open_midgard` build and `git diff --check` pass. Visual marker behavior and full packet lifecycle remain PK14.04; no discovery row closed. Evidence: `evidence/PK14.03-party-minimap-markers.md`. Stable subject: `packet(PK14.03): render party minimap markers`. Next: PK14.04 production replay and applicable UI lifecycle checks.
## 2026-11-04 — PK14.03 checkpoint finalized

PK14.03 implementation checkpoint `3ff44a3e` is recorded; marker rendering is complete at the source/build level. Visible marker/lifecycle behavior remains an explicit PK14.04 gate. Next: PK14.04 production replay and applicable UI lifecycle checks.
## 2026-11-04 — PK14.04 self-review; integration checks deferred

Reviewed PK14.01–PK14.03 source and state lifecycle. Production app build and focused party position state executable pass; plan validator passes. Deferred production-framed replay and visible lifecycle verification because the replay source is already dirty with ownership unresolved, and no standalone visible assertion is available. No discovery row closed; no end-to-end parity claim. Evidence: `evidence/PK14.04-verification-boundary.md`. PK14.04 needs stronger follow-up after replay-file ownership/reconciliation. Continue with independent PK16.01 scope.
## 2026-11-04 — PK16.01 bound privacy and social state

Scoped P11 across 0x02C8 party-invite settings, 0x02D8/0x02D9 server-confirmed configuration, 0x02DA notification, and separate friends/whispers/blocklists/count/failure-notice audit. Registry evidence supports candidate lengths/offsets; available server parser snapshot documents config request/ack flow but its exact 2008 profile identity is unresolved, and the client has no 0x02D8 framing registration. PK34 owns atomic friend snapshots. No rows closed. Evidence: `evidence/PK16.01-privacy-social-scope.md`. Stable subject: `packet(PK16.01): bound privacy and social state`. Next: PK16.02 adjudicate profile and implement proven settings only.
## 2026-11-04 — PK16.02 profile identity blocker

Confirmed the current receive registrations and ignored handlers; searched for client 0x02D8 framing, serializer, and setting UI call sites. The selected server registry and available external parser snapshot provide a candidate layout and setting enum but not an identity-matched 2008 profile; repository commit identity could not be read. Do not guess IDs or wire behavior. No rows closed. Evidence: `evidence/PK16.02-profile-blocker.md`. Continue with independent PK16.03 social-path audit.
## 2026-11-04 — PK16.03 audit social paths

Audited friend roster/state/add/delete/request handlers, whisper send/receive/ack state, candidate ignore commands, user-count packet framing, and failure notices. Found a malformed 0x0201 snapshot atomicity risk owned by PK34, a single-slot whisper correlation risk, and silent friend-add/delete outcomes. 0x00D2/0x00D7 semantics and complete block-list behavior remain unverified. No discovery rows closed. Evidence: `evidence/PK16.03-social-path-audit.md`. Stable subject: `packet(PK16.03): audit social state paths`. Next: PK16.04 production replay/UI verification; keep profile gates open.
## 2026-11-04 — PK16.04 verification deferred

Reviewed PK16.02/03 source evidence and confirmed the profile and replay gaps. Planning consistency passes, but no production replay or visible UI lifecycle check can prove behavior; the existing replay source is dirty with unresolved ownership. Defer for stronger review after replay ownership and target profile are established. No audit rows closed. Evidence: `evidence/PK16.04-verification-boundary.md`. Continue next independent PK18.01 scope.
## 2026-11-04 — PK18.01 scope dynamic map cells

Bound 0x0192 to a 24-byte map-cell update carrying signed cell coordinates, type and a 16-byte map name. The available rAthena sender snapshot shows Ice Wall and permission-gated map-cell call paths, but its internal cell enum does not match the wire value and no target 2008 profile/Ref definition is available. Require a proven type mapping before world/pathfinding/render changes; 0x0199/01D6/02E7 remain PK19. No rows closed. Evidence: `evidence/PK18.01-dynamic-map-cell-scope.md`. Stable subject: `packet(PK18.01): bound dynamic map cell scope`. Next: adjudicate PK18.02 type mapping before implementation.
## 2026-11-04 — PK18.02 deferred on cell type mapping

No runtime change: the available sender's 0x0192 type field cannot be mapped reliably to client collision/render semantics. Wire value 5 for Ice Wall conflicts with internal enum ordinal 9, and no identity-matched 2008 Ref/profile mapping or dynamic overlay exists. PK18.03/04 remain dependent and deferred. No rows closed. Evidence: `evidence/PK18.02-type-mapping-blocker.md`. Continue next independent eligible package PK20.01.
## 2026-11-04 — PK20.01 scope NPC class replacement

Bound fixed 11-byte 0x01B0 (GID, unused type byte, class ID) separately from 0x01D7's look selector/value contract. Available server snapshot shows class change excludes player classes and is used for mobs/homunculi, but target profile identity remains unverified. Listed model/resource, missing actor, repeated change, picking, visibility, and map teardown cases. No rows closed. Evidence: `evidence/PK20.01-npc-class-replacement-scope.md`. Stable subject: `packet(PK20.01): scope NPC class replacement`. Next: PK20.02 distinct class handler/rebuild.
## 2026-11-04 — PK20.02 deferred on actor resource lifecycle

Did not make a runtime change: 0x01D7's base-look update does not prove class-specific NPC model/resource replacement, and replacing/upgrading actors transfers effect/message ownership. PK20 acceptance requires render, picking, visibility, repeated-change, release, and teardown checks unavailable through the established actor API. Defer PK20.02 and dependent .03/.04 for stronger actor/resource review. No rows closed. Evidence: `evidence/PK20.02-resource-rebuild-blocker.md`. Continue independent PK22.01.
## 2026-11-04 — PK22.01 scope ShowEFST and extended status

Bound 0x028A's 18-byte four-u32 contract and recorded Ref's distinct `showEFST` branch groups; the current handler covers only level-99 and Assumptio. Separated 0x043F's unexplained trailing 12 bytes and newer-profile status from the 2008 reference. Exact target status enum identity and effect lifecycle remain gates. No rows closed. Evidence: `evidence/PK22.01-showefst-scope.md`. Stable subject: `packet(PK22.01): scope ShowEFST status branches`. Next: PK22.02 only after status enum/lifetime mapping is source-backed.
## 2026-11-04 — PK22.02 deferred on branch lifecycle proof

Compared Ref showEFST branches with available rAthena `e_sc_opt3`; all candidate masks align. However, branch/profile identity and status-end cleanup are not established, and Ref side effects include priority effects, hidden-state cleanup, and motion/ARGB lifetimes. The dirty replay harness cannot safely be extended here. Defer PK22.02–.04 for stronger status/profile and lifecycle review. No rows closed. Evidence: `evidence/PK22.02-branch-lifecycle-boundary.md`. Continue next independent package.
## 2026-11-04 — PK24.01 scope resource and actor invariants

Bounded PK24's 44 provisional primary discovery rows, separated arithmetic/width audit from actor lifecycle review, and identified 0x0AF0 as a PK44 cross-review boundary. PK24.02 covers status/resource widths and math; PK24.03 covers ordering/actor teardown/hallucination display; PK24.04 owns production extreme-value replay. No rows closed. Evidence: `evidence/PK24.01-resource-state-scope.md`. Stable subject: `packet(PK24.01): scope resource state invariants`. Next: PK24.02 audit and correct source-backed signed-width defects.
## 2026-11-04 — PK24.02 audit and preserve unsigned EXP

Audited resource/status widths and fixed unsigned EXP narrowing from u32 packet values through signed int session/actor fields. Added wide percentage math and extreme-value cases; high-vitals script and `ro_gamemode` build pass. Full app link could not replace the existing executable due `Permission denied`. Legacy selected-character HP/SP display cap remains separate from full-width runtime/session vitals. No discovery rows closed; actor-order/hallucination semantics remain .03/.04. Evidence: `evidence/PK24.02-width-audit.md`. Stable subject: `packet(PK24.02): preserve unsigned experience values`. Next: PK24.03 actor transition and lifecycle audit.
## 2026-11-04 — PK24.03 deferred on resource/effect ordering

Reviewed status-first session/actor updates and 0x0284 effect 331/332 recovery mutation. The generic server packet describes effect-dependent visual data; target 2008 call sites do not prove those values are authoritative recovery under Hallucination. Current path also adds without max-HP/SP clamping. Changing either side without sender proof risks incorrect resources. Broader 44-row actor lifecycle and production replay remain open. Defer PK24.03/04 for stronger source/replay review; no rows closed. Evidence: `evidence/PK24.03-actor-state-boundary.md`. Continue PK25.01 independently.
## 2026-11-04 — PK25.01 scope NPC dialogue/script messages

Scoped 21 provisional owners; bound existing dialogue and input packet family, and identified missing 23-byte 0x0144 compass and 4-byte 0x0189 map-notice handlers from the available server packet structures. PK25.02 audits menu indexes/cancel/string/input/shop lifetime; PK25.03 implements missing markers/notices/script choices; PK25.04 owns replay/UI verification. No rows closed. Evidence: `evidence/PK25.01-dialogue-script-scope.md`. Stable subject: `packet(PK25.01): scope NPC dialogue and messages`. Next: PK25.02 audit input/menu/close/shop handoffs.
## 2026-11-04 — PK25.02 audit menu/input/handoff

Audited menu ordinal/cancel and conversation routing; current choice mapping matches candidate server values 1–254 and 255 cancel. Found that 0x01D5 accepts any u16-sized string while available server parser caps at 70 bytes, silently truncating overlong input; target profile identity remains open. Shop flows are separate mode-gated packet functions. No runtime edits or rows closed. Evidence: `evidence/PK25.02-input-handoff-audit.md`. Stable subject: `packet(PK25.02): audit dialogue input handoffs`. Next: PK25.03 implement missing 0144/0189 and source-backed script choice behavior.
## 2026-11-04 — PK25.03 implement 0189; defer compass lifecycle

Added exact 4-byte 0x0189 handler using target Ref type 0–3 messages and item-ack reset; `ro_gamemode` builds. Existing script choice path preserves indexes and 255 cancel. Deferred 0x0144 because expiry, remove/persistent lifecycle, map transitions, native/Qt redraw state, and replay verification need a focused design. PK25.03 remains partial and .04 cannot close. No rows closed. Evidence: `evidence/PK25.03-map-notice-and-compass-boundary.md`. Continue independent PK32.01 legacy mail scope.
## 2026-11-04 — PK25.04 verification deferred

PK25.03 remains partial because the 0x0144 compass lifecycle is not implemented. PK25.04 cannot prove expiry/removal/map/death and native/Qt redraw, nor production replay. Preserve the dirty replay source. No discovery rows closed. Evidence: `evidence/PK25.04-verification-boundary.md`. Continue independent PK32.01.
## 2026-11-04 — PK32.01 scope legacy mail workflow

Bound 18 provisional owners across inbox/read/new-mail, attachments/claims, delete/return, and compose/send. Selected registry has candidate request/result sizes; target profile and variable-list fields remain to adjudicate. Targeted client search found no dedicated mail UI/session model and only a 0249 ignore registration. Claim semantics require idempotent server-confirmed state. No rows closed. Evidence: `evidence/PK32.01-legacy-mail-scope.md`. Stable subject: `packet(PK32.01): scope legacy mail workflow`. Next: adjudicate source/profile and client state before PK32.02 inbox implementation.
## 2026-11-04 — PK32.02 deferred on full mail state machine

Ref confirms distinct inbox/read/attachment/send/delete/return/result UI routines and inventory/gold mutation on claim ack. Current client lacks the required mail model/UI, and the 18-row workflow requires coherent variable-list parsing, stale IDs, duplicate claims/sends, denial/full inventory, and cancel semantics. No safe isolated inbox handler; replay source remains dirty. Defer PK32.02–.05 for stronger implementation review. No rows closed. Evidence: `evidence/PK32.02-full-mail-state-blocker.md`. Continue PK50.01 independently.
## 2026-11-04 — PK50.01 scope quest journal and missions

Bound legacy list/mission/add/remove/progress/state packets and related 02B6 request; recorded 104-byte mission records and 107-byte add entries from the available sender. Captured newer 097A/09F8/08FE/09F9 branches as conditional variants, not default-2008 evidence. Time units and source identity remain open; client has no quest journal model/handlers in inspected paths. No rows closed. Evidence: `evidence/PK50.01-quest-mission-scope.md`. Stable subject: `packet(PK50.01): scope quest mission lifecycle`. Next: adjudicate target profile/time units before implementing atomic snapshots.
## 2026-11-04 — PK50.02 deferred on journal model/time contract

Ref confirms legacy 02B1/02B2 behavior, but list handling clears before validating, mission snapshots update in place, and time/objective conversions need exact field review. Current client lacks a typed quest model/UI; newer PACKETVER branches use different layouts. Defer PK50.02–.04 for stronger profile/model work; do not copy non-atomic Ref behavior. No rows closed. Evidence: `evidence/PK50.02-snapshot-model-boundary.md`. Resume existing independent PK44.04 review after the login3 implementation checkpoint.

## 2026-11-04 — PK48.01 define independent certification scope

Recorded authoritative inputs, per-contract review and replay evidence requirements, summary schema, and entry gates. Certification is not ready: feature leaves remain deferred, PK44.04 lifecycle/UI work is incomplete, PK47.04 lacks row-level evidence, and replay-file ownership remains unresolved. Scope only; no runtime changes or rows closed. Evidence: `evidence/PK48.01-scope-boundary.md`. Resume PK44.04 implementation/evidence work before PK48.02.

## 2026-11-04 — PK44.04 rerun focused production replay

The replay target built successfully. Restricted CTest could not connect to its local mock before handler invocation; the same focused test passed 1/1 with local-loopback permission. Recorded executable and dirty source hashes in `evidence/PK44.04-focused-replay-rerun.md`. The replay source remains user-owned and untouched; no rows closed, and PK44.04 remains in progress.

## 2026-11-04 — PK46.01 define outbound parity scope

Specified source inputs, per-request trace fields, review leaves, and evidence gates. PK46.02 covers 177 accepted map requests (49 constructor matches, 128 unproven); PK46.03 covers login/character sends and unsupported gates; PK46.04 self-reviews and validates only applicable paths. The ledger has 237 secondary PK46 assignments and no primary owners. No runtime changes or rows closed. Evidence: `evidence/PK46.01-scope-boundary.md`. Next: trace PK46.02 in bounded batches.

## 2026-11-04 — PK46.02 trace map movement request

Traced WinMain mouse messages through held-click GameMode movement to the 8-byte 0x00A7 request. PacketVer22/23 encode the packed destination at offset 5, matching the selected 20080910 server registration/parser; explicit Legacy profile uses 0x0085/5 bytes and does not match. Active deployment profile and packet-specific replay remain open. One wire path only; no rows closed. Evidence: `evidence/PK46.02-map-movement.md`.

## 2026-11-04 — PK46.02 identify change-direction profile mismatch

Traced the mouse direction action to the 11-byte PacketVer22/23 0x0085 sender (HeadDir@7, Dir@10). The selected 20080910 server expects 14 bytes with fields at 10/13. Shared profile changes are unsafe without a selected 2008 outbound profile and production replay; defer correction for stronger profile review. Together with movement, this is only two bounded paths; PK46.02 remains incomplete and no rows closed. Evidence: `evidence/PK46.02-map-change-direction.md`. Proceed to PK46.03 login/character requests while retaining the open map inventory.

## 2026-11-04 — PK46.03 cross-check login/character sends

Cross-referenced current login sends and legacy character select/create captures with PK44 contracts/evidence. Confirmed request-direction evidence only for recorded settings/branches; native Qt delivery and full lifecycle remain open. Preserved the 0x0068/0x01FB deletion mismatch, 0x0825 SSO security gate, and unproven rename/scheduled/paged actions. PK46.03 remains partial; PK46.04 cannot certify while PK46.02 and these gates remain open. No rows closed. Evidence: `evidence/PK46.03-login-character-crosswalk.md`.

## 2026-11-04 — PK45.01 define actor/handler audit scope

Specified exact source inputs, per-handler evidence, and actor/receive-handler leaves. The 240 registered-entry and 136 handler-function inventories require packet/variant-level contracts. PK20 class-resource, PK22 status visual/motion, and PK24 effect-ordering branches remain explicit gates; audit unrelated handlers independently. Scope only; no runtime changes or rows closed. Evidence: `evidence/PK45.01-scope-boundary.md`. Next: bounded nonblocked PK45.02 actor paths.

## 2026-11-04 — PK45.02 trace actor name acknowledgments

Traced selected 20080910 server 0x0195 player name/party/guild/position and 0x0095 non-player name layouts into the current `HandleActorNameAck` receive and cache updates. The 0x0194 variant, request reachability, lifecycle cleanup, malformed replay, and visible rendering remain open. One handler family only; no rows closed. Evidence: `evidence/PK45.02-actor-name-ack.md`. Continue with nonblocked actor spawn/move/action leaves; preserve PK20/22/24 gates.

## 2026-11-04 — PK45.02 trace actor vanish lifecycle

Traced selected-source 7-byte 0x0080 (GID@2, reason@6) from server emitters into the client handler. Reasons 0/1/other map to remove/death transition/remove; name and chase caches clear on final actor removal. Timing, replay, idempotence and rendered lifecycle remain open. One handler family only; no rows closed. Evidence: evidence/PK45.02-actor-vanish.md.

## 2026-11-04 — PK45.02 trace self-movement acknowledgment

Traced selected-source 12-byte 0x0087 from clif_walkok through HandleSelfMoveAck. Tick@2 and packed path@6 update session/actor positions and clear pending move acknowledgment. Self-only path; remote movement/spawn, correlation, malformed bounds, and rendered interpolation remain open. No rows closed. Evidence: evidence/PK45.02-self-movement-ack.md.

## 2026-11-04 — PK45.02 trace actor action notification

Traced selected 20080910 33-byte 0x02E1 from clif_damage through HandleActorActionNotify. Signed damage, action, target, tick, hit count, and timing offsets align; server config changes values, and motion/effect presentation remains unverified. One selected variant only; no rows closed. Evidence: evidence/PK45.02-actor-action-notify.md.

## 2026-11-04 — PK45.02 defer actor spawn variant family

The client receive profile exposes legacy, shifted, modern, and font spawn variants; the selected server emitter/actor-kind layout has not been tied to the matching decode path. PK20 class-resource, PK22 status/motion, and PK24 effect-ordering boundaries also affect spawn presentation. Defer this family for stronger source/profile review and independent replay. No rows closed. Evidence: evidence/PK45.02-spawn-variant-boundary.md. Move to an independently supported PK45.03 receive-handler leaf.

## 2026-11-04 — PK45.03 correct 0x0189 type-1 meaning

Cross-checking PK25.03 against authoritative rAthena found type 1 is CANT_MEMO, emitted for MF_NOMEMO/MF_NOWARPTO, while the client said teleport was possible based on Ref wording. Corrected the message to “Saved point cannot be memorized.” Preserved the packet length/type-3 ack behavior; display color remains unverified. ro_gamemode build passed. One handler correction only; no rows closed. Evidence: evidence/PK45.03-map-info-semantics.md.

## 2026-11-04 — PK45.03 trace NPC number input

Traced 0x0142 prompt through HandleNpcDialogNumberInput and UINpcInputWnd to the 10-byte 0x0143 response and 0x0146 cancel request; selected server parser field offsets and script continuation match. Client value is u32/strtoul while server reads int32, and the UI does not visibly clamp this path to INT32_MAX. Defer numeric-domain parity for stronger review; no rows closed. Evidence: evidence/PK45.03-npc-number-input.md.

## 2026-11-04 — PK45.03 NPC string input

Traced 0x01D4 string prompt through `UINpcInputWnd` and the variable 0x01D5 reply. The UI has no string character cap and the client allows u16 packet lengths; pinned server audit records bounded storage that may truncate. Server source is absent here, so encoding, exact limit, and boundary behavior need a stronger source/profile review. No rows closed. Evidence: evidence/PK45.03-npc-string-input.md.

## 2026-11-04 — PK45.03 NPC menu input

Traced variable 0x00B7 through NPC ID/menu-string parsing, visible option mapping, and 7-byte 0x00B8 choice reply. Client retains source ordinals across empty segments, displays through choice 0xFE, and reserves 0xFF for cancel. Exact selected-server menu ordinal/cancel semantics and boundaries need stronger source review. No rows closed. Evidence: evidence/PK45.03-npc-menu-input.md.

## 2026-11-04 — PK45.03 dialogue text, next, and close

Traced 0x00B4 variable text and 0x00B5/0x00B6 fixed prompts through receive handlers, UI, and 0x00B9 next/close senders. Lock release and quest cleanup timing cross UI/session lifecycles; server continuation and send-failure semantics need stronger review. No rows closed. Evidence: evidence/PK45.03-dialogue-text-next-close.md.

## 2026-11-04 — PK45.03 actor state change

Traced client 13-byte 0x0119 and 15-byte 0x0229 branches through decoded fields, actor state message, and local cart-state updates. Historical server registry has no parser callback for these entries and does not prove selected-profile reachability. Defer source/profile and sit/cart lifecycle verification; no rows closed. Evidence: evidence/PK45.03-actor-state-change.md.

## 2026-11-04 — PK45.03 item pickup acknowledgment

Traced client 0x00A0 (23 bytes) and 0x02D4 (29 bytes) layouts through success handling to pickup animation, inventory, and pending-request cleanup. Selected-server reachability and nonzero-result cleanup/correlation remain open; no behavior changed. No rows closed. Evidence: evidence/PK45.03-item-pickup-ack.md.

## 2026-11-04 — PK45.03 cart metadata and clear

Traced fixed 14-byte 0x0121 cart metadata and 2-byte 0x012B clear through dedicated handlers and the isolated commerce receiver branches. Detailed item/equipment formats and cross-inventory synchronization remain deferred because the commerce parser is densely minified and spans trade/vending state. No rows closed. Evidence: evidence/PK45.03-cart-metadata-clear.md.

## 2026-11-04 — PK45.03 actor walking decoder

Inspected legacy 0x007B/0x01DA packet path: minimum 60-byte parsing and classic-vs-shifted candidate scoring for certain 0x007B lengths/type bytes. This heuristic and variable movement variants need stronger profile/capture review plus PK45.04 replay; no rows closed. Evidence: evidence/PK45.03-actor-walking-decoder.md.

## 2026-11-04 — PK45.03 actor position correction

Traced fixed 10-byte 0x0088/0x01FF through GID/signed-coordinate decoding and authoritative actor/local position correction, including pending move-ack clear. Selected-server branch semantics and same-cell/high-jump replay remain open. No rows closed. Evidence: evidence/PK45.03-actor-position-correction.md.

## 2026-11-04 — PK45.03 0x0086 actor movement update

Traced fixed 16-byte 0x0086 framing and handler fields/state updates. Handler itself tolerates 12–15 bytes and substitutes session server time; verify framing rejects truncation and replay local/remote movement timing. No rows closed. Evidence: evidence/PK45.03-actor-move-update.md.

## 2026-11-04 — PK45.03 actor resurrection

Traced fixed 8-byte 0x0148, GID field, ignored tail, and internal message 98 death-state reset plus local quest/dead/item-ack cleanup. Trailing fields and server event ordering need source/replay verification. No rows closed. Evidence: evidence/PK45.03-actor-resurrection.md.

PK45.03 pickup follow-up confirmed failed acknowledgments leave the request eligible for throttled retry until the ground item disappears; no retry ceiling/terminal UI was found. Evidence updated; no rows closed.

## 2026-11-04 — PK45.03 actor direction

Traced 9-byte 0x009C GID/head/body direction fields through actor rotation, seated motion, PC head motion/billboard, and local session direction. Server field domains and seated animation parity need replay/source confirmation. No rows closed. Evidence: evidence/PK45.03-actor-direction.md.

## 2026-11-04 — PK45.03 ground-item disappearance

Traced fixed 6-byte 0x00A1 removing logical ground state/pending pickup; the per-update world sync deletes stale rendered CItems, and empty pickup queue resets retry timing. Server reasons and UI-frame/repeated-packet behavior need replay. No rows closed. Evidence: evidence/PK45.03-ground-item-disappear.md.

## 2026-11-04 — PK45.03 ground-item entry

Traced 17-byte 0x009D existing and 0x009E dropped layouts through logical upsert, world-item creation/resource/position update, stale-item removal, and one-shot drop animation. Server variant selection and visual resource replay remain open. No rows closed. Evidence: evidence/PK45.03-ground-item-entry.md.

## 2026-11-04 — PK45.03 inventory item removal

Traced 6-byte 0x00AF and 8-byte 0x07FA client layouts through pending throw acknowledgment and session partial/full stack removal. Extended prefix/zero-amount and selected-server semantics need source/replay verification. No rows closed. Evidence: evidence/PK45.03-inventory-item-remove.md.

## 2026-11-04 — PK45.03 equipment acknowledgment

Traced fixed 7-byte 0x00AA through pending-index clear and conditional wear-mask/appearance synchronization. Nonzero result currently triggers equipment update; server result convention and slot semantics need confirmation/replay. No rows closed. Evidence: evidence/PK45.03-equip-item-ack.md.

PK45.03 equipment follow-up traced 0x00AC unequip and 0x013C ammo acknowledgments. Both update session wear masks/appearance under distinct branches; server result semantics and shared pending-index correlation remain open. No rows closed.

## 2026-11-04 — PK45.03 action-failure notification

Traced fixed 4-byte 0x013B type field, chat messages, and types 0–2 retry/chase cleanup. Type 3 is job dependent; selected-server mapping and replay remain open. No rows closed. Evidence: evidence/PK45.03-action-failure.md.

## 2026-11-04 — PK45.03 actor appearance/font updates

Traced 8-byte 0x02EF font and 11-byte 0x01D7 look packets through client fields/session/billboard effects. Type-12 robe stores no value in this handler; font invalidation/resource use and look catalog parity need visual/source review. No rows closed. Evidence: evidence/PK45.03-actor-appearance-font.md.

## 2026-11-04 — PK45.03 skill-failure acknowledgment

Traced fixed 10-byte 0x0110 fields and result-0 motion/emotion cleanup plus notice dispatch. The resolver's large skill/type/cause message matrix needs focused parity review and replay. No rows closed. Evidence: evidence/PK45.03-skill-failure-ack.md.

## 2026-11-04 — PK45.03 emotion request/ack

Traced chat emotion command to 3-byte 0x00BF, one-second request throttle, 7-byte 0x00C0 reply, actor effect mapping, and local pending-flag clear. Server handling, delayed/rejected reply cleanup, and asset parity remain open. No rows closed. Evidence: evidence/PK45.03-emotion-request-ack.md.

## 2026-11-04 — PK45.03 player skill list/update

Traced variable 0x010F 37-byte entry loop and fixed 11-byte 0x010E incremental update into session skill state. List clears before parsing and ignores incomplete tail; update inserts unknown IDs without the list range filter. Verify selected-server shape, framing guarantees, and list ordering/bounds. No rows closed. Evidence: evidence/PK45.03-player-skill-list-update.md.

## 2026-11-04 — PK45.03 companion skill lists/updates

Traced shared 37-byte list schema for variable 0x0235/0x029D and 11-byte updates 0x0239/0x029E through target filters/session stores. Both updates clear a shared upgrade-request flag without visible ID correlation; confirm overlap invariant and selected-server ranges. No rows closed. Evidence: evidence/PK45.03-companion-skill-lists.md.

## 2026-11-04 — PK45.03 player skill-add

Traced fixed 39-byte 0x0111 reusing the 37-byte skill record and inserting to session. It clears the shared upgrade-request flag before record validation and lacks the full-list ID-range filter; verify server semantics and ordering. No rows closed. Evidence: evidence/PK45.03-player-skill-add.md.

## 2026-11-04 — PK45.03 shortcut-key list

Traced fixed 191-byte 0x02B9 snapshot into 27 session slots (9×3), including clear-before-replace and type/empty normalization. Server slot ordering/domain and UI refresh replay remain open. No rows closed. Evidence: evidence/PK45.03-shortcut-key-list.md.

## 2026-11-04 — PK45.03 normal inventory list

Traced variable 0x00A3/0x01EE/0x02E8 with 10/18/22-byte entries through item fields and session replacement. Exact-empty clears, nonempty short payload preserves stale items, and trailing fragments are ignored; confirm valid emitter constraints and variant use. No rows closed. Evidence: evidence/PK45.03-normal-inventory-list.md.

## 2026-11-04 — PK45.03 equipment inventory list

Traced variable 0x00A4/0x01EF/0x02D0 with 20/24/26-byte entries through replacement, ammo preservation, inventory merge, and appearance rebuild/sync. Selected-server fields and empty/partial/duplicate behavior need profile replay. No rows closed. Evidence: evidence/PK45.03-equipment-inventory-list.md.

## 2026-11-04 — PK45.03 storage lifecycle

Traced 0x00F2 open status, 0x00A5/01F0/02EA and 0x00A6/02D1 lists, 0x00F4/01C4 adds, 0x00F6 removes, and 0x00F8 close through session/UI. Open/list do not clear storage rows; close does. Verify server open/close ordering and transfer pairing. No rows closed. Evidence: evidence/PK45.03-storage-lifecycle.md.

## 2026-11-04 — PK45.03 storage password framing

Added source-backed fixed sizes for 0x023A (4) and 0x023C (6); without them the decoder rejected these registered packets and the connection treated them as unknown opcode. The handlers remain placeholders (prompt is a notice; result is log-only); full password UI/protocol semantics need source review. No rows closed. Evidence: evidence/PK45.03-storage-password.md.

## 2026-11-04 — PK45.03 whisper flow

Traced variable 0x0096 request, variable 0x0097 receive, and 3-byte 0x0098 result. Pending echo stores only the latest name/text and does not consume on ack; verify server serialization and limits before changing. No rows closed. Evidence: evidence/PK45.03-whisper-flow.md.

## 2026-11-04 — PK45.03 chat notifications

Traced variable 0x008D normal chat with AID/text/bubble and 0x008E player chat text/channel. The 0x008D empty-text fallback to offset 4 is trace-based and not profile-confirmed; verify with selected captures. No rows closed. Evidence: evidence/PK45.03-chat-notifications.md.

PK45.03 chat follow-up added 0x0109 party chat: variable packet, AID@4/text@8, nonempty party-channel record only. Verify common payload/text profile and the 0x008D fallback with captures. No rows closed.

## 2026-11-04 — PK45.03 broadcast/battlefield chat

Traced variable 0x009A/0x01C3 broadcast parsing (including legacy text/color heuristics) and 0x02DC battlefield sender/text offsets. Prefix/color/profile semantics need selected-source captures and replay. No rows closed. Evidence: evidence/PK45.03-broadcast-chat.md.

## 2026-11-04 — PK45.03 friend handlers

Traced friend list/state/request/add/delete routing, framing, fields and session/UI effects. The variable-list parser ignores partial trailing entries; selected-server field semantics, malformed behavior, and independent replay remain open. No rows closed. Evidence: evidence/PK45.03-friend-handlers.md.

## 2026-11-04 — PK45.03 party state variants

Traced party option/removal/position and party HP/member-add variants through client framing and session/UI/actor effects. The 0x0101 fixed-six-byte framing conflicts with the handler's optional reads at bytes 6–7; HP/member-add select basic and extended profile opcodes. Effective server/profile semantics and independent fixtures/replays remain open. No rows closed. Evidence: evidence/PK45.03-party-options-removal-position.md, evidence/PK45.03-party-hp-update.md, evidence/PK45.03-party-member-added.md.

## 2026-11-04 — PK45.03 party create, invite, and list

Traced create acknowledgement, profile-selected invite ack/request variants, and party-list replacement through pending-create state, invite UI, member/HP caches and party UI. Result/profile meanings, create-option ordering, and partial-list behavior need selected-server fixtures and production replay. No rows closed. Evidence: evidence/PK45.03-party-create-invite-list.md.

## 2026-11-04 — PK45.03 recovery notification

Traced 0x013D's six-byte status/amount fields, HP/SP vitals updates, recovery number and effect branches. Verify amount semantics, unknown type handling, max-zero behavior, and effect/UI behavior with selected-server evidence and production replay. No rows closed. Evidence: evidence/PK45.03-recovery-notification.md.

## 2026-11-04 — PK45.03 ammunition equip acknowledgement

Traced fixed 0x013C item-index handling: exclusive 0x8000 ammo wear mask update and pending-equip ack reset. Confirm zero/unknown index semantics, selected-server field identity, and inventory/UI consequences with fixtures and production replay. No rows closed. Evidence: evidence/PK45.03-ammunition-equip-ack.md.

PK45.03 deferral: `HandleSkillCastAck` (0x013E, 24 bytes) is only partially traced. Its instant/timed cast split, special motion-freeze skills, actor-type-specific actions, and target/ground effect branches require a dedicated source/replay pass before a reliable leaf can be written. Leave this for stronger review; no row is closed.

## 2026-11-04 — PK45.03 attack chase range

Traced fixed 0x013A range field and minimum-one clamp into chase state. Confirm units and zero semantics against selected-server evidence, then replay boundary values. No rows closed. Evidence: evidence/PK45.03-attack-chase-range.md.

## 2026-11-04 — PK45.03 attack distance hint

Traced fixed 0x0139 signed-cell/range payload into target-keyed chase-hint state. Downstream chase state machine, clearing/expiry and retry semantics need dedicated review with server evidence and replay. No rows closed. Evidence: evidence/PK45.03-attack-distance-hint.md.

## 2026-11-04 — PK45.03 self stat update

Traced fixed 0x0141 type/base/plus fields and the separate selected-character, session plus-stat, and in-world actor updates. Unknown-type behavior differs with world state; verify server field semantics, valid ranges, and status UI consequences with fixtures and replay. No rows closed. Evidence: evidence/PK45.03-self-stat-update.md.

## 2026-11-04 — PK45.03 status-point cost

Traced fixed 0x00BE type/cost fields into the six per-stat cost values and status-window invalidation. Verify IDs/units and unknown-type behavior with selected-server evidence and replay. No rows closed. Evidence: evidence/PK45.03-status-point-cost.md.

## 2026-11-04 — PK45.03 server time notification

Traced profile-selected 0x007F and the first/subsequent tick handling, late-packet tolerance, sync flag, and three-sample ping average. Validate tick/epoch, request lifecycle and wrap/timing cases with independent traces and replay. No rows closed. Evidence: evidence/PK45.03-server-time-notification.md.

## 2026-11-04 — PK45.03 ban notification

Traced fixed 0x0081 handling through pending-action/inventory/chat clears, transport disconnect and login-mode switch. Payload reason is ignored; selected-server semantics and reset completeness need source and lifecycle replay. No rows closed. Evidence: evidence/PK45.03-ban-notification.md.

## 2026-11-04 — PK45.03 return-to-character-select acknowledgement

Traced fixed 0x00B3 result/action gate, pending flag, and login teardown. Verify server result meaning, flag consumer/reset, duplicate/timeout behavior with production replay. No rows closed. Evidence: evidence/PK45.03-return-character-select-ack.md.

## 2026-11-04 — PK45.03 disconnect acknowledgement

Traced fixed 0x018B result/action decision, refusal event, and exit-to-Windows disconnect/quit path. Confirm server result codes and cleanup lifecycle; replay duplicate and timeout races. No rows closed. Evidence: evidence/PK45.03-disconnect-ack.md.

## 2026-11-04 — PK45.03 login refusal

Traced fixed 0x006A framing and the handler's two-field auth/account reset. Refusal payload, surrounding login transition, and stale-state behavior need selected-profile evidence and replay. No rows closed. Evidence: evidence/PK45.03-login-refusal.md.

## 2026-11-04 — PK45.03 login acceptance

Traced variable 0x0069 framing and fixed auth/account/sex session fields. Verify declared length, profile layout, sex values and next login handoff with server evidence and replay. No rows closed. Evidence: evidence/PK45.03-login-acceptance.md.

## 2026-11-04 — PK45.03 skill unit disappearance

Traced fixed 0x0120 unit ID through world skill-list/game-object removal and object deletion. Confirm ID semantics and duplicate/missing-ID behavior with selected-server fixture and replay. No rows closed. Evidence: evidence/PK45.03-skill-unit-disappearance.md.

PK45.03 deferral: `HandleSkillUpdate` (0x01AC, 6 bytes) resolves the u32 skill-unit ID and sends actor message 119 only when the unit job is greater than 138 and at most 153. The message contract, job range provenance, and unit lifecycle effects were not established; defer this leaf for stronger review rather than inferring behavior from the numeric message ID.

## 2026-11-04 — PK45.03 talkbox chat contents

Traced fixed 0x0191 payload through skill-unit lookup and raw content-pointer dispatch (message 7). Actor message contract, buffer lifetime, terminator/encoding and presentation need focused receiver review and production replay. No rows closed. Evidence: evidence/PK45.03-talkbox-chat-contents.md.

PK45.03 deferral: `HandleGuildEmblem` (0x0152) inflates a zlib payload, validates BMP header/dimensions, and reconciles pending local emblem bytes. The compressed-size/malformed-image boundary and pending-upload/resource lifecycle need dedicated security and replay review; defer a full leaf for stronger review.

## 2026-11-04 — PK45.03 local guild membership

Traced fixed 0x016C guild identity/permission fields, guild-change resets, and optional live-player/name-list propagation. Verify field semantics, leave-guild reset and UI/cache behavior with selected-server evidence and replay. No rows closed. Evidence: evidence/PK45.03-local-guild-membership.md.

PK45.03 deferral: `HandleGuildWindowPacket` is shared by multiple guild-window opcodes and delegates parsing/mutation to `ro::guild::ReadWindowPacket`, then reconciles member permissions and titles. The variant schemas and UI/cache lifecycle need a dedicated per-opcode source/fixture audit; defer a combined leaf for stronger review.

## 2026-11-04 — PK45.03 pet status

Traced fixed 0x01A2/35 parser into atomic session pet-status replacement, including signed field decoding and malformed no-op behavior. Confirm selected-server field types/encoding and malformed policy with fixtures and replay. No rows closed. Evidence: evidence/PK45.03-pet-status.md.

## 2026-11-04 — PK45.03 actor guild change

Traced fixed 0x01B4 actor/local guild ID and emblem-version update, including `SetGuildInfo` job/version guards. Compare local cache reset with 0x016C behavior and validate eligible actors/zero values using selected-server evidence and replay. No rows closed. Evidence: evidence/PK45.03-actor-guild-change.md.

## 2026-11-04 — PK45.03 status change acknowledgement

Traced fixed 0x00BC success gate, combat-stat session mappings, primary attribute updates, ASPD conversion, player sync and status-window invalidation. Confirm IDs/units and boundary/unknown behavior with selected-server evidence and replay. No rows closed. Evidence: evidence/PK45.03-status-change-ack.md.

## 2026-11-04 — PK45.03 status summaries

Traced 0x00BD/44 and 0x0214/42 layouts, six base/cost pairs, fourteen combat fields and local actor/UI synchronization. 0x00BD includes status points and converts raw ASPD; 0x0214 omits points and treats ASPD as display value. Confirm effective profile and units with server evidence/replay. No rows closed. Evidence: evidence/PK45.03-status-summaries.md.

## 2026-11-04 — PK45.03 self status parameters

Traced 0x00B0/0x00B1 shared u16-type/u32-value path across vitals, experience, levels, points, combat values, speed/path retiming and cart state. Confirm selected-server type map, shared actor experience semantics, and level-up presentations with server evidence and replay. No rows closed. Evidence: evidence/PK45.03-self-status-parameters.md.

PK45.03 deferral: `HandleSkillCastCancel` (0x01B9) resolves an actor and sends messages 84, 87 and 83, then clears motion freeze/state only for casting states. The message receivers span actor/effect behavior and were not fully traced; defer this leaf for a dedicated actor-state review.

## 2026-11-04 — PK45.03 sound loop

Traced fixed 0x01D3 filename/action/duration/AID fields, one-shot and repeated playback, and stop-by-filename removal. Validate duration/action semantics, asset path handling, duplicate loops and teardown cleanup with selected-server evidence and replay. No rows closed. Evidence: evidence/PK45.03-sound-loop.md.

## 2026-11-04 — PK45.03 combo delay

Traced fixed 0x01D2 AID/tick pair into session cache and live actor, with later skill-action consumption. Verify tick semantics and cache lifecycle/cleanup using selected-server evidence and replay. No rows closed. Evidence: evidence/PK45.03-combo-delay.md.

## 2026-11-04 — PK45.03 NPC shop deal type

Traced fixed 0x00C4 NPC ID into active interaction state, shop-window closure and buy/sell chooser. Verify NPC/session lifecycle and UI behavior with selected-server evidence and replay. No rows closed. Evidence: evidence/PK45.03-npc-shop-deal-type.md.

## 2026-11-04 — PK45.03 NPC shop lists/results

Traced 0x00C6/0x00C7 variable row parsers, inventory eligibility, chooser/list UI transitions, and 0x00CA/0x00CB result/suppression cleanup. Confirm server layouts, result meanings, partial-tail and suppression lifecycle with fixtures/replay. 0x00C8 remains a separate unregistered framed opcode. No rows closed. Evidence: evidence/PK45.03-npc-shop-lists-results.md.

## 2026-11-04 — PK45.03 guild departure

Traced fixed 0x015A leave and 0x015C expulsion variants through member/position removal, local guild/window/skill resets, actor emblem clear and message. Confirm payload/reason semantics and refresh behavior with server evidence and replay. No rows closed. Evidence: evidence/PK45.03-guild-departure.md.

## 2026-11-04 — PK45.03 guild creation acknowledgement

Traced fixed 0x0167 result messages and success cache invalidation preceding membership via 0x016C/name refresh. Confirm code meanings and ordering with server evidence and replay. No rows closed. Evidence: evidence/PK45.03-guild-create-ack.md.

## 2026-11-04 — PK45.03 guild member position

Traced fixed 0x01EB AID/signed-coordinate branch into guild-position cache, including local-account suppression and negative-coordinate removal. Confirm coordinate units/self key and cache lifecycle with server evidence and replay. No rows closed. Evidence: evidence/PK45.03-guild-member-position.md.

## 2026-11-04 — PK45.03 party-channel notification

Traced variable 0x017F offset-4 text extraction into party-channel chat without sender/bubble mutation. Compare format with 0x0109 and validate text boundaries/encoding with selected-server fixtures and replay. No rows closed. Evidence: evidence/PK45.03-party-channel-notification.md.

## 2026-11-04 — PK45.03 guild notification results

Traced 0x0173 alliance and 0x0181 opposition results, 0x015F disband notice, and 0x015E result/cleanup branches. Confirm server code meanings and disband cleanup completeness with selected-source evidence and replay. No rows closed. Evidence: evidence/PK45.03-guild-notification-results.md.

## 2026-11-04 — PK45.03 guild alliance invitation

Traced fixed 0x0171 guild-ID/name payload into commerce prompt state without direct window creation. Production prompt consumer, expiry/replacement and accept/reject flow need separate UI/source audit and replay. No rows closed. Evidence: evidence/PK45.03-guild-alliance-invitation.md.

## 2026-11-04 — PK45.03 guild skill list

Traced variable 0x0162 37-byte records, SKID filtering, point update and UI invalidation. Flagged partial-tail behavior and signed-16 packet-length cast as schema/boundary review items. Verify against server fixtures and replay; no rows closed. Evidence: evidence/PK45.03-guild-skill-list.md.

## 2026-11-04 — PK45.03 guild relation removal

Isolated the 0x0184 branch from the shared guild-window parser: fixed 10-byte ID/hostility key removes matching relation entries, then outer handler may refresh local position-derived permissions. Other guild-window variants remain deferred. No rows closed. Evidence: evidence/PK45.03-guild-relation-remove.md.

PK45.03 guild relation follow-up: added the 0x0185 add/replace branch to the existing relation contract. It keys updates by `(guild ID, hostile)` and then recomputes local-position-derived permissions/title through the outer handler. No rows closed.

## 2026-11-04 — PK45.03 guild member record

Traced fixed 0x0176/0x0182 shared member record offsets, pair-key upsert and online-count recomputation. Flagged unused bytes/defaulted fields and replacement semantics for server/schema review. No rows closed. Evidence: evidence/PK45.03-guild-member-record.md.

## 2026-11-04 — PK45.03 guild window menu flags

Isolated 0x014E fixed flags update in the shared guild-window parser; UI action gating consumes these bits. Verify flag meanings, lifecycle and refresh behavior with selected-server evidence and UI replay. No rows closed. Evidence: evidence/PK45.03-guild-window-menu-flags.md.

## 2026-11-04 — PK45.03 guild position names

Isolated 0x0166 variable 28-byte position-name rows, whole-packet stride validation, keyed update and preservation of existing mode/tax. Verify full/delta semantics and stale-position/UI behavior with selected-server evidence and replay. No rows closed. Evidence: evidence/PK45.03-guild-position-names.md.

- 2026-11-04: Added PK45.03 guild position mode/tax evidence for 0x0160/0x0174 (16/40-byte rows), including preserved fields and alignment rejection. Server semantics and replay remain open; no rows closed.

- 2026-11-04: Added PK45.03 evidence for 0x01B0 fixed 11-byte framing and empty ignore handler. Packet identity/direction/semantics remain unresolved; no rows closed.

- 2026-11-04: Grouped the 18 router registrations targeting empty HandleIgnorePacket; recorded fixed/variable framing split. Packet meanings remain unresolved and no plan rows closed.

- 2026-11-04: Traced PK45.03 0x011C warp destination list parsing and skill-selection UI transition. Server semantics and replay remain open; no rows closed.

- 2026-11-04: Traced PK45.03 self movement ACK 0x0087, packed coordinate decode and actor/world synchronization. Packet profile ownership remains unresolved and replay is open; no rows closed.

- 2026-11-04: Traced PK45.03 0x01AD making-arrow item list and skill-select UI behavior, including warp-cancel send failure. Server semantics and replay remain open; no rows closed.

- 2026-11-04: Traced PK45.03 variable 0x017B equipment index list and composition-window card/list update. Packet ownership and replay remain open; no rows closed.

- 2026-11-04: Traced PK45.03 variable 0x0177 item-identification index list and identify-window selection/scroll reset. Server semantics and replay remain open; no rows closed.

- 2026-11-04: Traced PK45.03 fixed 0x0179 identify result/index update and notice behavior. Result semantics and route ownership remain provisional pending server evidence/replay; no rows closed.

- 2026-11-04: Traced PK45.03 fixed 0x017D composition ACK, item-slot mutation/consumption, and result notices. Server semantics and edge-case replay remain open; no rows closed.

- 2026-11-04: Traced PK45.03 0x0215 skill-message code mapping and notices; packet-size registration is absent, leaving framing/reachability for stronger source review. No rows closed.

- 2026-11-04: Traced PK45.03 0x0147 auto-run skill dispatch and profile-selected Merchant Identify request. Ownership/layout remain provisional pending server confirmation and replay; no rows closed.

- 2026-11-04: Traced PK45.03 0x01CD seven-skill read and append-only spell-window messages; packet-size registration is absent and lifecycle/replacement behavior needs stronger review. No rows closed.

- 2026-11-04: Traced PK45.03 0x0145 image-name/type extraction and cutin-state update; packet-size registration is absent, leaving framing and asset behavior for stronger review. No rows closed.

- 2026-11-04: Traced PK45.03 fixed 0x01B3 64-byte image name/type and shared cutin-state path, contrasting 0x0145. Asset and server semantics/replay remain open; no rows closed.

- 2026-11-04: Traced PK45.03 0x0196 status icon fields and local/remote actor update gate. Detailed ApplyActorStatusVisual mapping deferred for stronger review; protocol semantics/replay open. No rows closed.

- 2026-11-04: Traced PK45.03 0x01CF devotion list framing and literal actor/effect calls. AID-as-vector effect payload and effect contract deferred for stronger review; server semantics/replay open. No rows closed.

- 2026-11-04: Traced PK45.03 0x043F timed status icon path through the shared status helper; duration units, trailing 12 bytes and visual mapping need stronger review. No rows closed.

- 2026-11-04: Traced PK45.03 0x01D1 blade-stop actor visual mutations for both flag branches. Animation parameters, AID resolution and cleanup semantics deferred for stronger review; no rows closed.

- 2026-11-04: Traced PK45.03 0x01D0 spirit effect handler, job-based effect IDs and hidden/visible branches. Effect/job semantics deferred for stronger review; replay/server validation open. No rows closed.

- 2026-11-04: Traced PK45.03 0x0284 actor/vital/effect handling; packet-size registration is absent and effect/color/RNG semantics need stronger review. No rows closed.

- 2026-11-04: Traced PK45.03 0x01AC AID lookup/job filter and message-119 trap-body reveal. Job-range/protocol semantics remain provisional pending server evidence/replay; no rows closed.

- 2026-11-04: Traced PK45.03 0x028A show-effect fields, level-99 message and conditional Assumptio launch. Bit/state lifecycle and server semantics/replay remain open; no rows closed.

- 2026-11-04: Traced PK45.03 0x01E1 spirit effect duplicate suppression and hidden/visible handling, compared with 0x01D0. Job/effect semantics and complementary packet contract deferred for stronger review; no rows closed.

- 2026-11-04: Traced PK45.03 0x01B9 cast-cancel message order and conditional actor-state reset. Message 84/87/83 receiver effects and ordering deferred for stronger review; no rows closed.

- 2026-11-04: Added scoped PK45.03 0x013E top-level cast ACK evidence. The full 145-line timing/actor/target/effect branch matrix is deferred for stronger review and selected-server replay; no rows closed.

- 2026-11-04: Traced PK45.03 0x0152 zlib/BMP gate, emblem storage and pending-payload confirmation. Full bitmap validation/race semantics deferred for stronger review; server confirmation/replay open. No rows closed.

- 2026-11-04: PK08.04 follow-up: current build has RO_BUILD_SKILL_REPLAY_TESTS=OFF; focused target build failed because its generated project is absent. Existing 0x007E test covers the builder, not the production CGameMode send helper, so PK08.04 remains BLOCKED on production-path verification.

- 2026-11-04: Traced PK45.03 shared guild-window opcode 0x016D online member update, count recomputation and offline position-cache erase. Server semantics/replay remain open; no rows closed.

- 2026-11-04: Traced PK45.03 variable 0x0156 member position rows and local permission/title recomputation. Full/delta semantics and replay remain open; no rows closed.

- 2026-11-04: Traced PK45.03 0x014C 32-byte relation records and validate-before-clear snapshot replacement. Server field meanings and replay remain open; no rows closed.

- 2026-11-04: Traced PK45.03 0x0150/0x01B6 guild summary variants, offsets and local guild synchronization. Skipped bytes/profile semantics need server review; no rows closed.

- 2026-11-04: Traced PK45.03 fixed 0x016F guild notice title/body storage. No consumer found in scoped source; text/display lifecycle and replay remain open. No rows closed.

- 2026-11-04: Traced PK45.03 variable 0x0163 ban snapshot, 88-byte rows and validate-before-clear behavior. Text/skipped bytes and consumer semantics need server review/replay; no rows closed.

- 2026-11-04: Traced PK45.03 fixed 0x01F2 member online/appearance update, enum validation and offline cache erase; relationship to 0x016D remains for server/profile review and replay. No rows closed.

- 2026-11-04: Traced PK45.03 variable 0x0154 104-byte roster replacement and consumed/unconsumed fields. Server schema and separate online-count behavior need stronger review/replay; no rows closed.

- 2026-11-04: Traced PK45.03 0x016A guild invitation prompt, QML accept/decline and 0x016B send; 0x0169 result notices. Server code meanings/replay remain open; no rows closed.

- 2026-11-04: Traced PK45.03 commerce 0x0249 mail result and 0x00D1 whisper-preference notices/status. Result correlation/value semantics and replay remain open; no rows closed.

- 2026-11-04: Traced PK45.03 commerce cart-list rows for 0x0122/0x0123/0x01EF/0x0297/0x02D2/0x02E9, including stride validation, conversion and per-category replacement. Server schema and replay remain open; no rows closed.

- 2026-11-04: Traced PK45.03 commerce 0x0124/0x01C5 cart additions, 0x0125 subtraction/removal, 0x012C transfer notice and 0x012D vending-window setup. Server field/status meanings and replay remain open; no rows closed.

- 2026-11-04: Traced PK45.03 commerce 0x0131/0x0132 vending boards and 0x0133/0x0135/0x0136/0x0137/0x0138 shop list, purchase, draft and open-result transitions. Server schema/status meanings and replay remain open; no rows closed.

- 2026-11-04: Traced PK45.03 trade invitations and client trade state handlers 0x00E5/0x01F4, 0x00E7/0x01F5, 0x00E9/0x00EA, 0x00EC/0x00EE/0x00F0/0x00F1. Actor/status interpretation and replay remain open; no rows closed.

- 2026-11-04: Traced PK45.03 commerce 0x02D7 viewed-equipment header, 26-byte rows and equipment-window replacement. Header/server row semantics and replay remain open; no rows closed.

- 2026-11-04: Started PK45.04 with a bounded commerce source self-review; all explicit Commerce.cpp receive labels now have PK45.03 evidence references. Production replay build blocked by Windows SDK registry access during zlib FetchContent; no replay/UI lifecycle checks ran and no rows closed.

- 2026-11-04: Traced PK46.02 map continuous-attack request from monster click/retry through PacketVer22/23 serializers to selected 0x0190/0x0437 registrations. Legacy profile and final action/parser lifecycle remain open; no discovery row closed.

- 2026-11-04: Traced PK46.02 ground-item pickup flow and PacketVer 0x00F5 layout; found explicit legacy 0x009F/6-byte send conflicts with selected server's 0x009F UseItem/14-byte registration. No discovery row closed.

- 2026-11-04: Traced PK46.03 occupied-character delete UI through the 0x0068/46-byte sender and compared it with the selected server's 0x01FB/56-byte contract. Request is reachable but incompatible; no discovery row closed.

- 2026-11-04: Traced PK46.03 account-login packet selection, service/config gates and connect/hash send ordering. Deployed service/profile, credential security and SSO support remain open; no discovery rows closed.

- 2026-11-04: Started PK46.04 with bounded claim-to-source self-review for completed map/login-character traces. Full parity is premature with most PK46.02 requests and PK46.03 branches open; production replay/UI checks remain blocked/unavailable. No rows closed.

- 2026-11-04: Traced selected PACKETVER 20080910 recognized-class actor idle/spawn server emitters 0x02EE/60 and 0x02ED/59 against the current decoder; field-width and fallback/resource lifecycle questions remain open. No rows closed.

- 2026-11-04: Traced selected PACKETVER 20080910 0x02EC walking emitter (67 bytes) against the modern-font client decoder, including MoveData and tick offsets. Weapon/shield interpretation and interpolation lifecycle remain open; no rows closed.

- 2026-11-04: Traced PK46.02 inventory drag/drop request 0x0116 from UI and amount prompt through the 10-byte serializer to selected clif_parse_DropItem offsets/guards and failure notice. Replay/independent bytes remain open; no row closed.

- 2026-11-04: Traced PK46.02 inventory use from UI/profile serializer to selected 0x009F/0x0439 parser branches; explicit legacy 0x00A2/8-byte use send conflicts with selected 0x00A2/15-byte name request. No row closed.

- 2026-11-04: Traced PK46.02 common equip request 0x00A9 from UI/profile through the 6-byte server struct and equip parser. Ammo/location/result lifecycle remains open; no row closed.

- 2026-11-04: Traced PK46.02 unequip request 0x00AB from equipped-item UI action through the 4-byte sender to selected clif_parse_UnequipItem index/action path. Ack/UI lifecycle remains open; no row closed.

- 2026-11-04: Traced PK46.02 skill-up request 0x0112 from classic/Qt skill UI to the 4-byte sender and selected pc_skillup parser. Result lifecycle and replay remain open; no row closed.

- 2026-11-04: Traced PK46.02 sit/stand action values 0x02/0x03 through PacketVer22/23 ActionRequest layouts to selected server sit/stand branches and gates. Reply/replay remains open; no row closed.

- 2026-11-04: Traced PK46.02 return-to-save-point menu action to 0x00B2/type 0 request and selected pc_respawn parser branch. UI/result replay remains open; no row closed.

- 2026-11-04: Traced PK46.02 return-to-char-select request 0x00B2/type 1 through client pending-disconnect handling and selected server status/prevent_logout branches. Full transition/replay remains open; no row closed.

- 2026-11-04: Traced PK46.02 identify-item selection/cancel 0x0178 through the 4-byte sender to selected MC_IDENTIFY parser gate and server_index handling. Identify-result UI replay remains open; no row closed.

- 2026-11-04: Traced PK46.02 compose-card selection/cancel 0x017C through selected server dispatch. Full insertion validation/result lifecycle and replay remain open; no row closed.

- 2026-11-04: PK45.03 escalation recorded for unresolved 0x01CD automatic-spell-list framing/profile/server contract and storage-password prompt/send/result lifecycle. Kept both open for stronger source review; no rows closed.

- 2026-11-04: Extended PK45.03 guild-alliance evidence through the production Qt prompt, accept/decline/cancel actions, 10-byte 0x0172 sender, and selected server parser registry. Server semantics, expiry policy, and replay remain open; no row closed.

- 2026-11-04: Extended PK45.03 friend-request evidence through the Qt Friend Request prompt and 14-byte 0x0208 response to selected clif_parse_FriendsListReply. Server semantics, expiry policy, and replay remain open; no row closed.

- 2026-11-04: Extended PK45.03 party invite evidence through incoming request validation, native accept window lifecycle, 0x00FF reply and 0x00FC invite layouts, and selected server parser registrations. Server permission/state semantics, stale prompt policy, and replay remain open; no row closed.

- 2026-11-04: Added selected-server 0x016B guild-invitation reply registration (10 bytes, clif_parse_GuildReplyInvite) to PK45.03 evidence. Decision polarity, result semantics, and replay remain open; no row closed.

- 2026-11-04: Traced PK45.03 party creation option ordering: 0x00F9/26 create carries name only; successful ack may emit 0x0102/6 options request. Noted unchecked follow-up send and locally seeded option state for server/replay review; no row closed.

- 2026-11-04: PK45.04 completed a bounded invite-family source/registry self-review across five reply/invite packets. It records client-to-registry size/offset consistency and differing prompt failure behavior; selected handler semantics and runtime replay remain open; no rows closed.

- 2026-11-04: PK45.04 recorded party-list boundary behavior: clear-before-parse, floor division into 46-byte entries, ignored partial tail, cached HP restore, and one UI refresh. Malformed policy/server contract require replay/source review; no row closed.

- 2026-11-04: Rechecked PK45.04 replay availability: build-root-replay cache does not set RO_BUILD_PACKET_REPLAY_TESTS and its expected replay executable is absent. No runtime validation; existing CMake/SDK failure remains recorded.

- 2026-11-04: Extended PK45.03 sound-loop evidence through PlayWaveRelative CWave lookup and backend handoff. SFX name is not normalized at this call boundary; resource-manager key/path policy and server semantics remain open; no row closed.

- 2026-11-04: Traced PK46.02 warp-skill selection/cancel through the destination UI and 20-byte 0x011B sender; selected registry matches clif_parse_UseSkillMap. Cancel uses literal "cancel" sentinel; server semantics/replay remain open, no row closed.

- 2026-11-04: Completed a bounded source audit of ResolveSkillFailNotices: cause 0 skill/btype branches, all cause 1–10 IDs, setting gate, result-byte early return, and two-notice capacity. Server semantics and UI/replay remain open; no row closed.

- 2026-11-04: Completed the PK45.03 action-failure client switch audit: types 0–3, job-specific type-3 branches, unknown-type early return, and repeated retry-state clearing for types 0–2. Server mapping/replay remain open; no row closed.

- 2026-11-04: Extended PK45.03 actor-guild-change evidence with local snapshot replacement rules and exact CGameActor::SetGuildInfo eligibility/clear behavior. AID/GID domain, server semantics, and visual refresh remain open; no row closed.

- 2026-11-04: Verified PK45.03 0x01B4 against hash-pinned rAthena 20080910 PACKET_ZC_CHANGE_GUILD and clif_guild_emblem_area emitter: 12-byte AID/guild/emblem layout sent to AREA. Corrected client evidence from GID to AID; trigger lifecycle, visual/cache refresh, and replay remain open.

- 2026-11-04: Matched PK45.03 0x013B action-failure to hash-pinned rAthena ZC_ACTION_FAILURE/clif_arrow_fail: 4-byte self-send, type 0–3 documented semantics align with client messages/job branches. Reachability/repeat timing/UI replay remain open; no row closed.

- 2026-11-04: Matched PK45.03 0x0148 actor resurrection against hash-pinned server definition/emitter: 8-byte GID/type layout, type explicitly ignored and sent as zero, AREA broadcast then optional clif_spawn for disguised actors. Broader order/runtime replay remain open; no row closed.

- 2026-11-04: Matched PK45.03 storage emission to hash-pinned rAthena: empty storage emits no item-list frame, refresh sends 0x00F2 count/status, close is separate 0x00F8. Conditional stale-row risk depends on caller ordering; transfer correlation/replay remain open; no row closed.

- 2026-11-04: Found PK45.03 selected-server 0x0110 field partition mismatch: server 2008 uses btype i16@4 + itemId u16@6, while client reads u32@4. Recorded conditional impact and call-site itemId review for stronger follow-up; no row closed.

- 2026-11-04: Matched PK45.03 0x009C against hash-pinned server emitter: head direction 0–2, body 0–7, NPC head=0, plus disguised actor rewritten-ID SELF update. Seated animation and replay remain open; no row closed.

- 2026-11-04: Refined PK45.03 0x0110 finding: four current skill.cpp itemId call sites use causes 71/72, which the client resolver drops (it handles 0–10). Recorded exact file hashes and provenance caveat; verify against intended server revision/replay before code changes. No row closed.

- 2026-11-04: Verified PK45.03 party-member-added against hash-pinned 20080910 server: active opcode 0x01E9/81, AID/leader/offline/name/map fields match client offsets; trailing share flags are ignored by client. Alternate profile/UI use/replay remain open; no row closed.

- 2026-11-04: Matched PK45.03 0x02EF against hash-pinned server clif_font: selected PACKETVER enables the 8-byte account-ID/font-ID packet and area send. Corrected GID wording; render/resource parity remains open; no row closed.

- 2026-11-04: Extended PK45.03 guild alliance with hash-pinned 0x0171 inviter-AID/name and 0x0172 parser correlation; flag 1 means consent. Unpinned guild.cpp appears to send result 3 on decline, conflicting with pinned result-code docs; escalated for revision/provenance review and replay. No row closed.

- 2026-11-04: Matched PK45.03 friend reply 0x0208 against the pinned 20080910 registry/parser: 14-byte form, requester AID at 2, char ID ignored at 6, decision u32 at 10; acceptance requires reciprocal pending state. Documented result routing and auto-add behavior. Friend request-state implementation/list edge cases and replay remain open; no row closed.

- 2026-11-04: Matched PK45.03 guild alliance/opposition/disband ack code meanings and SELF recipients against pinned server emitters; client labels align. 0x015F emitter/callsite, cleanup completeness, and replay remain open. No row closed.

- 2026-11-04: PK45.03 party-create/reply follow-up: pinned 20080910 parser consumes name plus pickup/share flags for CreateParty2, and 0x00FF reply uses party ID u32 and flag u32 (0 reject, 1 accept). Business result selection, list-edge handling, and replay remain open; no row closed.

- 2026-11-04: Matched PK45.03 variable party-list client stride to pinned clif_party_info emitter: 28-byte header, 46-byte PACKETVER 20080910 member records, SELF or PARTY targeting. Malformed-tail/HP-cache and replay remain open; no row closed.

- 2026-11-04: PK45.03 guild relation: pinned clif_guild_delalliance contract confirms 0x0184 relation values (0 ally, 1 enemy), field order, and SELF send; 0x0185 field order is described but nearby emitter is commented out. Runtime add/callsite and replay remain open; no row closed.

- 2026-11-04: PK45.03 0x014C guild relation list matched to pinned clif_guild_allianceinfo: 4-byte header, 32-byte records, nonpositive guild IDs skipped, relation=opposition, SELF send; no guild returns without send. Replay/value-domain coverage remains open; no row closed.

- 2026-11-04: Resolved PK45.03 guild-summary profile: pinned PACKETVER 20080910 uses 0x01B6/114; 0x0150 is an older variant. Pinned emitter field values and SELF/no-guild behavior recorded. Old variant/text boundaries/replay remain open; no row closed.

- 2026-11-04: PK45.03 0x0154 guild roster matched to pinned 20080910 emitter: 4-byte header, 104-byte rows, online flag from member.online, empty intro, and SELF/no-guild behavior. Summary-online synchronization and replay remain open; no row closed.

- 2026-11-04: Escalated PK45.03 0x0176/0x0182 guild member-record contract: selected client handler is fixed 106 bytes, but pinned server search finds only bare size declarations and no corresponding emitter/schema; distinct 0x0154 roster must not be conflated. Needs stronger protocol-history/alternate-revision review; no row closed.

- 2026-11-04: Matched PK45.03 guild online-state 0x016D to pinned emitters: 14-byte AID/GID/online records; login helper batches other nonempty guild members and sends SELF, notice helper uses guild broadcast targets. First-notice UI timing and replay remain open; no row closed.

- 2026-11-04: PK45.03 0x014E menu flags matched to pinned emitter and Qt consumers: role masks 0xD7/0x57; known bits 01/02/04/10/80, 0x40 unknown and unused by Qt; live refresh remains unverified. No row closed.

- 2026-11-04: Matched PK45.03 0x0166 position-name list to pinned emitter: 20 ordered ID/name[24] rows, 28-byte stride, 564-byte frame, guild-required. Position mode/tax is a separate packet; replay/UI refresh remain open. No row closed.

- 2026-11-04: PK45.03 0x0160/0x0174 position info matched to pinned emitters: 20×16-byte list rows (324 total), 40-byte single-row updates; mode bits invite/expel, ranking TODO, payRate sourced from exp_mode while client labels tax. Exact exp_mode semantics are unpinned; stronger review/replay needed. No row closed.

- 2026-11-04: Matched PK45.03 0x0156 member-position updates to pinned emitter: 12-byte AID/GID/position rows sent to GUILD; current function sends one row and comments that intended behavior is post-commit batching. Call ordering/replay remain open; no row closed.

- 2026-11-04: Matched PK45.03 guild departure/expulsion to pinned 20080910 emitters: 0x015A name+reason/66 bytes; 0x015C name+reason+empty account name/90 bytes before 20100803; both use GUILD_NOBG. Recipient scope/order/replay remain open; no row closed.

- 2026-11-04: Matched PK45.03 0x0163 guild expulsion list to pinned 20080910 emitter: 88-byte name/account-name/reason rows, account name intentionally blank pre-20100803, zero-AID entries skipped, SELF send. Empty/replay/UI-consumer gaps remain; no row closed.

- 2026-11-04: Matched PK45.03 0x016F notice to pinned emitter: subject[60]+notice[120], SELF, and no send when both source fields empty or no guild. Reset/encoding/UI/replay remain open; no row closed.

- 2026-11-04: PK45.03 guild-create result correlation traced: pinned emitter is 3-byte SELF flag; current unpinned guild.cpp maps 1=in guild, 2=async same-name failure, 3=missing Emperium, 0=async success; blank name returns without ack. Source hash/revision caveat and replay recorded; no row closed.

- 2026-11-04: Matched PK45.03 0x0162 guild skill list to pinned emitter: 6-byte header/37-byte rows; filters invalid/unmet skills, sends points even for empty list, leader-only upFlag below max. Client consumes matching subset; boundary/reset replay remains open. No row closed.

- 2026-11-04: Resolved PK45.03 0x01F2 status layout against pinned server: PACKETVER branch is 20-byte AID/GID/status plus appearance u16s; 0x016D remains 14-byte SELF login snapshot. Historical cutoff comment and unpinned flag caller/replay remain open; no row closed.

- 2026-11-04: Matched PK45.03 0x0152 emblem response to pinned emitter: 12-byte header, exact compressed length, version at 8, direct session send, no packet for empty emblem. Inflate/BMP safety and UI refresh still need stronger security/replay review; no row closed.

- 2026-11-04: Escalated PK45.03 commerce small results 0x0249/0x00D1: focused selected-source search finds only bare 0x0249 size registration and no 0x00D1 occurrence; result polarity/correlation need stronger source/service or capture review. No row closed.

- 2026-11-04: Resolved PK45.03 guild invitation flow against pinned server: 016A guild ID/name prompt, 016B guild ID + u32 answer (0 refuse/1 accept), 0169 result flags 0–3 with client labels matching. Business validation/expiry/replay remain open; no row closed.

- 2026-11-04: PK45.03 trade receive semantics matched to pinned clif.cpp: invite result codes 0–5, 00EC who=0 self/1 other, 00E9 zero item ID for zeny, 00EE cancel, 00F0 success=0; client mappings align. Failure emitter, snapshot/delta, correlation and replay remain open; no row closed.

- 2026-11-04: Matched PK45.03 vending lifecycle to pinned emitters: 0133/0136 22-byte rows, 0133 filters unresolved item metadata, selected 2008 omits newer open-store ack; 0135 documented results and 0137 seller delta; boards 0131/0132. Result 3/order/replay remain open; no row closed.

- 2026-11-04: Matched PK45.03 cart deltas to pinned server: 20080910 selects 0x01C5/22-byte item-type variant (not legacy 0124), 0125 index/amount removal, and 012C failure codes 0=weight, 1=count. Invalid-slot send suppression noted; transaction/replay remain open. No row closed.

- 2026-11-04: Resolved PK45.03 cart-list profile/schema: 20080910 uses 02E9 normal/22 bytes and 02D2 equip/26 bytes; server emits only nonempty categories, client replaces matching category and preserves other. Type-classification equivalence and fixtures/replay remain open; no row closed.

- 2026-11-04: Matched PK45.03 02D7 viewed-equipment response to pinned server: same-map + visibility/permission gate, 43-byte header, 26-byte equipped item rows. Current unpinned defines_pre config hash recorded and selects SAK 20080910 branch; verify provenance/replay before treating branch activation as authoritative. No row closed.

- 2026-11-04: Matched PK45.03 broadcasts to pinned emitters: 009A message@4 with blue/ssss prefixes for corresponding modes; 01C3 has 16-byte header with uint32 color and four int16 font fields. micc/tool variants, color byte order/rendering, and replay remain open; no row closed.

- 2026-11-04: Matched PK45.03 whisper request/ack against pinned server: 0096 name@4/message@28, target/message NUL validation and CHAT_SIZE_MAX-1 cap; 0098 statuses 0–3 align but carry no request ID, confirming concurrent-send correlation limitation. Inter-map ordering/replay remain open; no row closed.

- 2026-11-04: Matched PK45.03 013D recovery to pinned clif_heal: 6-byte type/amount, types 5 HP/7 SP, u16 saturation, SELF. Current unpinned pc.cpp also calls AP recovery; profile applicability is uncertain and escalated with hash. Replay remains open; no row closed.

- 2026-11-04: Matched PK45.03 013C ammunition ack to pinned clif_arrowequip: four-byte packet contains client index of EQI_AMMO, sent SELF. Empty-slot sentinel conversion, call ordering and stale-ack replay remain open; no row closed.

- 2026-11-04: Resolved 013C no-ammo sentinel for current source pairing: server client_index(-1) wraps to client index 1; valid inventory indices start at 2, so client clears ammo marks without assigning a real item. equip_index initialization is unpinned; revalidate provenance and replay. No row closed.

- 2026-11-04: Matched PK45.03 013A to pinned SP_ATTACKRANGE emitter: sends rhw.range as u16 to SELF with no clamp. Client clamps zero to one; determine whether zero can occur and replay range boundaries. No row closed.

- 2026-11-04: Matched PK45.03 0139 distance hint to pinned emitter fields and SELF send. Current unpinned unit.cpp calls on failed player distance check and applies temporary +1 chase range for moving/step-attack targets while packet carries base range; hash recorded. Chase state machine/replay remain open; no row closed.

- 2026-11-04: Matched PK45.03 00BE status cost update to pinned emitter/callsite: u16 status ID + u8 value, IDs 32–37 send pc_need_status_point for each base stat. Mapping matches client; enum/formula source unpinned, replay remains open. No row closed.

- 2026-11-04: Matched PK45.03 0141 self-stat update to pinned clif_couplestatus: u32 status/base/plus sent SELF; cases send base stat and derived battle-status bonus for six attributes. Numeric enum unpinned; cap/UI/boundary replay remain open. No row closed.

- 2026-11-04: PK45.03 00B0/00B1 server structs confirm 8-byte varID u16 + signed int32 value, SELF. Client decodes u32; SP_MANNER may be negative, so signedness requires stronger review with unpinned enum/range provenance. Level-up/effect/replay remain open; no row closed.

- 2026-11-04: Matched PK45.03 initial status 00BD to pinned clif_initialstatus: 44-byte SELF summary, status point/base-stat caps, six costs and combat fields, followed by per-attribute/range/ASPD updates. No 0214 struct/emitter found in selected tree; treat as alternate-profile path. Formula/level-up/replay remain open; no row closed.

- 2026-11-04: Matched PK45.03 server time 007F to pinned server request-response: direct u32 cast of t_tick, SELF; 007E/0360 parser records client tick then replies with current gettick. Not an epoch; timer unit/cadence and client estimator replay remain open. No row closed.

- 2026-11-04: Matched PK45.03 02B9 shortcut snapshot to server profile: 27×7-byte hotkey records, type/ID/count in slot order, SELF. HOTKEY_SAVING and MAX_HOTKEYS current config corroborates activation but mmo.hpp is unpinned; UI/persistence/replay remain open. No row closed.

- 2026-11-04: Matched PK45.03 player skill list to pinned clif_skillinfoblock: 010F 37-byte rows, derived skill fields/upFlag rules, SELF, 8192-byte cap; WE_CALLPARTNER sent separately as 0111. Large-list truncation/profile replay remain open; no row closed.
- 2026-11-04: Matched PK45.03 auto-spell list to selected server 2008 SAK branch: fixed 01CD/30-byte seven-u32 snapshot, SELF, classic choices gated by learned skill and required level; unavailable slots are zero. Sparse-list/UI replay and exact profile correspondence remain open; no row closed.
- 2026-11-04: Added PK45.03 companion skill-list server trace: homunculus 0235 variable 37-byte rows and 0239 update; mercenary 029D variable 37-byte rows with upgradable=0. Client/server revision, ordering, and request correlation still need replay; no row closed.
- 2026-11-04: PK45.03 0147 server trace confirms fixed 39-byte ZC_AUTORUN_SKILL notification to SELF with skill metadata and upgradable=false. Client handler is traced as sending a skill-use request only for Merchant Identify; cross-direction relationship/profile match is unresolved and needs stronger review/replay. No row closed.
- 2026-11-04: PK45.03 0215 selected-server trace confirms packed six-byte ZC_SKILLMSG (i32 message ID), SELF route, Gospel IDs 15–20 and Full Strip coating failure 28. Client dispatch reachability and exact string/profile mapping remain open; no row closed.
- 2026-11-04: Matched PK45.03 blade-stop 01D1 to server fixed 14-byte src block ID/dst block ID/u32 active flag; 0 inactive, 1 active, AREA broadcast. Client lookup/visual replay and animation interpretation remain open; no row closed.
- 2026-11-04: Matched PK45.03 cart metadata/clear to server: 0121 14-byte count/weight fields sent SELF; 012B header-only closes storage and removes client cart items while retaining count/weight. Update ordering and inventory/equipment synchronization remain open; no row closed.
- 2026-11-04: PK45.03 position correction selected-server trace confirms 0088 STOPMOVE with actor ID/x/y, AREA; clif_blown emits slide then fixpos, with disguised SELF ID copy. 01FF high-jump semantics/profile remain unconfirmed; no row closed.
- 2026-11-04: PK45.03 ground-item entry trace confirms 009D existing items to entering session (SELF) and 009E new drops (AREA); both fixed layouts derive fields from floor item, with variant-specific amount/subcell order. Visual/resource parity and replay remain open; no row closed.
- 2026-11-04: PK45.03 ground-item disappearance server trace confirms 00A1 carries only item object ID; no target => AREA broadcast, target session => SELF. No reason field; frame timing/repeated-ID replay remain open; no row closed.
- 2026-11-04: PK45.03 normal inventory server trace resolves 2008 profile to 02E8 stackable list; non-stackable items use separate 02D0 equipment list. Variable packets sent SELF; empty rows skipped, partial groups flushed, ammo update sent separately. Replay/field parity remains open; no row closed.
- 2026-11-04: PK45.03 party trace: pre-2009 server 0101 option packet (EXP option u32, PARTY or SELF by flag), 0105 member removal (AID/name/result), 0107 position (AID/x/y to same-map party); x/y=-1 removes map marker. Validate result semantics/UI with replay; no row closed.
- 2026-11-04: PK45.03 equipment inventory server trace resolves 2008 profile to variable 02D0 SELF list; rows include client index/item/type, identify/damage, equip location/wear state, refine/cards/expiry/bind type. Visual sprite field is later-version only. Interpret/replay remains open; no row closed.
- 2026-11-04: PK45.03 ban notification server trace confirms 0081 opcode + reason byte, followed by EOF; matches 3-byte client framing/disconnect. Server codes include closed/already logged in/timeout/full and restrictions. Client ignores reason; diagnostic/reset completeness remains open; no row closed.
- 2026-11-04: PK45.03 return-to-character-select server trace confirms 00B3 is 3-byte ZC_RESTART_ACK; result 1 means disconnect/char-select and is sent SELF. Callers, mode-flag consumption and lifecycle teardown replay remain open; no row closed.
- 2026-11-04: PK45.03 skill-unit disappear 0120 carries unit GID; selected server has targeted SELF and general AREA removal paths. Targeted Ice Wall removal also sends map-cell change. Duplicate/missing-ID and teardown replay remain open; no row closed.
- 2026-11-04: PK45.03 talkbox 0191 server trace confirms fixed 86-byte block ID + 80-byte message, bounded safestrncpy, AREA broadcast. Encoding/NUL policy and client text presentation/replay remain open; no row closed.
- 2026-11-04: PK45.03 01B3 server trace confirms fixed 64-byte image + type, bounded copy, SELF; documented types 0–4 placement and 255 clear-all. Asset encoding and rendering/replay remain open; no row closed.
- 2026-11-04: PK45.03 skill cast cancel 01B9 server trace confirms six-byte ZC_DISPEL with actor GID, AREA broadcast. Trigger causes and client message/effect receiver sequence need stronger review/replay; no row closed.
- 2026-11-04: Corrected PK45.03 017F classification: server identifies variable packet as ZC_GUILD_CHAT, excludes empty messages, caps to chat buffer with terminator and sends GUILD_NOBG. Client routes through HandleGuildNotification; presentation/channel equivalence needs replay; no row closed.
- 2026-11-04: PK45.03 NPC shop trace: C6 variable buy rows (base/discount/type/item ID), C7 eligible inventory sell rows (index/base/overcharge), both SELF; CA/CB one-byte result semantics documented. Complete-row framing confirmed; malformed tails, races and UI suppression lifecycle remain open; no row closed.
- 2026-11-04: PK45.03 028A server trace maps four fields to actor GID/sc->option/level/sc->opt3; normal AREA, disguised sequence AREA_WOS then two SELF variants (disguised GID, then real GID + invisible state). Client masks/effect lifecycle need replay; no row closed.
- 2026-11-04: PK45.03 043F focused search found no definition/emitter in selected server clif/packet sources; legacy status packet is not evidence of timed layout. Profile mismatch and 25-byte tail/duration semantics need stronger review/capture; no row closed.
- 2026-11-04: PK45.03 01D0 server confirms 8-byte actor ID + u16 count; servant/abyss-ball emitters populate character ID and count, default target SELF when absent. Client interprets as job-based effect trigger; semantic/profile mismatch and callsite scope need stronger review/replay; no row closed.
- 2026-11-04: PK45.03 0284 server emitter confirms 14-byte actor/effect/value u32 fields, caller-selected scope and disguised SELF copy using disguised ID. Client special handling for 331/332 and no-registry reachability still need matching callsite/profile/replay; no row closed.
- 2026-11-04: PK45.03 01E1 cross-check: server struct exists, but clif_spiritball emits PACKET_ZC_SPIRITS/header 01D0; no SPIRITS2 emitter use found in selected clif.cpp. Treat client effect semantics/profile pairing as stronger-review item; no row closed.
- 2026-11-04: PK45.03 login refusal trace confirms common AC_REFUSE_LOGIN 006A packed 23-byte opcode/error/unblock_time[20]; sender fills error and empty unblock time. Matches client framing; error mapping and login-flow cleanup need replay; no row closed.
- 2026-11-04: PK45.03 login acceptance cross-check found layout discrepancy: selected server 0069 sex@46, char-server rows@47; client trace reads sex@38/minimum 39 and omits declared-length validation. High-priority client/server profile review and capture needed before trusting handoff; no row closed.
- 2026-11-04: Identified ignored 01B0 as server ZC_NPCSPRITE_CHANGE (actor ID/type=0/class ID, 11 bytes); player classes are skipped and caller selects route. Client handler is empty, potential NPC sprite parity gap; verify intentionality/replay; no row closed.
- 2026-11-04: PK45.03 ignored group partial source cross-check: 0100 is client leave-party request; 0199 is server map-property notification; 02C9 is SELF party-invite preference during login. Remaining opcodes require per-packet source/capture review; no guessed handling, no rows closed.
- 2026-11-04: Extended PK45.03 ignored-group audit: 01D6 map type; 02B1/02B2 quest lists; 02D3 bind index; 02D9 config type/value; 02DA view-equip state; 0814/0816 buying-store packets gated after selected 2008. 01F8/0283/02D5/02E7 remain unresolved; no row closed.
- 2026-11-04: Ignored-group 0283 maps to map-server connection handoff: account ID notification followed by chrif_authreq; no selected map-source literal for 01F8/02D5/02E7. Group remains partial and needs matching revision/captures; no rows closed.
- 2026-11-04: Corrected ignored-opcode search scope: clif_packetdb has 01F8 size2, 02D5 size2, 02E7 variable, 0283 size6; framing alone does not prove direction/meaning, and 0283 is used S2C too. Semantics/profile need dispatch/capture review; no rows closed.
- 2026-11-04: Added PK45.03 accept-enter leaf: client supports 0073/11-byte legacy and 02EB/13-byte extended handlers; selected 20080910 server emits 02EB (font field), not 0073. Both share time/position then switch world; production replay/load lifecycle remains open; no row closed.
- 2026-11-04: Added PK45.03 019B leaf: server 10-byte actor/effect-code packet broadcast AREA; client maps five level-up codes and suppresses local duplicates, while other documented refine/game-over/pharmacy codes fall through. Verify alternate UI paths/profile and replay; no row closed.
- 2026-11-04: Added 01F1 evidence: client variable packet is handled in login mode, resets handshake, extracts text@4, shows dialog and disconnects; inventory labels it map. No selected rAthena login/map emitter found. Stage/profile ownership and framing need stronger review; no row closed.
- 2026-11-04: PK45.03 walking decoder follow-up: client aliases 022C/07F7/0856 to skeleton handler; selected server only has packetdb entries (022C 64/65 branches, others variable), no S2C clif emitter. Keep direction/profile unresolved pending captures; no row closed.
- 2026-11-04: Added 0B0D profile note: server remove-effect packet is 10 bytes and gated at 2018-10-02, beyond selected 2008; client inventory has no handler. Later-profile context only; no row closed.
- 2026-11-04: PK45.03 coverage check found five assigned rows (0094/00F7/012A/0140/0234) are C2S parsers despite S2C work-item scope; added ownership-review leaf and registered new evidence files in state.json. Do not reassign without source/client-send audit.
- 2026-11-04: Completed missing PK45 handler reference 00F8 in storage-lifecycle leaf: selected server emits two-byte ZC_CLOSE_STORE to SELF; client CloseStorage clears session/UI. Reopen/list ordering still needs replay; no row closed.
- 2026-11-04: Added PK45.03 0117 ground-skill leaf: 18-byte source/skill/level/cell/tick, AREA or disguise AREA_WOS+SELF; client actor/world messages and local skill visuals traced. Replay remains open; no row closed.
- 2026-11-04: Added PK45.03 skill-result leaf for 0114/01DE active damage layouts, 0115 commented-out legacy position emitter, and 011A no-damage result. Server/client fields and disguise routing traced; effective branch/replay remain open; no row closed.
- 2026-11-04: Added explicit inventory handler-name references to storage, companion skills, NPC shop results, party invite ACK, player skill update, skill-unit set, and storage password evidence leaves; remaining uncovered names include map change, effect2, and use-item ACKs.
- 2026-11-04: Added PK45.03 direct effect 01F3 leaf: server actor/effect fields and caller route, disguised SELF copy; client resolves actor and launches effect without status mutation. Profile/resources/replay remain open; no row closed.
- 2026-11-04: Added PK45.03 use-item ACK leaf for 00A8/01C8: 7-/13-byte client layouts, server index/amount/result and extended item/actor fields, SELF failure/AREA success; client inventory consume/pending/card/potion effects. Compile branch and replay remain open; no row closed.

PK45.03 map-change receive (2026-11-04): traced client 0091/0092 layouts, profile registration, map/session updates, same-map reuse, and reconnect failure path. Server emitter and profile parity could not be verified because the pinned server snapshot is absent from this workspace; stronger source-side review needed. No rows closed.

PK45.03 map handler index crosswalk (2026-11-04): all six apparent map S2C name gaps traced to existing PK45.02 leaves (four handlers) or PK45.03 whisper-flow evidence (two handlers); added explicit crosswalk. This reconciles names only and does not close semantic acceptance criteria.

PK45.03 ownership audit correction: coverage cross-check found 0x00A7 was the one PK45-owned opcode absent from packet evidence. Inventory identifies it as C2S clif_parse_WalkToXY; the client table fixes 8 bytes and send-profile constant names kWalkToXY. Added it to the six-row C2S ownership-scope review. All 52 PK45-owned opcode strings are now referenced by PK45 evidence; this is a reference audit only, not a contract-completeness assertion. Rows remain unreviewed.

PK45 handler-index count reconciliation: baseline states 136 indexed map functions; packet inventory has 135 distinct map-stage client_handler names; current RegisterDefaultGameModePacketHandlers lexical references include 140 unique function names across 200 refs. All names are referenced in PK45 evidence, but the populations differ and guard/profile normalization is unresolved. Do not claim acceptance met; stronger index-method reconciliation required.

PK45.03 map-change server trace completed (2026-11-04): verified the PK01.02 clif.cpp SHA-256; traced 20080910 PACKET_ZC_NPCACK_MAPMOVE 0x0091 (22B, SELF) and fixed pre-20170315 PACKET_ZC_NPCACK_SERVERMOVE 0x0092 (28B, SELF), including chrif transfer-ack caller and same-server callers. Corrected prior note that pinned server source was unavailable. Replay of transfer/reconnect and UI lifecycle remains open; no rows closed.

PK45 ownership correction: moved AUDIT-0x00A7 primary ownership from PK45 to PK46 after source confirmation of the 20080910 clif_parse_WalkToXY registration and client map-movement request in PK46.02-map-movement.md. Classification remains UNREVIEWED; this is an owner correction, not parity closure. Five C2S owner-scope rows remain under PK45 for review.

PK45.03 broadcast chat follow-up: traced the selected 0x009A emitter branches in pinned clif_broadcast: ordinary text, 'blue' BC_BLUE, and 'ssss' BC_WOE. The selected emitter does not produce client parser prefixes 'micc'/'tool'; those remain compatibility branches with unresolved reachability. No rows closed.

PK45.03 C2S review advanced: pinned final registry now records exact selected parser, length, and registration line for remaining five C2S rows (0094, 00F7, 012A, 0140, 0234). 00A7 transferred to PK46 after full movement request trace. The remaining five need profile-specific client sender and feature-owner crosswalk before owner change; all remain UNREVIEWED.

PK45/PK46 ownership correction: moved AUDIT-0x0094 and AUDIT-0x00F7 to PK46 after matching the active client 14B/22B storage send structs to selected server parser registrations and offsets. Added PK46.02-storage-transfer-requests.md; PK45 is retained as secondary feature dependency. Rows remain UNREVIEWED pending replies/deltas and replay. Three PK45-owned C2S rows remain.

PK45 C2S ownership review: remaining three are now explicitly triaged. 0x012A collides between selected server RemoveOption and PacketVer22/23 client CartOff; no sender symbol found for 0x0140 MapMove or 0x0234 HomMoveToMaster in current client network/gamemode sources. Server registration alone does not prove current client emission, so leave owners and UNREVIEWED classifications unchanged pending profile/source-date adjudication.

PK45.03 warp map list traced against pinned server: 20080910 selects 0x011C fixed 68B four-slot structure with 16B extended map names, SELF routing, and a sole selected caller supplying at most two destinations. Documented that generic helper's five-entry case would index beyond the fixed array, but no current caller reaches it. Replay/UI lifecycle remains open; no rows closed.

PK45.03 NPC menu contract traced: 0x00B7 variable menu prompt (NPC ID + NUL-terminated option string, SELF); script builtin counts nonempty colon-delimited choices and warns at 255+; 0x00B8 7B response carries NPC ID@2 and selection@6, with 0 invalid and 0xFF cancel. Server/client ordinal semantics align; >254 overflow and repeated-menu UI need replay. No rows closed.

PK45.03 self-status signedness review refined: pinned clif.cpp confirms 0x00B0 SP_MANNER emission as signed int32 via SELF. Client reads u32 but has no explicit Manner storage case, so a large-value defect is not established. Negative-value reachability depends on setter code outside pinned hashes; retain for stronger source review. No rows closed.

PK45.03 NPC string input source trace completed: corrected stale claim that server source was unavailable; pinned 0x01D4 six-byte SELF prompt and 0x01D5 variable C2S header/parser are documented, including empty-input rejection and CHATBOX_SIZE copy bound. Byte encoding, non-ASCII UI counting and boundary replay remain open. No rows closed.

PK45.03 NPC number-input trace completed: verified 0x0142 SELF prompt, selected 0x0143 10B reply struct, int32 value stored directly to npc_amount, and client ten-character/strtoul-to-u32 path. Values above INT32_MAX are a real boundary if admitted by UI; allowed domain remains for stronger review/replay. No rows closed.

PK45.03 actor position correction traced: pinned 20080910 source confirms 0x01FF high-jump fields and AREA routing, disguised SELF copy, then clif_blown calls 0x0088 authoritative stop correction. Documented distinct walkability/sitting semantics and source order. Local interpolation/replay remains open; no rows closed.

PK45.03 resurrection follow-up: recorded both current pc.cpp callers (respawn warp failure and Super Novice automatic revival), their sequencing and the file SHA-256. pc.cpp is not in PK01.02 source-baseline hashes, so these are clearly labeled unversioned observations and require stronger source/config review; no rows closed.

PK45.03 dialogue flow traced: selected 20080910 0x00B4 variable say (NPC ID/message, SELF), 0x00B5 wait and 0x00B6 close (fixed SELF), and C2S 0x00B9/0x0146 next/close parsers verified. npc_scriptcont NPC-ID and state transitions documented as unversioned observations with npc.cpp hash; pin review and UI/replay timing remain open. No rows closed.

PK45.03 action-failure caller audit: current server tree has five clif_arrow_fail calls (four no-ammo, one ammo-equip success); recorded file hashes and marked unversioned because caller files are absent from PK01.02 hash set. Pinned emitter contract remains strong; callsite reachability/repeated notifications still need source pinning and replay. No rows closed.

PK45.03 movement profile gap resolved: PK01.02-pinned packet_structs select unit_walkingType 0x02EC for 20080910 and clif_set_unit_walking emits that 67B packet. The 0x0086/16B client handler is a separate compatibility branch; it is not the selected server movement packet. Documented 12B handler guard vs 16B frame mismatch separately; no rows closed.

PK45.03 ground-item disappearance: added bounded supplemental caller trace for current map.cpp: floor-item lifetime expiry and explicit clear both send 0x00A1 before removing/freeing the object. Caller source is unversioned (SHA-256 recorded); pickup reason and runtime synchronization remain open. No rows closed.

PK45.03 AUDIT-0x007B: corrected provisional direction to S2C from the versioned server movement branch. The pinned 20080910 profile uses 0x02EC; historical 0x007B layout remains uncataloged, so row remains UNREVIEWED.
PK45.03 AUDIT-0x0073: verified S2C ownership from client receive registration and packetver23 map-send profile; pinned 20080910 server selects 0x02EB. Historical 0x0073 server layout remains unsourced; row remains UNREVIEWED.
PK45.03 AUDIT-0x007F: replaced generic assignment basis with source-backed u32 tick and SELF-route evidence from PK45.03-server-time-notification.md. Direction remains S2C; canonical link and timing semantics remain open.
PK45.03 0x007F: traced the request parser to an immediate SELF tick reply and documented client 12-second sync cadence / 3-second pending retry. timer.cpp is unpinned; default platform paths are millisecond ticks while ENABLE_RDTSC is configured separately. Replay remains open.
PK45 AUDIT-0x0080: replaced provisional routing basis with PK45.02 actor-vanish packet/emitter evidence; replay and canonical-contract registration remain open. No row closed.
PK45 AUDIT-0x0086: ownership basis now distinguishes the traced 16-byte S2C client compatibility handler from the pinned 20080910 server 0x02EC emitter. Historical server mapping remains open; row stays UNREVIEWED.
PK45 AUDIT-0x0087: pinned 20080910 PACKET_ZC_NOTIFY_PLAYERMOVE and clif_walkok confirm packed 12-byte S2C local movement notification to SELF; separate from 0x02EC area updates. Replay remains open; no row closed.
PK45 AUDIT-0x0088: replaced provisional owner basis with PK45.03-actor-position-correction evidence for 10-byte S2C stop-move correction and AREA routing. Paired correction replay remains open.
PK45 AUDIT-0x008A: traced legacy 29-byte ZC_NOTIFY_ACT below PACKETVER 20071113; pinned 20080910 uses 0x02E1. Corrected direction to S2C; legacy replay/canonical linkage remain open.
PK45 AUDIT-0x009C: replaced provisional ownership basis with PK45.03-actor-direction evidence for the 9-byte S2C fields and pinned 20080910 emitter. Replay and canonical linkage remain open.
PK45 AUDIT-0x00B3: refined evidence to the 3-byte S2C return-to-character-select result and added current unpinned chrif.cpp forwarding observation with hash. Result origin and disconnect lifecycle replay remain open.
PK45 AUDIT-0x00C0: traced selected 0x00BF validation, flood/action checks, and 7-byte S2C emotion broadcast to AREA. Client rendering and pending-reply lifecycle remain open.
PK45 AUDIT-0x00E5: confirmed S2C direction from the client invitation parser and pinned SELF emitter; coverage now cites the 26-byte inviter-name contract. Trade lifecycle semantics/replay remain open.
PK45 AUDIT-0x00F8: replaced generic basis with PK45.03-storage-lifecycle evidence for the 2-byte SELF close packet and client storage-window closure. Empty/reopen ordering remains open.
PK45 AUDIT-0x0119: traced the server variant branch: 13-byte S2C legacy form below PACKETVER 7; pinned 20080910 selects 15-byte 0x0229. Updated direction/basis; state/cart lifecycle replay remains open.
PK45 AUDIT-0x0122: confirmed S2C legacy cart equipment-list variant; pinned 20080910 selects 0x02D2. Coverage cites PK45.03-commerce-cart-item-lists; historical schema remains provisional.
PK45 AUDIT-0x0123: confirmed S2C legacy normal-cart list; pinned 20080910 selects 0x02E9. Coverage cites PK45.03-commerce-cart-item-lists; historical schema remains provisional.
PK45 AUDIT-0x0124: confirmed legacy 21-byte S2C cart add-item form; selected 20080910 uses typed 0x01C5. Corrected stale cart-delta evidence status; historical variant and transaction replay remain open.
PK45 AUDIT-0x0138: identified three-byte client vending-open result and S2C direction; pinned server packet definitions inspected show 0x012D/0x0A28 instead. Added explicit historical identity/source-profile review blocker.
PK45 AUDIT-0x0139: replaced provisional basis with PK45.03-attack-distance-hint pinned 16-byte emitter/field evidence. Trigger unit.cpp caller is unpinned; chase/retry behavior and replay remain open.
PK45 AUDIT-0x013A: replaced generic basis with pinned 4-byte S2C attack-range emitter and source field evidence. Corrected stale open questions; zero/extreme-value behavior and chase replay remain open.
PK45 AUDIT-0x013B: replaced generic coverage basis with pinned 4-byte action-failure emitter/result semantics and client retry-state trace. Call-site reachability and replay remain open.
PK45 AUDIT-0x013D: replaced generic basis with pinned 6-byte S2C recovery contract. Corrected stale status; AP caller source is unpinned and max/zero/replay behavior remains open.
PK45 AUDIT-0x0145: recorded legacy client S2C 16-byte image path with no size-table entry; pinned server uses 0x01B3 instead. Marked historical framing/profile support unresolved for stronger review.
PK45 AUDIT-0x0148: replaced provisional basis with pinned 8-byte S2C resurrection emitter, ignored type field, and AREA route from PK45.03-actor-resurrection.md. Broader lifecycle replay remains open.
PK45 AUDIT-0x0191: replaced generic basis with PK45.03-talkbox-chat-contents 86-byte S2C packet and pinned emitter evidence. Text encoding/termination and actor-message replay remain open.
PK45 AUDIT-0x0194: added selected 30-byte S2C name-by-GID response crosswalk; pinned clif_solved_charname sends GID/name to SELF. Clarified actorNameAckParty is only a client alias; request correlation/cache lifecycle remain open.
PK45 AUDIT-0x0195: replaced provisional basis with PK45.02-actor-name-ack selected 102-byte full name response and pinned emitter evidence. Cache lifecycle and rendering replay remain open.
PK45 AUDIT-0x019B: replaced generic basis with PK45.03-notify-effect-019b selected 10-byte emitter, AREA route, and client effect mapping. Unsupported codes and replay remain open.
PK45 AUDIT-0x01B3: replaced generic basis with selected 67-byte S2C cutin contract and SELF route; corrected stale status. Asset encoding, type-255 behavior and rendering replay remain open.
PK45 AUDIT-0x01CF: added pinned 28-byte server layout and SELF/AREA routing. Client maps AID pairs into vector coordinates for effect 232/message 14; semantic mapping flagged for stronger review and replay.
PK45 AUDIT-0x01D1: added coverage basis citing fixed framing, AREA route, and local handler branches; lookup semantics, animation meaning, lifecycle cleanup, and replay remain open for review.
PK45 AUDIT-0x01D2: added coverage basis citing fixed 10-byte AID/tick framing, session cache update, live-actor application, and skill-action-272 consumption. Tick epoch, cache lifecycle and replay remain open.
PK45 AUDIT-0x01D3: added coverage basis citing fixed 35-byte frame, bounded sound name, one-shot/loop/stop branches and loop state handling. Server action contract, duration units, name encoding and replay remain open.
PK45 AUDIT-0x01DA: grounded inbound S2C direction in the client receive profile and 60-byte base registration; selected 20080910 server uses 0x02EC. Historical profile identity, layout heuristic and representative capture remain open.
PK45 AUDIT-0x01F1: corrected stage to login/char handshake and direction to inbound S2C from LoginMode receive-profile dispatch and AC_NOTIFY_ERROR handshake reset. Selected-server identity and variable-packet prefix/text contract remain unresolved.
PK45 AUDIT-0x01FF: replaced provisional basis with selected 10-byte ZC_HIGHJUMP layout, AREA route and clif_blown ordering before 0x0088. Movement interpolation and paired-event replay remain open.
PK45 AUDIT-0x0229: replaced provisional basis with selected 15-byte ZC_STATE_CHANGE variant at PACKETVER 7+ and client status/cart handling. Status/cart lifecycle and replay remain open.
PK45 AUDIT-0x022C: grounded inbound S2C direction in client profile/shared walking decoder; selected-server evidence is only packet-database registration with 64/65-byte branches, not a selected emitter. Alternate profile identity/capture remain open.
PK45 AUDIT-0x0234: replaced provisional basis with selected 6-byte clif_parse_HomMoveToMaster registration and recorded absence of a client sender in network/gamemode; leave PK45 ownership unresolved pending feature/profile crosswalk.
PK45 AUDIT-0x0297: grounded inbound S2C direction and cart-list family; identified 0x0297 as alternate profile while selected 20080910 uses 0x02E9/0x02D2. Server field schema and replay remain open.
PK45 AUDIT-0x02B9: replaced generic basis with selected 191-byte 2008 SAK contract and matching client 27-slot decode/replacement. UI mapping, persistence/update order and replay remain open.
PK45 AUDIT-0x02EB: replaced generic basis with selected 13-byte 20080910 ZC_ACCEPT_ENTER2 contract and client session/world-entry path. Map-load timing and production replay remain open.
PK45 AUDIT-0x02EC: replaced provisional basis with selected 67-byte 20080910 unit_walkingType/clif_set_unit_walking contract and client shared decoder mapping. Movement interpolation and replay remain open.
PK45 AUDIT-0x02EF: replaced provisional basis with selected 8-byte clif_font account-ID/font-ID fields and AREA route; client state assignment traced. Resource lookup and visible refresh parity remain open.
PK45 AUDIT-0x022C/0x07F7/0x0856: recorded client profile decoder mappings and selected-server absence of matching S2C emitters. Kept direction unresolved per PK45.03-actor-walking-decoder.md pending authoritative profile/capture reconciliation.
PK45 AUDIT-0x0B0D: replaced provisional basis with later-profile 10-byte ZC_REMOVE_EFFECT and 20181002 gate; selected 20080910 server omits it and current client lacks receive handler. Keep open as later-profile context pending profile decision.
PK45 AUDIT-0x012A/0x0140: replaced generic bases with selected parser and client-profile evidence. 0x012A conflicts with PacketVer22/23 kCartOff; 0x0140 has no identified client sender. Keep both unreviewed pending profile/date and feature-owner adjudication.
PK46.02: added 0x00E4 player trade request client path from player-context action through six-byte target-ID frame and selected registry parser identity. Server parser body, response lifecycle and replay remain open; primary owner remains PK50.
PK46.02: added 0x00E6 accept/decline action values and incoming-trade close/timeout branches with selected parser registration; server result order and replay remain open. Primary owner remains PK37.
PK46.02: added 0x00E8 item/currency offer guards, index/amount framing and optimistic UI state branches; selected parser identity recorded. Server index-0 currency meaning and result lifecycle/replay remain open; owner remains PK37.
PK46.02: added 0x00EB lock and 0x00EF final-confirmation request guards and pending-state behavior with selected parser registrations. Server acknowledgements, transaction result/rollback and replay remain open; primary owners remain PK37.
PK46.02: added 0x00ED outgoing trade cancel call sites and close/timeout/disappearance state behavior with selected parser identity; incoming decline distinction recorded. Server lifecycle and replay remain open; owner remains PK37.
PK46.02: added 0x00BF emotion command parsing, malformed-command boundary, three-byte frame and successful-send throttle; selected parser registration identified. Server ID semantics and replay remain open; primary owner remains PK47.
PK46.02: added 0x00F3 global chat call paths and profile-selected variable-length name/text frame. Selected parser identity recorded; deployment profile, server text constraints and replay remain open; primary owner remains PK47.
PK46.02: added 0x0096 whisper call path, profile mapping, target/message frame offsets and client target guard. Selected parser identity recorded; server validation, text encoding and replay remain open; primary owner remains PK47.
PK46.02: added 0x00CF reject-whisper request call path, 27-byte name field and 23-byte client guard; selected parser identity recorded. Server persistence/result semantics and replay remain open; owner remains PK16.
PK46.02: recorded selected parser registrations but no client sender for 0x00D0/0x00D3; retained both as unresolved outbound paths rather than infer behavior from names. PK16 profile/feature crosswalk requires stronger review.
PK46.02: added 0x007D profile opcode/two-byte load-end ack and map-loading retry state, including repeated failed retries after 900 ms and suppression after sync. Selected parser identity recorded; server transition and replay remain open; owner remains PK47.
PK46.02: added 0x008C modern actor-name request fields, hover/proactive callers, per-GID cooldown and PacketVer22 deferral; selected final registry matches 11-byte GID@7 layout. Legacy 0x008C is global chat; preserve profile distinction and open response lifecycle/replay. Owner remains PK50.
PK46.02: added 0x0113 ground-skill caller and 22-byte serializer fields; selected final registry agrees on skill/coordinate offsets. Range units, server validation/replies and replay remain open; owner remains PK23.
PK46.02: added 0x012E vending-close request guard, two-byte frame and success/failure UI state ordering; selected parser identity recorded. Server acknowledgement and pending-open races remain open; owner remains PK37.
PK46.02: added 0x0130 player-context vending-list request and target-ID frame matching selected parser registration. Action label, server gates/response sequence and replay remain open; owner remains PK37.
PK46.02 follow-up 0x0130: confirmed action case 8 is absent from the current PlayerContextAction enum/menu (which cover 0–7); request serialization exists but standard UI reachability is unproven.
PK46.02: added 0x0134 vending purchase field offsets and local 0x0135 result handling; selected variable-length parser entry recorded. Server acceptance/emitter parity and stale/duplicate result replay remain open; owner remains PK38.
PK46.02: added 0x014F guild-window request sequence, subtype 0–4, fixed six-byte framing and ignored send-result behavior; selected parser identity recorded. Reply mapping, no-guild behavior and replay remain open; owner remains PK50.
PK46.02: added 0x0151 guild-emblem request from Qt and GR2 resolvers; both send guild ID u32 with shared five-second per-guild throttle. Selected parser identity recorded; response/cache lifecycle remains open; owner remains PK50.
PK46.02: added 0x0155 guild-member-position and 0x0159 guild-leave UI request guards and field layouts; selected final registry entries match. Server authorization/results and replay remain open; primary owners remain PK12.
PK46.02: added 0x0161 guild position-metadata request and exact 44-byte fields plus permission/name/range guards; selected parser identity recorded. Server bit/tax semantics and response/replay remain open; owner remains PK12.
PK46.02: added 0x0168 guild invitation player-context guard and 14-byte target/inviter IDs matching selected parser registration. Membership/response lifecycle and replay remain open; owner remains PK12.
PK46.02: added 0x016B guild invitation reply fields and prompt reset ordering; selected parser registration matches 10-byte frame. Prompt origin/correlation and membership result remain open; owner remains PK12.
PK46.02: added 0x016E guild notice update gate and 186-byte UTF-8 subject/body frame matching selected final registry. Server persistence/response and UTF-8 boundary replay remain open; owner remains PK12.
PK46.02: added 0x0172 alliance prompt reply fields and send-success state reset matching selected parser registration. Prompt correlation/result semantics and replay remain open; owner remains PK12.
PK46.02: added 0x017A card-composition request branch, four-byte index frame and send-success state update matching selected parser registration. Response correlation and replay remain open; owner remains PK38.
PK46.02: added 0x01AF cart-selector index/type mapping and four-byte request fields matching selected parser registration; selector closes on send success only. Server cart-state result and replay remain open; owner remains PK37.
PK46.02: added 0x01B2 explicit vending-open serializer/guards and 0x0138 client result path; flagged inactive close branch that sends an empty 85-byte open-vending frame and clears draft without pending state for stronger review.
PK46.02: added 0x0202 friend-add name frame and 0x0208 friend-reply prompt fields; selected parser entries match. Duplicate/online behavior, prompt correlation and replay remain open; primary owner remains PK16.
PK46.02: added 0x0246 mail-window request and 0x0248 mail-send frame/guards; selected parser entries match. 0x0246 mutates local composer state before send result; mail delivery/credit/attachment semantics and replay remain open. Owners remain PK32.
PK46.02: added 0x02D6 player-equipment inspect request fields and menu caller; selected parser matches. The initial search incorrectly concluded 0x02D7 had no response handler; superseded by the PK45.03/PK46.02 viewed-equipment traces showing Commerce.Receive and QtCommerce processing. Rendered UI/replay remain open; owner remains PK38.
PK46.02: added 0x0438 PacketVer23 10-byte target-skill frame and common guards; recorded separate PacketVer22 0x0072/25-byte branch and multiple caller matrix as stronger-review work. Server semantics and replay remain open; owner remains PK23.

PK45.03 follow-up (2026-11-04): NPC string input now cites the client’s 0-for-string UI limit and std::string submission path; NPC menu evidence now cites reused-window replacement of NPC ID/options/ordinals. Non-ASCII/byte-boundary behavior and malformed or >254-choice server semantics remain open for stronger source/runtime review.

PK45.03 follow-up (2026-11-04): dialogue evidence now records client close cleanup (NPC ID/text/action state cleared) and re-prompt state reset; selected-server NPC script-lock release timing remains open.

PK45.03 follow-up (2026-11-04): resolved the 0x0086 short-frame concern from the production decoder: fixed-length frames are rejected before dispatch unless all profile-sized bytes are present. Runtime movement timing remains for PK45.04.

PK45.03 follow-up (2026-11-04): 0x0148 client handler now documents the exact 8-byte boundary, ignored trailing u16, existing-actor message 98 behavior, local-player flag clearing, and local operation order. Cross-packet/server timing remains open for replay/runtime review.

PK45.03 follow-up (2026-11-04): actor state-change note now traces basic/extended receive selection through actor message 43 and the actor's body/effect/health/PK state storage and cart/bird transition dispatch. Rendering, teardown, unknown-GID recovery, and alternate-profile behavior remain for replay/runtime review.

PK45.03 follow-up (2026-11-04): pickup acknowledgment evidence now maps the single pending ground-object AID, success clearing, nonzero-result retry retention, and timer retry while the ground item persists. Stale/duplicate ack ordering and selected-server emitter remain open.

PK45.03 follow-up (2026-11-04): cart-list note now traces integral row framing, category-only replacement, row rejection, header-only category clearing, and state commit in the client receiver. Selected-server empty-list emission/schema and replay behavior remain open.

PK45.03 follow-up (2026-11-04): cart delta note now records add quantity merge with incoming metadata replacement, remove/erase thresholds, minimum-length-only checks, and acceptance of trailing bytes. Malformed/overflow, duplicate/reordered delta, and list/delta ordering need replay validation.

PK45.03 follow-up (2026-11-04): cart-list evidence now records equipment and normal row offsets, notes the unconsumed tail in 26-byte equipment rows, and distinguishes commerce cart state from session inventory. Transfer-packet correlation/order remains open.

PK45.03 follow-up (2026-11-04): position-correction note now confirms 0x0088/0x01FF share the same 10-byte handler and documents authoritative same-cell stop, local move-ack reset, and spatial unregister/snap/re-register effects. Visual timing/repeated packet ordering remain runtime checks.

PK45.03 follow-up (2026-11-04): emotion note now records that the request tick has no reader/timeout, the pending flag clears only on the special failure or a local actor emotion ack, and a lost reply can leave that informational flag set. Intended timeout policy remains open.

PK45.03 implementation follow-up (2026-11-04): normal/equipment inventory snapshots now reject partial-row tails before clearing existing state; valid empty lists keep their prior clear semantics. Equipment replacement preserves ammo types 10/16/17 and refreshes appearance after parsing. Release open_midgard build passed; replay boundary scenarios remain unrun.

PK45.03 storage follow-up (2026-11-04): corrected the open/list lifecycle claim. Client 0x00F2 calls OpenStorage, which clears storage rows; list packets then update incrementally. Remaining question is selected-server status-before-list ordering for initial/split lists.

PK45.03 follow-up (2026-11-04): actor font note now records a source-wide usage audit: m_charfont has no renderer/resource consumer, so the receive handler only stores the ID and visible font parity remains an implementation gap for graphics review.

PK45.03 implementation follow-up (2026-11-04): shared skill-list receiver now rejects partial 37-byte row tails before clearing player/homunculus/mercenary skills; empty-list semantics remain. Release open_midgard build passed. Target ID/upgrade flag/UI refresh replay review remains open.

PK45.03 follow-up (2026-11-04): updated the player-skill note to reflect the shared pre-clear 37-byte alignment guard now covering player as well as homunculus/mercenary lists; selected-server framing/order and ID-boundary replay remain open.

PK45.03 implementation follow-up (2026-11-04): normal/equipment storage-list handlers now reject partial row tails before applying entries; valid empty lists remain accepted after the 0x00F2 reset. Release open_midgard build passed; list/add/remove ordering still needs replay.

PK45.03 implementation follow-up (2026-11-04): NPC shop buy/sell list handlers now reject partial 11-/10-byte rows before quest/UI mutation; empty lists remain accepted. Release build passed; transaction/UI replay gaps remain.

PK45.03 follow-up (2026-11-04): shortcut snapshot evidence now maps the 27 serialized rows to 3 pages of 9 visible slots, documents zero-ID clearing and type normalization, and traces the UI's page-to-absolute-slot mapping. Selected-profile capture and visible refresh timing remain open.

PK45.03 NPC string serialization: sender byte-length and packet bound are now source-traced in the existing evidence note; encoding/boundary replay remains open.

PK46.02 tracker reconciliation: indexed the 96 existing package evidence notes in state.json. This is evidence bookkeeping only; full 177-request comparison and row review remain open.

PK46.02 opcode-name census: recorded that all 177 unique selected-registry parser opcodes are explicitly mentioned in the 97 indexed package notes. The census does not establish per-request contract completeness; step remains IN_PROGRESS.

PK46.02 movement request: added source-backed byte formula and bounds for the modern 0x00A7 serializer; selected registry offset/length match. Legacy profile and runtime replay remain unresolved.

PK46.02 add-friend: replaced the unavailable-parser note with the hash-verified selected server parser branches and target-side 0x0207 behavior. Client 0x0208 reply/cache lifecycle and replay remain open; no row closed.

PK46.02 friend flow: 0x0208 server parser decisions and `friend_auto_add` behavior are now source-traced; request note cross-links the reply. Client result/list receive and prompt-origin paths remain open.

PK46.02 friend workflow: linked the source-traced 0x0202/0x0207/0x0208/0x0209 request, prompt, response and result paths. Replay and stale/duplicate prompt behavior remain open; no discovery row closed.

PK46.02 identify item: added source-backed 0x0179 server/client result path, including no-ack cancel/invalid branch and current skill.cpp hash. Runtime replay/UI acceptance remains open; no discovery row closed.

Current focus pointer: state.json active_step is now PK46.02 per the latest handoff. PK45.03 remains IN_PROGRESS and is not closed.

PK45.03/PK46.02 drop path: fixed the confirmed zero-count 0x00AF rejection case so it no longer erases the client inventory row; recorded the selected server branch and left extended-profile/replay review open.

PK46.02 equipment request: documented source-level ammo normalization and selected server EQP_AMMO override; acknowledgement/replay and other location values remain open.

PK46.02 unequip request: traced the selected 0x00AC success flag and receiver state changes. Recorded index-zero invalid-index ack as a stronger-review item; runtime replay remains open.

PK45.03/PK46.02 storage path: fixed zero-count 0x00F6 failure corrections so they no longer erase client storage rows; source call sites and packet fields are recorded. Success ordering/replay remains open.

PK46.02 warp-skill request: traced the cancel sentinel through server name normalization into the exact cancellation branch; hashes for newly inspected unversioned server files are recorded. Destination semantics/replay remain open.

PK46.02 warp-skill: recorded the selected server failure path for an unknown AL_WARP destination. Map-name case matching remains a stronger-review item; no discovery row closed.

PK46.02 inspect-equipment: corrected stale evidence; 0x02D7 is variable-framed and handled by Commerce.Receive/QtCommerce. Server permissions and 26-byte rows are traced; rendered UI/replay remain open.

PK45.03 viewed-equipment: corrected prior uncertainty; the selected 0x02D7 server snapshot layout, client Commerce receiver, and Qt data path are source-traced against PK01.02. Replay/UI acceptance remains open.


## 2026-11-04 — independent in-progress review and repair continuation plan

At HEAD `2a9ba7982bc6899ba62720989ef0799e77d321eb` with existing dirty runtime/replay/research work preserved, reviewed implementation since `af41f6484cb7ff31825296babe4b913aa36c4c67`. Recorded R1 terminal map failure, R2 full-roster marker loss and R3 marker lifetime gaps in `evidence/REVIEW-2026-11-04.md`. Fresh deploy-off MinGW app/replay build passed; focused CTest passed 1/1 in 3.12 seconds after local-loopback escalation. This review is not PK48 certification and makes no repair claim.

Allocated PK05.05, PK14.05 and PK14.06 with exact production-path regressions, allowed files, evidence and commit expectations; scheduled PK14.04 integration verification afterward, then resume previously active PK46.02. Active step is PK05.05 (specified, runtime unmodified). Reconciled PK14.03's stale machine status to its existing `3ff44a3e` implementation checkpoint. Preserved prior HANDOFF/STATUS verbatim in evidence history and replaced active summaries to eliminate competing continuation instructions. Preserved all existing progress entries and coverage classifications. Updated plan, package cards, contract boundary and blocker instructions; independent certification remains pending. `python tools/check_packet_plan.py` passed with 51 packages, 235 numbered steps and 990 discovery owners; `git diff --check` passed. These validate documentation only. Stable plan-update subject: `docs(packet): prioritize integration review repairs`. This documentation update is not a numbered implementation completion.

PK05.05 deferral (2026-11-04): recorded the explicit receive-outcome transition matrix and production caller/replay gap; left the repair open and continued as requested. PK14.05 (2026-11-04): HandlePartyList now validates and atomically replaces full rosters, preserving same-AID/same-map online coordinates and cached HP. The production replay passed the specified update, malformed input, identity, map/offline, removal and sentinel cases. Evidence is in PK14.05-roster-position-repair.md; the leaf is READY_TO_COMMIT because prior unrelated edits prevent safe staging of its handler hunk. Active step is PK14.06.

PK14.06 partial repair (2026-11-04): added session-owned party-position invalidation/revision and shared map/online eligibility; map changes, game-mode init/exit and login-mode exit invalidate without clearing membership. Both minimap paths use the shared policy, and visual revision tracks marker lifetime. Headless production OnExit replay passed; map-A/B, same-map warp, server-move failure, reconnect/character-change orchestration and rendered UI remain open in PK14.06-marker-lifetime-repair.md. PK14.04 verification records the current integration/UI limits; active work resumes at PK46.02.

PK46.02 sit/stand follow-up (2026-11-04): traced the selected 20080910 server's 0x02E1 ZC_NOTIFY_ACT sit/stand broadcast through the client 33-byte framing, production actor-action router, and CGameActor::Am_Action. Corrected the old-opcode ambiguity: 0x008A is not this server profile's wire ID. No dedicated request/response replay or visible posture capture; no row closed.

PK46.02 continuous attack follow-up (2026-11-05): traced selected 0x0437 action value 7 through clif_parse_ActionRequest_sub to unit_attack(..., true), including parser gates, target resolution, auto-chase and retry-with-active-timer behavior. Normal positive signed block IDs match the client u32 wire bits; high-bit IDs remain unproven. Legacy 0x0089 and response/UI replay remain open; no row closed.

PK46.02 actor-name follow-up (2026-11-04): corrected the stale claim that selected server source was unavailable. Traced GetCharNameRequest validation/range behavior, clif_name response variants, and client per-GID cache/timer updates. Exact packet-version response shape and production replay remain open; no row closed.

PK46.02 actor-name packet-version follow-up (2026-11-04): resolved the selected 20080910 response structs at source level: player 0x0195/102 bytes, NPC-like 0x0095/30 bytes, matching the client full/basic parse sizes. 0x0194 is registered by the client but is not emitted by this selected server branch. Fourth field is position_name server-side and guildTitle client-side; replay/cache validation remains open.

PK46.02 guild metadata/member-position follow-up (2026-11-04): traced both selected server parsers. The 0x0161 server consumes one 40-byte record from @4; packet-relative fields position@4, permission@8, exp-share@16 and name@20 agree with the client frame, while duplicate ID@12 is skipped. The 0x0155 parser gates on guild-master, treats position 0 as leadership transfer, and forwards positive member updates to inter-server service; error/update semantics remain partly delegated. No row closed.

PK46.02 guild member-position service continuation (2026-11-04): traced map inter-server command 0x303A through character-service identity lookup, persistence flag and broadcast, then the map update/broadcast callback. Character service checks guild and AID/CID membership but the inspected handler does not independently authorize guild master or clamp the position; stronger review should audit trust boundary and malformed frame paths. No replay or row closure.

PK46.02 guild metadata service continuation (2026-11-04): traced 0x303B position updates through character-service guild/index validation, state replacement, map broadcast and persistence marking. Source-level server update path is now covered; UI/request replay remains open.

PK46.02 vending purchase follow-up (2026-11-04): traced selected 0x0134 parser and transaction validation/effects. Server 0x0135 emit sites found cover explicit failures; successful purchase mutates both sides and notifies seller without a success result in this path. Silent rejection branches and production replay remain open; no row closed.

PK46.02 vending-list follow-up (2026-11-04): traced 0x0130 request gates, saved seller vending UID and 0x0133 list emission. Selected 20080910 response has 8-byte header/22-byte entries and matches Commerce::Receive offsets. List-request routine itself lacks a proximity check; UI reachability/privacy and replay remain open; no row closed.

PK46.02 guild-invite follow-up (2026-11-04): traced 0x0168 server permission/recipient gates and 0x016B pending-guild reply handling through asynchronous guild add-member. Server derives inviter from authenticated session, ignoring redundant inviter IDs in request; no replay or row closure.

PK46.02 close-vending follow-up (2026-11-04): traced opcode-only 0x012E through selected server handler. Active shop closure deletes shop rows, clears server vending state, broadcasts actor disappearance and removes the in-memory vending entry; no close ack is emitted. Client/server order and disconnect cases remain for replay; no row closed.

PK46.02 emotion follow-up (2026-11-04): traced 0x00BF server gates and 7-byte 0x00C0 area broadcast/client actor route. Server enum supports 0–87 but client accepts only 0–80. Integer-second server flood condition can reject at exactly 1000 ms and advances its timestamp on rejection, differing from client send throttle; timing/effect replay remains open.

PK46.02 card-composition follow-up (2026-11-04): traced 0x017A validation and 0x017B eligible equipment list, client selection handler, then 0x017C server-side composition checks and 0x017D result. Empty eligibility is silent and has no explicit client request-token/timeout cleanup; replay/UI acceptance remains open.

PK46.02 equip-item follow-up (2026-11-04): traced server acceptance gates and legacy 0x00AA ack values through client state updates. Silent early returns can leave pending client state; mismatched acks still apply wear-location updates while only matching index clears pending state. Replay and appearance timing remain open.

PK46.02 global-chat follow-up (2026-11-04): traced selected server name-prefix/NUL/length validation, at-command/mute/chat-delay gates, channel-vs-area route and sender echo. Explicit UTF-8 validation is absent in the inspected path; boundary/replay remains open.

PK46.02 guild-emblem follow-up (2026-11-04): traced 0x0151 server lookup and 0x0152 compressed bitmap response into client validation/cache. Missing guild/empty emblem are silent; malformed data does not clear the shared 5-second client throttle. Visible invalidation/replay remain open.

PK46.02 NPC-contact follow-up (2026-11-04): traced 0x0090 target dispatch and server NPC gates/proximity/subtype behavior. Client contact-type byte is ignored; no generic ack is emitted, with subtype-specific dialog/shop responses. Script/UI replay remains open.

PK46.02 guild-info follow-up (2026-11-04): mapped request subtypes 0–4 to selected server reply groups and client guild-window handlers. Parser reads a 32-bit subtype from byte+padding; malformed nonzero padding becomes unknown and receives no reply. Async ordering/replay remains open.

PK46.02 NPC-menu follow-up (2026-11-04): traced 0x00B8 choice validation and script continuation. Choice 0xFF is allowed; zero or above-current-menu choices kick, and target must match active NPC with range/visibility recheck. No generic ack; send-failure UI recovery and script replay remain open.

PK46.02 NPC-dialog-close follow-up (2026-11-04): traced 0x0146 into npc_scriptcont closing-state handling. Server validates active/matching NPC and proximity; CLOSE ends script, STOP resumes it, with no generic ack. Client pre-clears UI/state, so silent rejection/send failure ordering remains for replay.

PK46.02 NPC-next follow-up (2026-11-04): traced 0x00B9 through secure timeout, active NPC/range/progressbar gates and script state transitions. Invalid requests are silent; later script output is the response. Client pre-clears dialog action, with send-loss recovery still open.

PK46.02 NPC-number-input follow-up (2026-11-04): linked PK45.03 selected parser evidence to PK46.02 UI caller; server stores signed int32 directly and resumes script under shared NPC continuation gates. >INT32_MAX meaning remains a stronger-review item; cancel/send/replay remain open.

PK46.02 NPC-string-input follow-up (2026-11-04): reconciled the UI sender with PK45.03 selected parser trace. Server bounded-copies payload into CHATBOX_SIZE and resumes script; explicit encoding validation is absent. Malformed termination, multibyte byte-boundary and UI recovery remain replay/review gaps.

PK46.02 NPC-shop purchase follow-up (2026-11-04): traced 0x00C8 row parsing, nearby shop/item and inventory/zeny validation, normal vs script-shop transactions, and 0x00CA result handling. Parser floors incomplete trailing rows and has no explicit zero-quantity check in the inspected path; targeted replay/review remains open.

PK46.02 NPC-shop sell follow-up (2026-11-04): traced 0x00C9 validation, normal/script shop branches and 0x00CB result. Found source-level duplicate-index partial-deletion risk: per-row validation precedes sequential deletion, and a later deletion failure returns before zeny credit. Recorded for stronger review and replay; no code fix attempted without transaction-level validation.

PK46.02 NPC-shop deal-type follow-up (2026-11-04): traced 0x00C5 proximity/visibility/subtype checks and buy/sell list response dispatch. Server treats type 0 as buy and every nonzero as sell, whereas client accepts only 0/1; malformed-type and shop-switch replay remain open.

PK46.02 open-vending follow-up (2026-11-04): traced 0x01B2 flag-0 empty frame as server prep reset/cancel, valid-offer validation and 0x0136 success list. Selected 2008 build compiles out newer open-store ack, and some rejection paths are silent, potentially leaving client pending state; replay remains open.

PK46.02 guild chat/alliance follow-up (2026-11-04): traced server 0x017E validation/routing and 0x0170/0x0180/0x0183 guild-master, WoE, no-ask, capacity and character-service paths. No direct client constructor found in bounded search; full generic/indirect UI affordance audit is handed off for stronger review. Rows remain open.

PK46.02 player-trade follow-up (2026-11-04): traced 0x00E4 server target/session, action/skill, map, recipient, permission and distance gates. Success sets reciprocal pending IDs and prompts target; no requester success ack. Accept/reject/timeout/duplicate cleanup and replay remain open.

PK46.02 player-trade reply continuation (2026-11-04): traced 0x00E6 accept/reject against reciprocal pending state, server distance/blocker rechecks, trade activation/cancel cleanup and client 0x00E7 result-to-UI route. Disconnect/timeout/duplicate races remain open.

PK05.05 continuation review (2026-11-04): deferred the production map-update/terminal-receive integration replay for stronger review; existing framing and acknowledgement replays do not cover one-shot mode handoff or absence of further world updates. No implementation or acceptance claimed. Continued to PK14.05 as directed; selective commit boundary and read-only Git index remain open.
PK14.05/14.06 continuation review (2026-11-05): PK14.05 implementation/replay remains READY_TO_COMMIT, but session/replay hunks mix the roster change with marker-lifecycle changes and the repository index is read-only; no mixed hunk staged. PK14.06 source invalidation is present at map-change state application and game-mode init/exit, with shared native/Qt eligibility, but production map/server-move transition replay and rendered-frame evidence remain open. Neither repair is certified complete; resumed PK46.02 under the user's instruction to continue past complex gates.

PK46.02 trade-item offer follow-up (2026-11-05): traced selected server 0x00E8 parser narrowing, trade/lock/partner gates, item/rental/bound checks, duplicate-slot accumulation, recipient stack/space/weight validation, silent and explicit rejection paths, source 0x00EA ack, and peer add-item notification. Source pins added to the evidence note; runtime replay remains open.

PK46.02 trade-currency offer continuation (2026-11-05): traced index-zero dispatch to `trade_tradeaddzeny`; negative, sender-balance and recipient-MAX_ZENY failures are silent, successful offers replace trade zeny and notify the peer without a source add-item ack or actual-balance debit. Updated the trade evidence note; runtime rejection/reconciliation replay remains open.

PK46.02 trade-offer commit revalidation (2026-11-05): traced both-side lock gating and `trade_check` aggregate inventory simulation at commit; stack over-capacity, insufficient items or missing slots cancel the transaction. Per-add validation is not the only commit gate. Lock-race/client rollback replay remains open.

PK46.02 trade lock continuation (2026-11-05): traced 0x00EB server guards, source 0x00EA index-zero ack, per-side 0x00EC lock notifications and 0x00EF two-party commit gate plus aggregate validation. Duplicate/out-of-order and lock/item-ack races remain open for production replay.

PK46.02 trade-cancel continuation (2026-11-05): traced 0x00ED pending/active cancel to selected server cleanup and 0x00EE notices. Pending invites clear both peer IDs; active offers return staged item/zeny display state, clear locks/trade state, and notify connected peers. Cancel/commit and teardown interleavings remain open for replay.

PK46.02 trade commit continuation (2026-11-05): traced 0x00EF two-party state-2 gate, final simulated-inventory validation, item/zeny transfer, reset, and per-side 0x00F0 completion. The post-validation pc_additem failure branch merits stronger review; double-commit/disconnect/rollback replay remains open.

PK46.02 change-cart follow-up (2026-11-05): traced selected fixed 0x01AF parser, change-cart skill and renewal busy gates, level allowlist, independent PushCart/type checks, and success cart/status notifications. Effective RENEWAL/NEW_CARTS build flags and replay are still required.

PK46.02 use-item continuation (2026-11-05): confirmed PacketVer22 0x009F and PacketVer23 0x0439 lengths/indices against the selected registry; explicit Legacy 0x00A2 instead maps to a 15-byte SolveCharName packet. Traced parser dead/busy/range gates and pc_useitem restrictions/ack split. Script-specific use and early-return recovery replay remain open.

PK46.02 continuous-attack summary correction (2026-11-05): aligned the evidence front matter with its selected-server 0x0437/unit_attack follow-up; packet semantics are source-traced, but runtime/replay effects remain open.

PK46.02 reject-whisper continuation (2026-11-05): traced 0x00CF add/remove rules, byte truncation, duplicate/full outcomes and 0x00D1 result, plus map-session ignore filtering on incoming whispers. Persistence and multibyte/retry behavior remain open for replay.

PK46.02 whisper continuation (2026-11-05): traced selected 0x0096 map parser, NPC/channel branches, exact-name local delivery, offline fallback, local ignore/autotrade gates, and remote map-side ignore filtering/reply. Char-service result ordering and runtime text/encoding replay remain open.

PK46.02 status-increase follow-up (2026-11-05): traced the six-byte 0x00BC client acknowledgement path; failures leave cached stats unchanged, successes apply the stat update, and 0x00BE refreshes point-cost UI. Selected-server eligibility/refusal ordering and repeated-click replay remain open.

PK46.02 skill-up follow-up (2026-11-05): traced variable-length 0x010F client skill-list replacement (37-byte entries, player-skill filtering, session-cache rebuild). No evidence yet ties this receive packet to 0x0112; selected-server response ordering and Qt queue replay remain open.

PK46.02 shortcut-update follow-up (2026-11-05): traced fixed 191-byte 0x02B9 receive path and full replacement of 27 cached shortcut slots from seven-byte entries. Server update timing, persistence/rejection, and send-failure rollback remain unverified.

PK46.02 request-memo follow-up (2026-11-05): exact word-boundary UI/game-mode/network search finds only the friend/group “Memo” column heading, no outbound sender. Retained as inconclusive/UNREVIEWED; feature applicability and selected-server prerequisites need owner review.

PK46.02 actor-name follow-up (2026-11-05): reconciled stale “Still open” text; selected 20080910 server response IDs/lengths and client cache/timer handling are already source-traced. Profile-specific replay and rendered cache acceptance remain open.

PK46.02 party-option follow-up (2026-11-05): traced fixed six-byte 0x0101 client response; it updates EXP sharing or displays refusal, while optional item-option parsing requires eight bytes and cannot run under registered framing. Selected-server response and item-option reconciliation remain open.

PK46.02 equip-item follow-up (2026-11-05): traced seven-byte 0x00AA handling, matching-index pending clear, failure no-op, and successful wear-mask/appearance update. Unmatched successful acknowledgements still mutate equipment; stale/unsolicited ordering and visible replay remain open.

PK46.02 NPC-next follow-up (2026-11-05): traced fixed 0x00B5 client response restoring the dialog Next action and variable 0x00B4 text append/conditional-clear behavior. These are subsequent script outputs, not generic acknowledgements; silent rejection/send-failure recovery needs replay.

PK46.02 NPC-menu follow-up (2026-11-05): traced variable 0x00B7 client parsing into menu labels/server choice bytes; selection submits the mapped choice, not necessarily visible row+1. Script-output semantics are not an acknowledgement; runtime choice parsing and send-failure recovery remain open.

PK46.02 NPC-close follow-up (2026-11-05): traced fixed 0x00B6 client output handling: hide menu/input, show a Close action when dialog remains, otherwise release conversation lock and close dialog windows. Send/rejection ordering remains open for replay.

PK46.02 NPC-contact handoff (2026-11-05): marked 0x0090 as a subtype-dispatch root; full NPC/shop/script response-to-window matrix and click-modifier replay need stronger crosswalk review. No uniform acknowledgement or acceptance claim made.

PK46.02 party-leave follow-up (2026-11-05): traced fixed 31-byte 0x0105 client notifications, refusal messages, member removal, self/disband cache clearing and party UI refresh. Back-to-back disband request ordering and non-master leave reachability remain stronger-review/replay gates.

PK46.02 party-member-removal follow-up (2026-11-05): linked 0x0103 disband sends to the client 0x0105 removal notification/cache behavior documented in the leave-request leaf; no request token proves per-member correlation. Server authorization and multi-send ordering remain stronger-review/replay gates.

PK46.02 party-invite follow-up (2026-11-05): traced basic/extended client invite-result layouts and result-to-chat messages. Acceptance notice does not itself mutate party membership; separate member/list updates do. Server authorization and full result/update ordering remain open.

PK46.02 party-invite-reply follow-up (2026-11-05): traced incoming basic/extended invite prompt fields, party-ID validation, stored name/ID, and prompt clear-before-send behavior. UI sets no expiry here; server validity, stale/duplicate prompts and membership result timing remain open.

PK46.02 party-create follow-up (2026-11-05): traced 0x00FA result messages, conditional local master-party cache seeding, and saved-option follow-up send. The follow-up send result is ignored before pending values clear; server acceptance/order and failure reconciliation remain open.

PK46.02 party-chat follow-up (2026-11-05): confirmed the Group tab is a party/broadcast/battlefield display filter; chat submission passes text and whisper target, not a channel selector. This does not prove 0x0108 unsupported; PK17 ownership stays UNREVIEWED.

PK46.02 storage-transfer follow-up (2026-11-05): traced client storage-add variants, positive removal amounts and nonpositive 0x00F6 correction handling. Cache effects are source-backed; per-direction inventory/storage notification ordering and atomicity remain replay gates.

PK46.02 storage-close follow-up (2026-11-05): traced fixed 0x00F8 client close notification; it closes session storage and storage windows, while request send success alone leaves them open. Server trigger/order and transfer interleaving need replay.

PK46.02 ground-pickup follow-up (2026-11-05): traced 23-byte basic/29-byte extended ack handling. Only success adds inventory and clears pickup queue/tick; failures retain retry state, and animation associates success with queue-front ground ID without a response token. Profile/replay races remain open.

PK46.02 private-message ignore follow-up (2026-11-05): distinguished current per-name Reject Whispering sender 0x00CF from registered ignore-all 0x00D0 and ignore-list 0x00D3 requests. The latter remain UNREVIEWED under PK16; no sender inferred.

PK46.02 emotion follow-up (2026-11-05): traced 0x00C0 actor lookup, pending-flag clear on local echo, emotion-ID-to-animation mapping and ID-34 silence effect. IDs 81-87 remain ignored by client; asset rendering and throttle-boundary replay remain open.

PK46.02 use-item follow-up (2026-11-05): traced basic 0x00A8 and extended 0x01C8 acknowledgement handling, inventory-count reconciliation and pending-state differences. Extended acks clear local pending state only for local actor IDs; profile, stale-ack and early-return recovery remain open.

PK46.02 return-to-character-select follow-up (2026-11-05): traced 0x00B3 success, 0x018B refusal handling and the 15-second drained-receive timeout fallback to LoginMode. Live character-server/list restoration and response-branch replay remain open.

PK46.02 drop-item follow-up (2026-11-05): reconciled stale front matter and traced removal-ack correlation: matching index clears pending wait, basic zero-count 0x00AF preserves inventory, but any other positive removal mutates inventory without matching the pending index. Stale/duplicate replay remains open.

PK46.02 vending-close follow-up (2026-11-05): traced fixed 0x0132 seller-disappearance notification; it removes the board and clears local vending active/pending state for the local seller. It is not a direct 0x012E ack; lost/reordered broadcast and SQL-failure replay remain open.

PK46.02 guild-invite follow-up (2026-11-05): traced fixed 0x0169 client result-to-system-message mapping. It does not mutate guild membership; accepted membership is asynchronous via guild updates. Reachability and accept/reject/update ordering remain replay gates.

PK46.02 plan review (2026-11-05): reconciled the 2026-11-04 review sequence with the later user-directed continuation. PK46.02 remains active ahead of PK05.05 and PK14.05/.06/.04; removed the superseded 11/04 HANDOFF instruction while retaining R1-R3 findings and their acceptance gates. No packet rows or repair states were closed.

PK46.02 mail-composer follow-up (2026-11-05): found an unresolved client/server lifecycle ambiguity for 0x0246. Client zero-initializes the four-byte request (type 0); selected PACKETVER=20080910 handler type 0 invokes draft-item and zeny removal helpers. Updated the leaf with source paths and a controlled reproduction gate; intent/compatibility needs stronger cross-layer review. No row closed.

PK46.02 mail-send follow-up (2026-11-05): traced 0x0248 through the selected legacy parser, mail throttle/quota/attachment validation, character-service handoff, immediate refund failure path and 0x0249 client result handling. Delivery durability/order and replay remain open; paired 0x0246 draft-state ambiguity remains referred for stronger review. No row closed.

PK46.02 NPC-number follow-up (2026-11-05): pinned the boundary for stronger review: 10-char/type-1 edit control, unchecked `strtoul` to client u32, signed server int32 assignment and direct script continuation. Numeric domain, edit filtering, overflow and send-failure replay remain open; no row closed.

PK46.02 vending-open deferral (2026-11-05): marked the dual-form 0x01B2 request and 20080910 response/rejection lifecycle for stronger independent review and production replay. Local pending behavior under silent rejects/lost responses remains open; no row closed.

PK46.02 guild-invite-reply follow-up (2026-11-05): replaced a stale parser-unavailable note with the selected 0x016B map-server path, pending-invite correlation/replacement, inviter result cases and asynchronous add-member boundary. Stale/reordered replies and membership/result ordering remain replay gates; no row closed.

PK46.02 guild-alliance-reply follow-up (2026-11-05): traced 0x0172 through online-inviter/guild correlation, capacity/opposition checks and asynchronous guild-service updates. Exact inviter-account and rank authorization are not checked in the cited reply function; deferred for stronger review/replay without declaring a vulnerability or changing row status.

PK46.02 guild-leave follow-up (2026-11-05): replaced stale parser-unavailable text with the selected server validation gates and guild-service handoff. Server-side leader policy and asynchronous membership/result behavior remain open for replay; no row closed.

PK46.02 guild-notice follow-up (2026-11-05): traced server gmaster/guild-ID guards, legacy multilingual marker cleanup and asynchronous intif notice update/broadcast. UTF-8 boundary, failure and refresh-order replay remain open; no row closed.

PK46.02 vending-list UI follow-up (2026-11-05): confirmed Qt Browse shop dispatch requires a cached local shop-board entry; corrected the stale gate that still requested response-handler tracing already present below. Board lifetime/privacy and production replay remain open; no row closed.

PK46.02 trade-ack follow-up (2026-11-05): traced selected 0x00E6 server action handling, reciprocal partner/range/busy checks, both-side cancel or trade initialization, and client result mapping. Timeout/duplicate/stale request ordering remains replay-open; no row closed.

PK46.02 load-end ACK follow-up (2026-11-05): replaced stale parser-unavailable text with the selected server entry gates, rewarp path and normal bootstrap landmarks. Full 437-line lifecycle ordering and retry/synchronization replay are deferred for stronger review; no row closed.

PK46.02 pickup-profile deferral (2026-11-05): made explicit that client Legacy 0x009F conflicts with the selected 20080910 use-item registration, while PacketVer22/23 0x00F5 matches take-item framing. Actual selected executable/profile and compatibility remain for stronger review; no opcode classification changed.

PK46.02 whisper follow-up (2026-11-05): traced char-service target lookup/canonicalization, multi-map 0x3801 fanout, ignore-result aggregation and TTL record expiry. Source-map timeout recovery, wire-text boundaries and runtime replay remain open; no row closed.

PK46.02 card-composition follow-up (2026-11-05): traced server composition validation and 0x017D ack branches plus the registered client handler/cache mutation. Invalid requests can be silent; local ack correlation and inventory-notification ordering remain replay gates. No row closed.

PK46.02 card-composition ordering follow-up (2026-11-05): confirmed pc_delitem type bit 1 suppresses a separate delete-item packet while still updating weight, making the 0x017D ACK central to local card/equipment cache mutation. Packet ordering remains a replay gate.

PK46.02 exit-to-Windows follow-up (2026-11-05): traced server logout-prevention checks, 0x018B result/EOF behavior, client acceptance/refusal handling and 15-second drained-receive fallback. Menu recovery and runtime disconnect/reconnect replay remain open; no row closed.

PK46.02 save-point follow-up (2026-11-05): traced 0x00B2 type-0 behavior through dead-state guard, battleground respawn, saved-location warp and warp-failure resurrection notification. Menu and production map-transition replay remain open; no row closed.

PK46.02 reject-whisper reconciliation (2026-11-05): aligned the leaf summary with its existing selected-server per-session ignore mutation and 0x00D1 result trace. Durability, encoding truncation and replay remain open; no row closed.

PK46.02 global-chat follow-up (2026-11-05): traced Qt QString-to-UTF-8 input cleaning, 180-byte/character cap and pass-through serialization; documented settings-driven base/gameplay profile defaults. Runtime deployment profile, native-input byte behavior and production text/reply replay remain open; no row closed.

PK46.02 ground-skill follow-up (2026-11-05): replaced stale parser-unavailable text with common server gates, homunculus/mercenary delegation and learned/skill-item level dispatch. Flagged u16 client coordinates passed to i16 server arguments and deferred skill-specific range/result review; no row closed.

PK46.02 cart-change reconciliation (2026-11-05): corrected the leaf summary to match its selected parser/build-specific cart-state trace below. Request/result correlation, rejection recovery and production replay remain open; no row closed.

PK46.02 vending-purchase follow-up (2026-11-05): confirmed packed request row order and fixed 0x0135 response offsets/result values against selected server headers; replaced stale “verify offsets” gate with precise replay/state-effect gates. No row closed; production replay remains open.

PK46.02 alternate vending opcode follow-up (2026-11-05): traced selected 0x012F registration and its distinct 84-byte title/offer offset path through the shared parser; bounded client source still shows framing registration but no sender. Profile/path reachability is deferred for stronger review; no row closed.

PK46.02 trade-request cross-reference (2026-11-05): linked the 0x00E4 invitation lifecycle summary to the dedicated 0x00E6 server/client result trace so its replay gaps remain explicit and are not mistaken for missing source research. No row closed.

PK46.02 cart-off request reconciliation (2026-11-05): traced 0x012A across all gameplay send profiles, selected registry and `clif_parse_RemoveOption`; recorded mount-first/cart-disable behavior and `NEW_CARTS` branch. Replaced stale profile-identity review gate with runtime UI/state and deployed-build gates. No row closed.

PK46.02 alternate movement request follow-up (2026-11-05): traced selected 0x0140 packed map/x/y layout to the server `@mapmove` atcommand parser and contrasted it with ordinary profile movement opcodes 0x0085/0x00A7. Client sender reachability and command permission remain for stronger review; no row closed.

PK46.02 time-sync follow-up (2026-11-05): traced selected 0x0089 parser tick offset, stored client tick and immediate ZC_NOTIFY_TIME response; retained the 0x007E legacy conflict and deployed-profile/retry timing gates for stronger review/replay. No row closed.

PK46.02 alternate party-create request follow-up (2026-11-05): traced selected 0x01E8 fields and common server guards, documenting its extra pickup/share settings versus 0x00F9; bounded current-client search found no sender. Reachability and ownership need stronger review; no row closed.

PK46.02 emotion request reconciliation (2026-11-05): compared selected 0x00BF player-emote and 0x01A9 pet-emote parser bodies; recorded their distinct enum/rate-limit versus pet-object/u32 behavior. Current-client pet-action reachability remains for stronger review; no row closed.

PK46.02 party-leave source follow-up (2026-11-05): traced selected 0x0100 parser through party_leave and character-service withdrawal; confirmed disband send order and that any successful expel send can mask a failed leave send in the aggregate result. Cross-service ordering/recovery stays for stronger review; no row closed.

PK46.02 guild-invite UI reachability follow-up (2026-11-05): traced the native player-context menu builder, entry selection and dispatch to the existing permission-guarded sender. Replaced the source-reachability gap with visible-runtime/replay gates; no row closed.

PK46.02 GM item/monster request follow-up (2026-11-05): traced selected 0x013F text input through item/zeny command dispatch and GM-gated monster lookup. Current-client sender absence remains bounded; supported-profile/administrative-path classification is deferred for stronger review. No row closed.

PK46.02 drop-item status reconciliation (2026-11-05): aligned the note status with its later HandleItemRemove trace, which shows unmatched positive removal notifications continue to inventory deletion. The remaining gate is stale/duplicate behavior under production replay; no row closed.

PK46.02 continuous-attack legacy profile reconciliation (2026-11-05): replaced a vague unproven status with the selected 20080910 registry conflict: legacy action 0x0089/7 bytes collides with server TickSend 0x0089/8 bytes. Profile repair and production replay remain open; no row closed.

PK46.02 targeted-skill parser follow-up (2026-11-05): traced selected 0x0438 offsets through clif_parse_UseSkillToId into shared skill gates and homunculus/mercenary dispatch. Per-skill result matrix and effective deployment profile remain for stronger review; no row closed.

PK46.02 guild-position wire reconciliation (2026-11-05): compared Qt�s 44-byte packet builder with the selected 40-byte record parser; confirmed bytes 12-15 carry a duplicate position ID that the server skips, with tax/name consumed at 16/20. Runtime boundary and service-order replay remain open; no row closed.

PK46.02 NPC sell duplicate-row finding (2026-11-05): source-traced independent prevalidation followed by sequential pc_delitem calls; duplicate indices whose combined amount exceeds the stack can partially delete items, then fail before zeny credit. Production-path regression and atomic fix design are deferred for stronger review; no row closed.

PK46.02 NPC purchase duplicate-stock finding (2026-11-05): selected source checks each row against original limited stock, charges aggregate zeny, then decrements stock/adds items sequentially. Duplicate rows exceeding stock in aggregate can fail after payment and partial mutation; production parser regression and atomic fix need stronger review. No row closed.

PK46.02 NPC shop deal-type validation follow-up (2026-11-05): confirmed the client rejects type >1, while selected server npc_buysellsel treats only zero as buy and routes every other value to clif_selllist after shop checks. Malformed-type replay remains open; no row closed.

PK46.02 NPC numeric-input conversion follow-up (2026-11-05): traced unchecked strtoul conversion through the u32 request field into signed int32 npc_amount; nonnumeric text becomes zero and high-bit values enter the signed domain. Script meaning and boundary/send-failure replay remain open; no row closed.

PK46.02 NPC-next send-failure trace (2026-11-05): confirmed both keyboard and click routes clear the next action before send; keyboard propagates send failure, click ignores it, and neither restores local action state. Visible recovery remains for replay; no row closed.

PK46.02 NPC menu cancel follow-up (2026-11-05): traced selected 0x00B8 0xFF sentinel through parser into script continuation; standard buildin_menu handles 0xFF as cancel, clears menu/input state and ends the script state. UI/send-failure replay remains open; no row closed.

PK46.02 NPC-contact request detail (2026-11-05): confirmed the client always sets contact type 1 while the selected server parser ignores that byte and dispatches by target kind/subtype. Full subtype response matrix remains a stronger-review/replay task; no row closed.

PK46.02 NPC string-input encoding follow-up (2026-11-05): traced WM_CHAR through CP_ACP conversion, byte-oriented UIEditCtrl storage/limit/backspace and raw strlen packet serialization. Deployed ACP/server script compatibility and boundary replay remain open; no row closed.

PK46.02 NPC-close failure trace (2026-11-05): confirmed RequestNpcCloseDialog clears quest/NPC tracking before zero-ID validation/send; all UI close paths tear down locally first, with keyboard returning send status while click/input dismissal ignore it. Restore/reconnect behavior remains for replay; no row closed.

PK46.02 party-removal authorization trace (2026-11-05): traced leader/member gate and AID+name target match through character-service expulsion request; noted no explicit self-target guard although the current client skips self. Character-service self-expel effects, name-byte compatibility and sequencing remain stronger-review/replay gates; no row closed.

PK46.02 party-create server continuation (2026-11-05): traced selected 0x00F9 party-lock/skill gates, name trim, membership/create-state rejection and asynchronous character-service success/name-conflict acknowledgements. The request carries zero option values; client post-create option application and replay remain open. No row closed.

PK46.02 party-create client continuation (2026-11-05): traced 0x00FA success/error handling, local leader seeding, deferred nonzero option update and pending-field clear. Recorded acknowledgement scheduling/send-failure and changed-local-party cases for stronger review or replay. No row closed.

PK46.02 party-create request checkpoint (2026-11-05): reconciled remaining evidence list against source. Confirmed name normalization is byte-counted to 23 bytes, packet zero-init supplies terminator, and direct memcpy performs no charset conversion. Deferred deployed encoding/boundary replay, ack scheduling/send-failure behavior and membership/options replay for stronger review. No discovery row closed.

PK46.02 party-option change server continuation (2026-11-05): traced selected 0x0102 leader/membership gate and found parser reads a u32 EXP value spanning the client u16 EXP and u16 item fields, then preserves existing item options. Documented the resulting source-backed field mismatch; all option combinations and downstream response behavior remain replay/strong-review gates. No row closed.

PK46.02 party-option response continuation (2026-11-05): traced selected pre-2009 0x0101 response as six bytes with u32 EXP only; the client item-state branch requires eight bytes and is unreachable for this registration. Character-service callback stores returned option integers. Recorded end-to-end combinations and cache/UI state for production replay and stronger review. No row closed.

PK46.02 party-leave caller continuation (2026-11-05): verified party-window disband visibility and the sole bounded GameMsg sender are master-gated; helper repeats that guard before remove requests and leave. Low-level leave sender itself checks only local party presence. Queued-removal order and rejection recovery remain stronger-review/replay gates. No row closed.

PK46.02 party-invite server continuation (2026-11-05): traced selected AID parser and party_invite gates for leader, map/instance policy, target availability/preferences, party capacity, permissions and pending-invite state. End-to-end invite prompt, acceptance race and membership update ordering remain replay gates. No row closed.

PK46.02 party-invite reply continuation (2026-11-05): traced selected parser flag 1 acceptance, forged/stale party-ID clearing, existing-party rejection, party_joining plus asynchronous add-member request, and rejection notification cleanup. Callback failure and membership race behavior remain replay/strong-review gates. No row closed.

PK46.02 party-invite reply race note (2026-11-05): confirmed party_reply_invite does not reject a null party lookup on accept before setting party_joining and issuing intif_party_addmember. Flagged stale party/disband race for stronger review and replay.

PK46.02 party-invite callback continuation (2026-11-05): traced character-service add-member dispatch, logged-off compensating leave, pending-state clear, failure notification to inviter, and success membership/option/info updates. Remaining gap is production prompt and race ordering replay. No row closed.

PK46.02 change-cart response continuation (2026-11-05): traced selected 0x01AF rejection paths and client 0x0121/0x012B state handlers. Selector closes on local send success; later packets are uncorrelated state updates, not request acknowledgements. Async rejection/UI recovery and notification ordering remain replay gates. No row closed.

PK46.02 inventory reconciliation (2026-11-05): updated C2S-007d evidence state from note-pointer-only to source trace recorded, lifecycle/replay still open; linked map-load-end-ack evidence already covers profile, request trigger/retry and selected parser gate. No row closed.

PK46.02 inventory reconciliation (2026-11-05): updated C2S-0090 to source trace recorded; subtype outcome and production replay remain open per NPC-contact evidence. No row closed.

PK46.02 inventory reconciliation (2026-11-05): updated C2S-0096 to source trace recorded; encoding boundaries and timeout/runtime replay remain open per whisper evidence. No row closed.

PK46.02 inventory reconciliation (2026-11-05): updated C2S-00a9 and C2S-00ab evidence states to reflect their source-traced client/parser/ack paths; stale ack, index-zero correlation and production replay remain open. No rows closed.

PK46.02 inventory reconciliation (2026-11-05): updated C2S-00b2 to reflect source traces for both menu branches through the linked character-select and save-point evidence; transition/UI replay remains open. No row closed.

PK46.02 inventory reconciliation (2026-11-05): updated C2S-00b8, 00b9, 00bb and 00bf to distinguish their linked source traces from remaining replay/recovery gates. No rows closed.

PK46.02 inventory reconciliation (2026-11-05): updated C2S-00c5/00c8/00c9 to reflect linked shop source traces; invalid-type boundaries and duplicate-row transaction risks remain for replay/strong review. No rows closed.

PK46.02 inventory reconciliation (2026-11-05): updated C2S-00cf to reflect linked ignore-whisper sender, selected server mutation and 0x00d1 result trace; persistence/encoding/replay remain open. No row closed.

PK46.02 inventory reconciliation (2026-11-05): C2S-00d0/00d3 retain UNREVIEWED classification; inventory now reflects the bounded no-sender search and unresolved generic-send/feature-path question. Stronger review needed before any support conclusion.

PK46.02 inventory reconciliation (2026-11-05): six chat-room registrations now reflect the linked explicit client-not-wired evidence; classifications remain UNREVIEWED pending PK24/PK31 owner review. No rows closed.

PK46.02 inventory reconciliation (2026-11-05): updated five trade C2S rows (00e4/00e6/00e8/00eb/00ed) to distinguish their linked client/server source traces from invitation, currency/item and delayed-message replay gates. No rows closed.

PK46.02 inventory reconciliation (2026-11-05): updated 00ef and party requests 00f9/00fc/00ff/0100 to reflect linked source-traced request and result paths, retaining transaction, option, prompt and asynchronous ordering replay gates. No rows closed.

PK46.02 inventory reconciliation (2026-11-05): updated 0102/0103/0112 to reflect linked source traces and their review gates; 0108/0118 now record bounded no-sender results while remaining UNREVIEWED. Corrected stale party-option status header. No rows closed.

PK46.02 inventory reconciliation (2026-11-05): 011b now reflects source-traced warp-skill request semantics with destination/replay open; 011d and cart-transfer 0126/0127/0128 record bounded no-sender findings for owner review, preserving UNREVIEWED classifications. No rows closed.

PK46.02 inventory reconciliation (2026-11-05): updated cart-off 012a, vending close/open 012e and vending-list 0130 to reflect their source traces and remaining replay gates. 0129 and alternate 012f retain no-sender/profile review states. No rows closed.

PK46.02 inventory reconciliation (2026-11-05): updated 0134/0143/0146 for source-traced vending purchase and NPC input/close paths; 013f/0140 retain bounded no-sender and profile/reachability review states. No rows closed.

PK46.02 inventory reconciliation (2026-11-06): guild info 014d/014f, emblem 0151 and member-position 0155 now reflect linked source traces and remaining UI/replay/trust gates. Guild GM requests 0149/0153 retain bounded no-sender and ownership review. No rows closed.

PK46.02 inventory reconciliation (2026-11-06): guild leave 0159, position info 0161 and invite 0168 now reflect source traces and replay gates. Guild expulsion/disband/create 015b/015d/0165 retain bounded no-sender and owner review. No rows closed.

PK46.02 inventory reconciliation (2026-11-06): guild invite/alliance replies 016b/0172, guild notice 016e, item identify 0178 and card composition 017a now reflect linked source traces and replay/review gates. Guild alliance request 0170 retains bounded no-sender/indirect-path review. No rows closed.

PK46.02 inventory reconciliation (2026-11-06): card insertion 017c and exit-to-Windows 018a now reflect source-traced request/result paths. Guild chat/alliance 017e/0180/0183 and production 018e retain bounded no-sender/feature-owner review. No rows closed.

PK46.02 inventory reconciliation (2026-11-06): production/reset/GM 0197/0198/019c/019d and pet 019f/01a1/01a5/01a7 now reflect bounded no-sender evidence; feature applicability remains open and classifications stay UNREVIEWED. No rows closed.

PK46.02 inventory reconciliation (2026-11-06): pet-emotion 01a9 now distinguishes selected server pet semantics from absent current-client action; making-arrow 01ae, change-cart 01af and open-vending 01b2 reflect source traces with profile/replay gates. No rows closed.

PK46.02 inventory reconciliation (2026-11-06): GM shift 01ba/01bb, recall 01bc/01bd and 01ce record bounded no-sender searches with feature-path review open; NPC string input 01d5 reflects source trace and ACP/boundary replay gate. No rows closed.

PK46.02 inventory reconciliation (2026-11-06): party-create alternate 01e8 and add-friend 0202 reflect source traces with profile/replay gates; novice/adoption/repair 01e7/01ed/01f7/01f9/01fd retain bounded no-sender feature review. No rows closed.

PK46.02 inventory reconciliation (2026-11-06): friend reply 0208 now reflects linked source traces and stale-prompt replay; friend removal, GM/check and PVP ranking rows 0203/0212/0213/020f/0217 retain bounded no-sender/path review. No rows closed.

PK46.02 inventory reconciliation (2026-11-06): ten PVP/ranking/refine/HOM registrations now reflect bounded no-sender searches with feature applicability still open; global chat 00f3 reflects its linked source trace with text/profile replay open. No rows closed.

PK46.02 inventory reconciliation (2026-11-06): eight homunculus/storage-password/mail registrations now reflect the linked bounded no-sender searches; feature applicability and indirect/generic construction remain open. Classifications remain UNREVIEWED. No rows closed.

PK46.02 inventory reconciliation (2026-11-06): ten auction/feeling/cooking requests now record bounded no-sender searches with generic-path/feature review open; mail send 0248 reflects linked source trace with delivery/retry replay open. No rows closed.

PK46.02 inventory reconciliation (2026-11-06): movement 00a7, skill-target 0072 and time-sync 007e reflect linked source traces with profile/replay gates. Service registrations 0273/0292/029f retain bounded no-sender and applicability review. No rows closed.

PK46.02 inventory reconciliation (2026-11-06): direction 0085, tick/action 0089, actor-name 008c, zone-entry 009b and item/char-name 009f/00a2 now reflect linked source traces, with profile collision and response/replay gates preserved. No rows closed.

PK46.02 inventory reconciliation (2026-11-06): take-item 00f5, storage transfer 00f7, ground skill 0113, drop-item 0116 and action 0190 reflect linked source traces with profile/result replay gates; service request 0288 retains bounded no-sender review. No rows closed.

PK46.02 inventory reconciliation (2026-11-06): shortcut update 02ba and view-equipment 02d6 reflect linked source traces with persistence/visual gates; newer-party invite/reply/tick and memorial command 02c4/02c7/02c8/02cf retain bounded no-sender feature review. No rows closed.

PK46.02 inventory reconciliation (2026-11-06): zone connection 0436 and action request 0437 reflect linked source traces with profile/replay review. Configuration, battle-chat, quest-state, account-name and progress-bar registrations 02d8/02db/02b6/01df/02f1 retain bounded no-sender path review. No rows closed.

PK46.02 inventory reconciliation (2026-11-06): skill-to-ID 0438 and item-use 0439 reflect linked source traces with profile/result replay gates; party-booking 0802/0806 retain bounded no-sender review. No rows closed.

PK46.02 inventory reconciliation (2026-11-06): last seven pointer-only states replaced with the linked source result: storage close 0193, mail window 0246, and conflicting 0094 profile evidence; four no-sender registrations remain owner/support review items. All classifications remain UNREVIEWED.

Plan handoff (2026-11-06): PK46.02 source inventory is preserved IN_PROGRESS with all 177 rows UNREVIEWED, linked evidence and explicit open states. Deferred its remaining source-conflict/no-sender/replay review to a stronger reviewer and moved active_step to the handoff's next task, PK05.05. No discovery rows closed.

## 2026-11-06 — PK05.05 seam deferral; resume PK46.02

Re-read the revised implementation plan, status, handoff, state and PK05.05 card. The plan now explicitly says a pending independent review alone does not block implementation; it still requires production-path evidence and an owned checkpoint. PK05.05 is deferred IN_PROGRESS because the current receive loop can observe EOF after an accepted acknowledgement already queued Login or quit, and has no production-visible signal to distinguish that handoff from an unexpected terminal receive. A direct closed-socket fallback could overwrite successful behavior. Record the seam needed and resume PK46.02 without closing any inventory row.

Validation: `python tools/check_packet_plan.py` passed before this checkpoint update; rerun after edits. No runtime code changed in this checkpoint.

## 2026-11-06 — PK46.02 inventory crosswalk audit; move to PK14.05

Validated the 177-row PK46.02 inventory: all evidence-note links resolve; all rows remain UNREVIEWED; grouped evidence states are 79 source traces, 83 bounded no-sender searches and 15 other open states. Found all 177 coverage_rows and primary/secondary owner cells blank. Recorded this in `evidence/PK46.02-inventory-link-audit-2026-11-05.md`; did not infer owners or close rows. PK46.02 remains IN_PROGRESS and is resumed after the currently next owned repair, PK14.05.

Validation: `python tools/check_packet_plan.py` will be rerun after this checkpoint update. No packet behavior changed.

## 2026-11-05 - PK14.05 committed; PK14.06 active

Committed `packet(PK14.05): preserve party markers across roster snapshots` as `8d3e4f13d9c0877b1d1eb7bcd7008fc7c5895070`. It includes atomic validation and production 0x0107 -> 0x00FB replay coverage plus the server-derived field oracle. Verification: deploy-off MinGW configuration confirmed; replay and app build command exited 0 with binaries newer than source timestamps; focused packet_stream_replay_checks passed 1/1 in 3.13s after the sandbox-localhost denial was cleared with the authorized loopback rerun; plan checker passed. Visual renderer evidence remains for PK14.04. PK14.06 is next; no discovery row was closed.

## 2026-11-05 - PK46.02 canonical owner crosswalk filled; packet review remains open

Follow-up to the earlier bounded audit: all 177 inventory discovery IDs match unique canonical coverage.csv rows. Filled coverage_rows with one-based CSV physical line numbers and copied primary/secondary packages. All rows remain UNREVIEWED; profile conflicts, no-sender support and per-request replay/source behavior remain open. Corrected the prior inference that ownership could not be mapped from this inventory.

## 2026-11-05 - PK14.06 remaining transition matrix deferred

The existing production OnExit/Qt display-data replay is partial. Deferred map-A-to-B, same-map warp, failed server-move/reconnect, character change and rendered UI checks for a focused production seam design; recorded exact gaps in evidence/PK14.06-deferral-2026-11-05.md. PK14.06 stays IN_PROGRESS. PK46.02 is now active.

## 2026-11-05 - PK46.02 full trace deferred; PK46.03 active

Completed the bounded PK46.02 owner/link crosswalk: exact unique coverage.csv mapping for 177/177 rows, all evidence pointers resolved. The remaining selected-profile/raw-buffer/UI/replay comparison spans 177 requests and cross-profile conflicts; keep the package IN_PROGRESS and every row UNREVIEWED pending narrower leaves and stronger review. Handoff now continues at PK46.03 login/character outbound and unsupported-action gates.

## 2026-11-05 - PK46.03 source correction committed; PK46.04 active

Committed `packet(PK46.03): correct character delete size evidence` as `4f083e2a4910e188a40d4ed0ccb9f2d9406ce631`. The recheck fixed an incorrect contract claim: current client packet is 46 bytes/key[40], selected server expects 56 bytes/opcode 0x01FB/key[50]. PK46.03 stays IN_PROGRESS; no discovery row was closed. PK46.04 is now active for bounded self-review.

## 2026-11-05 - PK47.03.002 catalog artifact recheck committed

Revalidated the corrected historical catalog: 1,380 unique rows; hash matches PK47.02.002 evidence; the four comment-only candidates remain absent; no historical or discovery rows were closed. Committed the evidence refresh as `packet(PK47.03.002): verify corrected catalog artifact`, SHA `9aa752ab` (full SHA is in git history). PK47.04 remains blocked on per-identity source evidence; active continuation remains PK46.04.


## 2026-11-05 — PK46.04 bounded replay attempt and verification

Tried to expose the production 0x007D load-end acknowledgement sender for a literal loopback capture. The replay target reported an unresolved SendMapLoadEndAckPacket symbol; cause not isolated, so the seam/test were removed and the capture gap remains open. No packet behavior, discovery row, or classification changed. Rebuilt packet_stream_replay_checks and open_midgard successfully after rollback; focused packet_stream_replay_checks CTest passed 1/1 with localhost access. Continue PK46.04 source/replay self-review; retain PK46.02/03 open statuses.

## 2026-11-05 — PK46.03 Legacy login parser comparison

Compared the selected default client `0x0064` sender and packed layout with the configured rAthena login-server registration, packed `PACKET_CA_LOGIN`, and `logclif_parse_reqauth_raw` field use. The 55-byte request and offsets match. Added the server-backed comparison to `PK46.03-account-login-variant-layouts.md` and its state evidence list. This is one variant only; no rows closed and PK46.03 remains IN_PROGRESS.
## 2026-11-05 — PK46.03 LoginChannel parser comparison

Compared the client `0x02B0` sender and 85-byte field map with rAthena’s registered `PACKET_CA_LOGIN_CHANNEL` parser. The layout matches; the parser consumes username, password, and clienttype and ignores the transmitted version, IP, MAC, and IsGravity fields. Evidence is recorded in the PK46.03 variant-layout note. Remaining variants/lifecycle branches are open; no rows closed.
## 2026-11-05 — PK46.03 PCBang and Login4 parser checks

Compared client/server layouts and registered parser paths for PCBang `0x0277` and Login4 `0x027C`. The layouts match; PCBang’s extra IP/MAC data is not consumed by the raw parser, while the MD5 parser consumes Login4’s digest and conditionally rejects it when `use_md5_passwds` is enabled. Variant selection and deployment acceptance remain open; no rows closed.
## 2026-11-05 — PK46.03 Login3 frame check

Compared the `0x01FA` Login3 clientinfo response struct and sender with the selected rAthena registration and MD5 parser. The 48-byte layout and dispatch match; parser semantics, challenge acquisition, configured support, and retry/cancel remain open. No rows closed.
## 2026-11-05 — PK46.03 Login2 frame check

Compared the `0x01DD` Login2 client response frame with the server packet registration, struct and MD5 parser. The 47-byte layout and dispatch match; challenge lifecycle, digest-mode variants, and retry/cancel remain open. No rows closed.
## 2026-11-05 — PK46.03 character-select parser comparison

Compared the client `0x0066` slot request with the configured character-server registration and parser. Both layouts are three bytes; server account/slot/deletion checks and map-server wait/reject outcomes are recorded. Transfer completion and retry behavior remain open; no rows closed.
## 2026-11-05 — PK46.03 character-create parser comparison

Compared client/server `0x0067` field maps under configured PACKETVER 20080910. Both use the same 37-byte name/stats/slot/hair layout; server version branches read the fields and hand them to `char_make_new_char`. The client ignores send failure before entering its wait state. Creation/refusal rule parity and failure recovery remain open; no rows closed.
## 2026-11-05 — PK46.03 character-server entry parser check

Compared the client 17-byte `0x0065` request with rAthena’s raw character-server parser. Offsets align; client `UserLevel` is read as server `login_id2`, so cross-service auth-node semantics still need verification. Send failure, response ordering and timeout cleanup remain open; no rows closed.
## 2026-11-05 — PK46.03 character-entry auth token verified

Traced the `0x0069` server `login_id2` token through client offset 12 storage and unchanged `0x0065` forwarding, then through character-server and login-server auth-node comparisons. The `m_userLevel` label is stale naming, not a different value. Lifecycle ordering/timeouts/send-failure recovery remain open; no rows closed.
## 2026-11-05 — PK46.02 final movement registry recheck

Revalidated `0x00A7` movement using PK01.02’s generated final registry and confirmed the source file hash matches the pinned baseline. Its final selected entry is 8-byte `clif_parse_WalkToXY` with destination offset 5; intermediate assignments do not define the final contract. Packet-specific production replay and deployed-profile proof remain open; no row closed.

## 2026-11-05 — PK46.02 movement production capture

Added a replay-only hook around the production movement sender and captured the default PacketVer23 request through a local TCP peer. The test verifies `a7 00`, 8-byte frame length, destination bytes `55 6a b0` at offset 5 for (341,683), and rejection without an open connection. Both `packet_stream_replay_checks` and `open_midgard` built; focused CTest passed 1/1 with localhost access. Deployed profile/live UI and inventory row review remain open; no row closed.


## 2026-11-05 — PK46.02 change-direction final-registry and replay check

Replaced the stale claim based on an intermediate `0x0085` assignment with PK01.02’s final 11-byte `clif_parse_ChangeDir` entry at offsets 7 and 10. Added a production loopback capture for PacketVer23; the test passed with the movement replay. The discovery row remains open under primary PK47 ownership; no row closed.


## 2026-11-05 — PK46.02 load-end acknowledgement capture

Added a replay-only external-linkage hook and captured the production `0x007D` send through a local TCP peer. The test verifies literal `7d 00` and closed-connection rejection; focused replay CTest passed 1/1 with localhost access. The first link failure was traced to placement inside a nested anonymous namespace and corrected. Lifecycle/retry parity remains open; no row closed.


## 2026-11-05 — PK46.04 inventory-link recheck

Reviewed the updated PK46.02 inventory audit against the active PK46.04 scope. It reports 177/177 existing evidence pointers and 177/177 unique canonical owner joins; all request classifications remain UNREVIEWED. This validates only link and ownership integrity. The selected-profile/raw-buffer/UI/source-branch/replay comparisons remain open, so PK46.04 and PK46.02 stay IN_PROGRESS. `python tools/check_packet_plan.py` passed: 51 packages, 235 numbered steps, 990 discovery owners, 21 findings, 19 ignored registrations; this is planning consistency only.


## 2026-11-05 — reconcile review sequencing

The review note retained an older instruction to resume PK46.02 before PK46.04. Added an explicit supersession note pointing to the current plan, handoff, status, and state checkpoint, which consistently place PK46.04 first. No package status or runtime claim changed.


## 2026-11-05 — PK46.02 CartOff production send

Added a replay-only seam and loopback capture for production `RequestCartOff`. Default PacketVer23 emitted `2a 01` (`0x012A`) and rejected a closed connection. The focused replay rebuilt and passed 1/1; deploy-off cache was confirmed. Replay binary SHA-256: `c1a3a84d8fe186c8c170c905ce8f89287191cf1f28ddf2c8cc399b213ea7d90c`. UI state feedback, send-failure presentation, and deployed server `NEW_CARTS` remain unverified; no classification is closed.


## Production request framing replay recheck (2026-11-05)

The shared action-request replay already covers sit and stand: independent PacketVer23 fixtures are `37 04 00 00 00 00 02` (target 0, sit) and `37 04 00 00 00 00 03` (target 0, stand). `ActionRequestBuilderReplay` validates both through `BuildActiveActionRequestPacket`, then sends the approved sample sequence through production `SendActionRequestPacket` to the loopback peer and compares the captured stream with the independent fixture stream. The focused replay passed 1/1 after the latest rebuild.

This closes request serialization/sender framing evidence for the sit/stand action values. Still open: driving the UI/chat caller with realistic player state, server response timing, local state transition and visual feedback. Do not close the row on framing alone.


## Production send replay (2026-11-05)

Added a replay-only seam for production `RequestNpcCloseDialog` and a loopback capture. With NPC ID `0x11223344`, the production sender emitted `46 01 44 33 22 11`, matching the selected fixed six-byte `0x0146` registry entry and little-endian NPC ID at offset 2. Zero NPC ID was rejected without emitting bytes; a closed connection also returned failure. Case ID: `npc_close_dialog_production_send`.

Verification: `cmake --build --preset build-mingw-qt-x64-ci --target packet_stream_replay_checks -j 6` succeeded; focused loopback `ctest --test-dir build-mingw-preset -R '^packet_stream_replay_checks$' --output-on-failure` passed 1/1 in 3.33 seconds. Replay binary: 19,954,707 bytes, SHA-256 `69e599d8257ed6374ca18ae41b5885923a68cf86440855092b9c156f1911c455`, modified `2026-11-05T02:56:22.090520Z`.

Still open: exercising both UI close routes, quest/dialog local effects, server CLOSE/STOP script-state behavior and output ordering, and visible dialog feedback. No discovery row is closed.


## Production request replay (2026-11-05)

Added a replay-only seam for `RequestIncreaseStatus` and loopback capture for the valid range boundaries. IDs 13 and 18 emitted exact independent five-byte frames `bb 00 0d 00 01` and `bb 00 12 00 01`. IDs 12 and 19 were rejected without bytes; a closed connection returned failure. Case ID: `status_increase_production_send`.

Verification: `cmake --build --preset build-mingw-qt-x64-ci --target packet_stream_replay_checks -j 6` succeeded; focused loopback `ctest --test-dir build-mingw-preset -R '^packet_stream_replay_checks$' --output-on-failure` passed 1/1 in 3.27 seconds. Deploy-off cache confirmed. Replay binary: 19,957,228 bytes, SHA-256 `f29d15b55b0539308a1fd0ef650935bec45f148e92ba981606f7d7a423002822`, modified `2026-11-05T02:58:08.501459Z`.

Still open: selected server stat eligibility/cost and rejection response, repeated-click behavior, and visible UI update. No discovery row is closed.


## Selected server eligibility and acknowledgement source recheck (2026-11-05)

The selected `clif_parse_StatusUp` reads the status ID and one-byte increase from the final registry positions and calls `pc_statusup`. That server routine rejects IDs outside `SP_STR..SP_LUK`, nonpositive increases, stat-cap overflow, and insufficient points; it caps the requested increase to the maximum affordable amount, computes cost by summing `PC_STATUS_POINT_COST` over the current stat interval, then recalculates and spends points. On success it sends cost and point updates followed by the status acknowledgement; if the resulting stat exceeds 255, a full status update follows the acknowledgement because the ack value is one byte. Refusal sends the same six-byte `0x00BC` acknowledgement with `ok=0` and no state mutation.

The client acknowledgement handler ignores a refusal and updates the selected character/session/player stat only on success. This source comparison closes the branch/contract description, but not timing behavior. Defer a production receive/replay for insufficient points, stat caps and repeated clicks until a stateful server-response fixture can verify ack ordering and UI feedback; keep the discovery row UNREVIEWED.


## Production send replay (2026-11-05)

Added a replay-only seam for production `RequestNpcNext` and a loopback capture. NPC ID `0x11223344` emitted exact bytes `b9 00 44 33 22 11`, matching selected fixed six-byte `0x00B9` and the little-endian NPC ID at offset 2. Zero ID was rejected without bytes; closed-connection failure was also checked. Case ID: `npc_next_production_send`.

Verification: `cmake --build --preset build-mingw-qt-x64-ci --target packet_stream_replay_checks -j 6` succeeded; focused loopback `ctest --test-dir build-mingw-preset -R '^packet_stream_replay_checks$' --output-on-failure` passed 1/1 in 3.21 seconds. Replay binary: 19,958,859 bytes, SHA-256 `cee4ac864b853d24b78ee5264ef49aecf6981b99477b5cdfea2bfd15484cbdd2`, modified `2026-11-05T03:00:40.197453Z`.

Still open: keyboard/button UI side effects on send failure, repeated clicks, stale/wrong NPC IDs, server script-state/timeout branches and visible text continuation. No discovery row is closed.

The application was also rebuilt with `cmake --build --preset build-mingw-qt-x64-ci --target open_midgard -j 6`; `[100%] Built target open_midgard`. Deploy-off cache confirmed. App binary: 19,685,328 bytes, SHA-256 `bf080bdb9f955d1e98cbdb36f47ef4b1c896bdb70d5a05c05d4daa74da84cf73`, modified `2026-11-05T03:01:18.257579Z`.


## Production wire-boundary replay (2026-11-05)

Added a replay-only seam for production `RequestNpcInputNumber` and captured two fixed ten-byte frames through a loopback peer for NPC `0x11223344`: value `0x7fffffff` produced `43 01 44 33 22 11 ff ff ff 7f`; value `0x80000000` produced `43 01 44 33 22 11 00 00 00 80`. Zero NPC ID emitted no bytes, and a closed connection returned failure. Case ID: `npc_input_number_production_send`.

Verification: replay target built and focused loopback CTest passed 1/1 in 3.39 seconds. `open_midgard` also built; deploy-off cache confirmed. Replay binary: 19,961,129 bytes, SHA-256 `068b3e1eec2d738b731bec91703170914cbb98649573136f39925ce97b0dd96a`, modified `2026-11-05T03:02:38.475437Z`. App binary: 19,685,492 bytes, SHA-256 `db1f5cfa0c802dd70462269b544f4e99dded87afd71c263d1f824c52d47ada31`, modified `2026-11-05T03:02:58.686615Z`.

This establishes only client serialization at the signed boundary. UI `strtoul` overflow behavior and selected server assignment to signed `npc_amount` require script/runtime fixtures; keep those checks open and do not close the discovery row.


## Production variable-frame replay (2026-11-05)

Added a replay-only seam for `RequestNpcInputString` and captured the selected variable-length frame through loopback. NPC ID `0x11223344` plus ASCII `Hi` emitted `d5 01 0b 00 44 33 22 11 48 69 00`: opcode, 11-byte total length, NPC ID, payload, and NUL. Zero ID, null text, empty text, and a payload that would make the packet exceed `u16` length were rejected without bytes; closed-connection failure was also verified. Case ID: `npc_input_string_production_send`.

Verification: replay target built and focused loopback CTest passed 1/1 in 3.47 seconds. `open_midgard` built; deploy-off cache confirmed. Replay binary: 19,964,059 bytes, SHA-256 `4fb90dbac8ab42ba3a8bdef8383ee55e130f202104b15fa5b2e602b7c69cdd28`, modified `2026-11-05T03:04:22.924707Z`. App binary: 19,685,804 bytes, SHA-256 `ab0fdf5811176fb4a3ce391bcc017445e4c27d40afd4f2711a3e0b9deaf72d76`, modified `2026-11-05T03:04:40.461467Z`.

Still open: deployed ACP/codepage compatibility, non-ASCII conversion and truncation, UI state recovery on send failure, and server script behavior at signed-value/buffer boundaries. No discovery row is closed.


## Production contact-send replay (2026-11-05)

Added a replay-only seam for `RequestNpcContact` and a loopback capture. NPC ID `0x11223344` emitted `90 00 44 33 22 11 01`, matching the selected fixed seven-byte frame and contact type 1. Zero ID emitted no bytes; closed-connection failure was verified. Case ID: `npc_contact_production_send`.

Verification: replay target built and focused loopback CTest passed 1/1 in 3.39 seconds. `open_midgard` built; deploy-off cache confirmed. Replay binary: 19,965,696 bytes, SHA-256 `f4f7c4111e597c5cc8399a8412b291f18e17772236791bb6b4adee498e0e4c4b`, modified `2026-11-05T03:06:08.362892Z`. App binary: 19,686,363 bytes, SHA-256 `34005aef9226227e960abcaa0ef571664fe862b9bef6f89d7d17ebb9997f955d`, modified `2026-11-05T03:06:26.428837Z`.

Still open: production map hit-test/action eligibility, target subtype dispatch, interaction rejection/timeout replies and visible NPC UI transition. No discovery row is closed.


## Production NPC menu-selection send replay (2026-11-05)

Added a replay-only seam for production `RequestNpcMenuSelection` and captured the normal-choice frame through loopback. NPC ID `0x11223344`, choice 3 emitted `b8 00 44 33 22 11 03`, matching the selected fixed seven-byte `0x00B8` registration. Zero NPC ID and zero choice emitted no bytes; closed-connection failure was also verified. The special `0xFF` cancel path is not exercised because it mutates active dialog state after a successful send. Case ID: `npc_menu_selection_production_send`.

Verification: replay target built and focused loopback CTest passed 1/1 in 3.49 seconds. `open_midgard` built; deploy-off cache confirmed. Replay binary: 19,967,651 bytes, SHA-256 `f6d144b802b2dc96187e4ca1baa93a7e2995f4dc1dacd432c02d140580926d4a`, modified `2026-11-05T03:07:42.734638Z`. App binary: 19,685,698 bytes, SHA-256 `12d44486069c4ff450e206a8677078381485146958f2ca910b13db91de582eab`, modified `2026-11-05T03:08:02.321170Z`.

Still open: UI hide/close effects on failure and cancel, repeated selections, stale NPC IDs, server menu/script state and visible continuation. No discovery row is closed.


## Production save-point request replay (2026-11-05)

Added a replay-only seam for `RequestReturnToSavePoint` and captured the production frame through loopback. It emitted exactly `b2 00 00`: selected opcode `0x00B2`, fixed length 3, type 0. A closed connection returned failure. Case ID: `return_save_point_production_send`.

Verification: replay target built and focused loopback CTest passed 1/1 in 3.49 seconds. `open_midgard` built; deploy-off cache confirmed. Replay binary: 19,968,722 bytes, SHA-256 `4b1e001d37c1e4088018d764fe98aa8314d88e3c1597d83d58faccc7316f6b2d`, modified `2026-11-05T03:09:33.874974Z`. App binary: 19,686,196 bytes, SHA-256 `7ad5b1d62c6a5cbac94833a38253a16c32d3ceadca7ef1c72db88d9435abe060`, modified `2026-11-05T03:09:52.654948Z`.

Still open: dead-character/server save-point behavior, menu close behavior on send failure, and visible result. No discovery row is closed.


## Production skill-up request replay (2026-11-05)

Added a replay-only seam for `RequestUpgradeSkillLevel` and captured the production sender under the default PacketVer23 profile. Skill ID `0x1234` emitted `12 01 34 12`, matching the selected four-byte `0x0112` request. Skill ID zero emitted no bytes, and a closed connection returned failure. Case ID: `skill_upgrade_production_send`.

Verification: replay target built and focused loopback CTest passed 1/1 in 3.53 seconds. `open_midgard` built; deploy-off cache confirmed. Replay binary: 19,970,953 bytes, SHA-256 `4ba47fadd9dc479b00ddd3bd0213ea64e285c8caade0bb4da5a53b5fca06e079`, modified `2026-11-05T03:11:16.546833Z`. App binary: 19,686,835 bytes, SHA-256 `ec09136982d108a9e209beb741f7568352667c124e981868c248fdcb001b639f`, modified `2026-11-05T03:11:37.237722Z`.

Still open: UI skill-point/upgrade eligibility under repeated clicks, selected server point consumption and refusal response, Qt apply-queue ordering, and visible skill-list refresh. No discovery row is closed.


## Production identify-cancel replay (2026-11-05)

Added a replay-only seam for `RequestIdentifyInventoryItem` and captured the production cancel sentinel through loopback. Under PacketVer23, signed item index `-1` emitted `78 01 ff ff`, matching the selected four-byte `0x0178` request. A closed connection returned failure. Case ID: `identify_item_cancel_production_send`.

Verification: replay target built and focused loopback CTest passed 1/1 in 3.66 seconds. `open_midgard` built; deploy-off cache confirmed. Replay binary: 19,973,388 bytes, SHA-256 `bc13867a1582d6a9b11f269dc561e3587aae96ce3d20ac4ffed5754b1058a6ce`, modified `2026-11-05T03:13:12.462748Z`. App binary: 19,687,672 bytes, SHA-256 `643a28ce993329831a9e3121420bf0cd8239d189fbf4878ee75ca78c9aeb377d`, modified `2026-11-05T03:13:32.968821Z`.

This covers the cancel sentinel send only. A positive item selection still requires a seeded inventory fixture, the server requires the identify menu skill state, and ack/refusal inventory refresh remains open. No discovery row is closed.


## Production shortcut update replay (2026-11-05)

Added a replay-only seam for `RequestShortcutSlotUpdate`, seeded session slot 5 with a skill and count, and restored the prior slot after capture. The production sender emitted `ba 02 05 00 01 44 33 22 11 66 55`: opcode `0x02BA`, absolute slot 5, skill flag, ID `0x11223344`, and count `0x5566`. Invalid slot and closed-connection failures were checked. Case ID: `shortcut_slot_update_production_send`.

Verification: replay target built and focused loopback CTest passed 1/1 in 3.78 seconds. `open_midgard` built; deploy-off cache confirmed. Replay binary: 19,975,975 bytes, SHA-256 `daf9875e1a46c2c722c7acef7dc8c17b7ffe13221df2d2a51e631d11c22d9f64`, modified `2026-11-05T03:15:33.785056Z`. App binary: 19,687,637 bytes, SHA-256 `288c031db025eb8ff8f52c7bacdfdb6dc8c0e7c56f3aba2447a695fc6da7b95c`, modified `2026-11-05T03:15:54.797748Z`.

Still open: UI drag/drop mutation and multi-slot ordering, server persistence/validation behavior, and rollback or retry after partial/failed send. No discovery row is closed.


## Production context-action send replay (2026-11-05)

Added a replay that calls production `ro::commerce::PlayerAction` for `PlayerContext_CheckEquipmentInfo`. Target AID `0x11223344` emitted `d6 02 44 33 22 11`, matching the selected fixed six-byte `0x02D6` registration. Zero target emitted no bytes; a closed connection returned failure. Case ID: `inspect_player_equipment_production_send`.

Verification: replay target built and focused loopback CTest passed 1/1 in 3.75 seconds. `open_midgard` target built; deploy-off cache confirmed. Replay binary: 19,977,577 bytes, SHA-256 `9b2413ea80797fb292e79e1351c392242fbc782408d50f1578437dad0bf5dc61`, modified `2026-11-05T03:17:35.066855Z`. App binary: 19,687,637 bytes, SHA-256 `288c031db025eb8ff8f52c7bacdfdb6dc8c0e7c56f3aba2447a695fc6da7b95c`, modified `2026-11-05T03:15:54.797748Z` (unchanged by this replay-test-only edit).

Still open: player-context hit testing, permission/refusal/offline/cross-map results and actual equipment rendering. No discovery row is closed.


## Production friend-request name-boundary replay (2026-11-05)

Added a replay calling production `ro::commerce::PlayerAction(RegisterFriend)`. A 23-byte name emitted the selected fixed 26-byte `0x0202` frame with the name at offset 2 and zero-filled terminator/padding. Empty name, 24-byte name, and zero target were rejected without bytes; closed-connection failure was verified. Case ID: `add_friend_request_production_send`.

Verification: replay target built and focused loopback CTest passed 1/1 in 3.73 seconds. `open_midgard` target built; deploy-off cache confirmed. Replay binary: 19,980,197 bytes, SHA-256 `0a64e9a68031783a7d9ae73dd620c1f130e8dae607c5880875fbc448aa7627c1`, modified `2026-11-05T03:19:23.582164Z`. App binary: 19,687,637 bytes, SHA-256 `288c031db025eb8ff8f52c7bacdfdb6dc8c0e7c56f3aba2447a695fc6da7b95c`, modified `2026-11-05T03:15:54.797748Z` (unchanged by this test-only edit).

Still open: non-ASCII name encoding, duplicate/already-friend server outcomes, invite/reply result receive path and visible UI feedback. No discovery row is closed.


## Production guild refresh sequence replay (2026-11-05)

Refactored `UIGuildWnd::Refresh` to call production-used `SendRefreshRequests`, then replayed the helper through loopback. With no guild ID, it emitted only permissions request `4d 01`. With a guild ID, it emitted the permissions request followed by five ordered six-byte `0x014F` packets for subtypes 0 through 4. Session guild ID was restored after the replay. Case ID: `guild_info_refresh_production_send`.

Verification: replay target built and focused loopback CTest passed 1/1 in 3.97 seconds. `open_midgard` built; deploy-off cache confirmed. Replay binary: 19,981,850 bytes, SHA-256 `72e751973e20eed5ce65d056c9fa4a1d43db9a7c1daf9f2f015cf9395d5d1972`, modified `2026-11-05T03:21:45.729884Z`. App binary: 19,687,694 bytes, SHA-256 `12db6d399ad8ff4195c74970f38b7cccb8d9be4c929569987d53a41afae9bfb2`, modified `2026-11-05T03:22:03.683415Z`.

Still open: partial-send failure behavior (the UI currently ignores SendPacket results), response ordering/duplicates, missing guild data and rendered UI reset. No discovery row is closed.


## Production guild-invite permission/send replay (2026-11-05)

Added a replay for production `ro::commerce::PlayerAction(InviteJoinGuild)`. With a local guild but neither master status nor permission bit `0x01`, it rejected without bytes. Granting permission emitted the selected 14-byte `0x0168` frame with target AID `0x11223344`, local AID `0x55667788`, and local GID `0x99AABBCC`. Zero target and empty name were rejected; closed-connection failure was verified. The original session guild/AID/GID fields were restored. Case ID: `guild_invite_production_send`.

Verification: replay target built and focused loopback CTest passed 1/1 in 3.85 seconds. `open_midgard` target built; deploy-off cache confirmed. Replay binary: 19,984,465 bytes, SHA-256 `ccfc88c9f1f873f06a4d17adc67fae51d21949df51e2dc9a44736a33021d5d2e`, modified `2026-11-05T03:23:25.126160Z`. App binary: 19,687,694 bytes, SHA-256 `12db6d399ad8ff4195c74970f38b7cccb8d9be4c929569987d53a41afae9bfb2`, modified `2026-11-05T03:22:03.683415Z` (unchanged by this test-only edit).

Still open: invite acceptance/rejection reply, duplicate/online server branches, membership update and visible UI feedback. No discovery row is closed.


## Production reject-whisper name-boundary replay (2026-11-05)

Added a replay calling production `ro::commerce::PlayerAction(RejectWhispering)`. A 23-byte name emitted the selected fixed 27-byte `0x00CF` frame with the name at offset 2 and zero operation byte at offset 26. Empty name, 24-byte name and zero target were rejected without bytes; closed-connection failure was checked. Case ID: `reject_whisper_production_send`.

Verification: replay target built and focused loopback CTest passed 1/1 in 3.96 seconds. `open_midgard` target built; deploy-off cache confirmed. Replay binary: 19,987,082 bytes, SHA-256 `5048011279387fe7ce27180b73c012769ebae0d256382c0654392cb339307d7a`, modified `2026-11-05T03:24:56.307246Z`. App binary: 19,687,694 bytes, SHA-256 `12db6d399ad8ff4195c74970f38b7cccb8d9be4c929569987d53a41afae9bfb2`, modified `2026-11-05T03:22:03.683415Z` (unchanged by this test-only edit).

Still open: non-ASCII name conversion, server ignore-list persistence/duplicate behavior, and `0x00D1` result display. No discovery row is closed.


## Production trade-request transaction replay (2026-11-05)

Added a replay for production `ro::commerce::PlayerAction(RequestDeal)` with a saved/restored commerce state and AID. At cached peer distance 2, the action emitted `e4 00 44 33 22 11`; successful send set the expected `tradeWaiting`, `partnerId`, and partner name. The same request failed on a closed connection and did not enter pending state. Case ID: `trade_request_production_send`.

Verification: replay target built and focused loopback CTest passed 1/1 in 3.94 seconds. `open_midgard` target built; deploy-off cache confirmed. Replay binary: 20,003,687 bytes, SHA-256 `057dfe0fbd425d8dd36463d2370c7f8f805b3275107c90dd022b2e206a249074`, modified `2026-11-05T03:26:41.290231Z`. App binary: 19,687,694 bytes, SHA-256 `12db6d399ad8ff4195c74970f38b7cccb8d9be4c929569987d53a41afae9bfb2`, modified `2026-11-05T03:22:03.683415Z` (unchanged by this test-only edit).

Still open: invite acceptance/rejection, duplicate/stale replies, timeout and trade-window response behavior, and visible UI parity. No discovery row is closed.


## Production mail-composer request replay (2026-11-05)

Added a replay for production `ro::commerce::PlayerAction(SendMail)` with saved/restored commerce state and AID. It emitted exact frame `46 02 00 00`; the transaction busy guard rejected a request without bytes. A closed-connection send returned failure while retaining the local recipient and visible composer, with `mailPending` false. Case ID: `mail_composer_production_send`.

Verification: replay target built and focused loopback CTest passed 1/1 in 3.94 seconds. `open_midgard` target built; deploy-off cache confirmed. Replay binary: 20,006,870 bytes, SHA-256 `bb448a7131d8f3c417ba1e3dcecb6b12d007f16ee150f67202cd4cbcf715a460`, modified `2026-11-05T03:28:44.054534Z`. App binary: 19,687,694 bytes, SHA-256 `12db6d399ad8ff4195c74970f38b7cccb8d9be4c929569987d53a41afae9bfb2`, modified `2026-11-05T03:22:03.683415Z` (unchanged by this test-only edit).

Still open: selected-server interpretation of zero payload, actual composer/attachment flow, server denial, cancel/reset behavior and send-failure UX. No discovery row is closed.


## Production storage-close guard/send replay (2026-11-05)

Added a replay-only seam for production `RequestCloseStorage`, seeded/restored `g_session.m_storageOpen`, and exercised both guard states. Closed storage emitted no bytes; open storage emitted exactly `93 01`, matching the selected fixed two-byte `0x0193` packet. Closed-connection failure was verified. Case ID: `storage_close_production_send`.

Verification: replay target built and focused loopback CTest passed 1/1 in 4.03 seconds. `open_midgard` built; deploy-off cache confirmed. Replay binary: 20,009,023 bytes, SHA-256 `3a42a5a2128cd90527fb93995019f8d785bbaca78a220e1e44ddd1039eec33ea`, modified `2026-11-05T03:31:15.361557Z`. App binary: 19,687,743 bytes, SHA-256 `221e18554e4195b616640a1040dc8c5c32c43f82519b23572e5062b100da0906`, modified `2026-11-05T03:31:37.354294Z`.

Still open: production storage-close notification/result ordering, duplicate requests and visible window lifecycle. No discovery row is closed.


## 2026-11-05 — PK46.04 mail-send replay checkpoint

Added the production `Commerce::Action(mail)` replay for variable `0x0248`, including accepted maximum field boundaries, rejected empty/over-limit inputs, pending-send guard, closed-connection failure, and pending-state behavior. Build of replay and app targets succeeded with deploy OFF; focused loopback CTest passed 1/1. See `evidence/PK46.04-bounded-self-review.md` and `evidence/PK46.02-map-mail-send-request.md` for the fixture and binary details. Remaining server result/delivery, attachment-credit, text-encoding and visible UI behavior stay open; no discovery row is closed.


## 2026-11-05 — PK46.04 mail-result routing replay

Verified client handling of selected 0x0249 result frames through production decode/routing: partial frame does not mutate state; failure clears pending and retains composer; success clears pending and closes composer. Replay/app targets built, focused CTest passed 1/1, deploy OFF confirmed, and packet-plan consistency checker passed. Exact fixture and replay binary provenance are in the PK46.04 and mail-send evidence notes. Server delivery/persistence and visible Qt behavior remain deferred; no discovery row is closed.


## 2026-11-05 — PK46.04 mail frame boundary extension

Added a short-value 0x0248 production frame fixture to verify zero-padded recipient/subject fields and body NUL, complementing the maximum boundary case. Replay/app targets rebuilt; focused CTest passed 1/1 in 4.21 seconds; plan consistency and diff checks passed. Updated binary provenance is in the PK46.04 evidence note. No discovery row is closed.


## 2026-11-05 — PK46.03 invalid character-selection gate

Added and replayed the local invalid-index gate: index 1 against a one-character list is rejected, does not change the next submode, and emits no bytes. Replay/app targets built; focused CTest passed 1/1 in 4.06 seconds; deploy OFF, plan checker and diff check confirmed. Binary provenance and limits are recorded in the PK46.03/PK46.04 evidence notes. Character-delete mismatch and account-server profile applicability remain deferred; no discovery row is closed.


## 2026-11-05 — PK46.04 0x0067 source parity review

Independently compared the current production 0x0067 fixture with the selected 20080910 character-server struct, registration and parser handoff. Field offsets and active opcode match; current server source hashes match PK01.02. No mismatch found in this bounded request review; live deployment/UI/result behavior and destructive delete remain open. Next bounded comparison is 0x0066. Details are in the PK46.04 evidence note.


## 2026-11-05 — PK46.04 0x0066 source parity review

Compared the existing 0x0066 production replay with the pinned 20080910 character-server packed struct, registration and parser. Both sides use a three-byte opcode-plus-slot frame, and the parser resolves that slot for the session account. No request mismatch found in this bounded check. Live deployment and response lifecycle remain open; destructive 0x0068 repair is deferred for stronger review. No discovery row is newly closed.


## 2026-11-05 — PK46.04 0x0246 semantics recheck

Rechecked the selected server source: type 0 at 0x0246 runs mail draft item/zeny removal helpers; current client sends type 0 when opening its composer. The parser evidence alone cannot establish whether this is intended reset-on-open legacy behavior or a destructive state mismatch. Recorded the uncertainty and deferred any code change for stronger review; no discovery row closed.


## 2026-11-05 — PK45.03 action-failure production replay

Added a production receive/router replay for selected 0x013B action-failure types: types 0–2 clear the tested retry fields idempotently; type 3 and unknown type 4 preserve them. Replay/app targets built; focused CTest passed 1/1 in 4.04 seconds; deploy OFF and plan checks passed. World/player target cleanup and visible chat rendering remain deferred to stronger review; no discovery row closed.


## 2026-11-05 — PK46.02 party-invite production send

Added a production dispatcher/send replay for 0x00FC: empty-party and zero-AID guards emit nothing; valid target 0x11223344 emits `fc 00 44 33 22 11`. Replay/app targets built; focused CTest passed 1/1 in 4.01 seconds; deploy OFF and plan checks passed. Native Qt event and server reply/UI lifecycle remain open; no discovery row closed.


## 2026-11-05 — PK46.02 party-invite result replay

Added production receive/router coverage for basic 0x00FD success, partial then complete extended 0x02C5 denial, and ignored unknown result code. It verifies queued chat-event content/color, not rendered Qt UI or membership updates. Replay/app targets built; focused CTest passed 1/1 in 4.11 seconds; deploy OFF and plan checks passed. No discovery row closed.


## 2026-11-05 — PK46.02 party-invite reply send

Added production sender replay for accept/reject 0x00FF frames and zero-party-ID rejection. The exact ten-byte fixtures, build/test result and binary provenance are in the reply evidence note. Focused CTest passed 1/1 in 4.25 seconds; deploy OFF and plan checks passed. Qt prompt lifecycle and server/roster results remain open; no discovery row closed.


## 2026-11-05 — PK46.02 party-create production send

Added production 0x00F9 create-party send replay for closed-connection failure, whitespace rejection, 23-byte name truncation/padding, and retained pending options after success. Replay/app targets built; focused CTest passed 1/1 in 4.31 seconds; deploy OFF and plan checks passed. Server result/option-application and Qt lifecycle remain open; no discovery row closed.


## 2026-11-05 — PK46.04 0x0102 source mismatch review

The selected server reads u32 EXP at offset 2 from a client packet that writes u16 EXP followed by u16 item options; for 0x0102 it ignores incoming item options and retains server state. Thus item bits contaminate the server EXP integer, while the six-byte response cannot reconcile item state. Recorded exact example values and source provenance limits. Deferred code/UI policy changes for stronger review; next bounded request is party leave/disband 0x0100. No discovery row closed.


## 2026-11-05 — PK46.02 party options and leave sends

Added production replays for all eight 0x0102 client option combinations and 0x0100 leader disband ordering. Failed 0x0102 transport preserves prior local cache; success updates it. Disband guards reject empty/non-master state and valid leader flow sends one member removal before leave. The 0x0102 selected-server field mismatch is documented and deferred for stronger review; these client captures do not certify it. Replay/app targets built; focused CTest passed 1/1 in 4.49 seconds; deploy OFF and plan checks passed. No discovery row closed.


## 2026-11-05 — PK46.02 party option and leave sends

Added production replays for all eight 0x0102 client option combinations and 0x0100 leader disband ordering. Failed 0x0102 transport preserves prior local cache; success updates it. Disband guards reject empty/non-master state and valid leader flow sends one member removal before leave. The 0x0102 selected-server field mismatch is documented and deferred for stronger review; these client captures do not certify it. Replay/app targets built; focused CTest passed 1/1 in 4.49 seconds; deploy OFF and plan checks passed. No discovery row closed.


## 2026-11-05 — PK46.02 party-option result replay

Added production receive/router coverage for six-byte 0x0101 denial and EXP success results. It checks denial cache preservation, partial-frame buffering, EXP updates and item-cache preservation. This does not resolve the selected server item-option mismatch. Replay/app targets built; focused CTest passed 1/1 in 4.53 seconds; deploy OFF and plan checks passed. No discovery row closed.


## 2026-11-05 — PK46.02 0x0108 sender gate

Rechecked current chat submission and selected server parser. The server accepts 0x0108 party messages, but the client Group tab only filters displayed history; submit passes text and whisper target, then the non-whisper path uses generic profile-selected global chat. No party-message sender found in current client source. Recorded hashes and retained PK17 primary ownership; whether to add a party-chat action or label Group display-only needs product review. No discovery row closed.


## 2026-11-05 — PK46.02 0x0103 removal source/send cross-check

Cross-checked the selected fixed 30-byte registration/parser and leader/AID/name checks against the production disband replay, which captured 0x0103 removal before 0x0100 leave. Pinned clif.cpp/registry hashes still match PK01.02. Server runtime/char-service lifecycle, denial/reordering, self-target and UI remain open; no discovery row closed.

## 2026-11-05 � PK46.02 0x0105 party-member removal notification

Added a production-router replay for the selected 31-byte 0x0105 packet. Ordinary withdrawal removes only the named cached member; CANT_LEAVE preserves membership and records the expected system notice. The selected server enum defines results 0�3, while the client also clears party state for result bit 0x10; that branch lacks matching selected-server enum/emitter evidence and is deferred for stronger review. Build and focused CTest passed with developer deployment OFF; artifact details are in [the evidence note](evidence/PK46.02-map-party-member-removal-notification.md). No discovery row closed.

## 2026-11-05 � PK46.02 0x00AB unequip acknowledgement correlation

Added a production-router replay for the selected 0x00AC acknowledgement. The server failure ack with item index zero preserves an unrelated pending item index; the matching item-index ack clears it. Build and focused CTest passed with developer deployment OFF. Reachability of the server invalid-index branch and visible Qt equipment feedback remain open; no discovery row closed. See the evidence note at evidence/PK46.02-map-unequip-item-request.md.


## 2026-11-05 � PK46.02 ignore-request server cross-check

Cross-checked the selected server semantics for 0x00CF, 0x00D0 and 0x00D3. The map parser mutates ignore state and emits 0x00D1/0x00D2 responses; 0x00D3 returns the current list in 0x00D4. No current client sender exists for ignore-all/list requests, so those rows remain unreviewed under PK16.03 pending profile/product review. No discovery row closed.


## 2026-11-05 � PK46.02 0x00EB trade lock sender

Added a production Commerce::Action replay for the two-byte lock request. Send failure leaves lockPending false, success emits EB 00 and marks pending, and a duplicate is suppressed. Build and focused CTest passed with developer deployment OFF. Server response ordering/races remain open; no discovery row closed. See the evidence note at evidence/PK46.02-map-trade-lock-request.md.


## 2026-11-05 � PK46.02 0x00E6 trade acceptance/decline

Added a production Commerce::Action replay for trade acceptance and decline. Send failure preserves the invitation; accept emits type 3 and waits for the server; decline emits type 4 and clears local invite state. Build and focused CTest passed with developer deployment OFF. Server stale/duplicate invite and response ordering remain open; no discovery row closed. See evidence/PK46.02-map-trade-ack-request.md.


## 2026-11-05 � PK46.02 0x00EF final trade confirmation

Added a production Commerce::Action replay. Commit requires both sides locked; failure leaves the request unpending; valid send emits EF 00 and rejects duplicates while pending. Build and focused CTest passed with developer deployment OFF. Server transfer/rollback and lifecycle races remain open; no discovery row closed. See evidence/PK46.02-map-trade-commit-request.md.


## 2026-11-05 � PK46.02 0x00ED outgoing trade cancellation

Added a production Commerce::Action replay. Failed send preserves the pending outgoing trade; success emits ED 00 and resets it. Build and focused CTest passed with developer deployment OFF. Timeout/disappearance, incoming-invite decline and server cancellation races remain open; no discovery row closed. See evidence/PK46.02-map-trade-cancel-request.md.


## 2026-11-05 � PK46.02 0x00E8 trade item offer

Added a production Commerce::Action replay for item offers. Send failure does not pend; success emits the eight-byte item/amount request, stores the pending item, and rejects duplicates until acknowledgement. Build and focused CTest passed with developer deployment OFF. Server validation and acknowledgement races remain open; no discovery row closed. See evidence/PK46.02-map-trade-add-item-request.md.


## 2026-11-05 � PK46.02 0x011B warp-skill destination

Added a production sender replay for 0x011B. Invalid destination and send failure preserve selection; a listed destination emits the expected 20-byte frame and clears pending state. Build and focused CTest passed with developer deployment OFF. Server response timing and destination semantics remain open; no discovery row closed. See evidence/PK46.02-map-warp-skill-selection-request.md.


## 2026-11-05 � PK46.02 0x012E close vending

Added a production Commerce::Action replay. Failed send preserves active vending state; success emits 2E 01 and clears local state. Build and focused CTest passed with developer deployment OFF. Server SQL failure and broadcast timing remain open; no discovery row closed. See evidence/PK46.02-map-close-vending-request.md.


## 2026-11-05 � PK46.02 0x0134 vending purchase

Added a production Commerce::Action replay. Stock overrun is rejected, send failure preserves local stock, and a valid purchase emits the 12-byte request without optimistic decrement. Build and focused CTest passed with developer deployment OFF. Server success-response parity and rollback remain open; no discovery row closed. See evidence/PK46.02-map-vending-purchase-request.md.


## 2026-11-05 � PK46.02 0x0178 positive item-identify selection

Added a seeded inventory production sender replay for the positive selection path. Failed send is rejected; valid item emits 78 01 30 75; already-identified item is rejected. Build and focused CTest passed with developer deployment OFF. Server skill prerequisites and post-ack inventory/UI transition remain open; no discovery row closed. See evidence/PK46.02-map-identify-item-request.md.


## 2026-11-05 � PK46.02 0x018A exit-to-Windows request

Added a production message-dispatch replay. Failed send clears pending disconnect state; success emits 8A 01 00 and suppresses duplicates. Build and focused CTest passed with developer deployment OFF. Server response and menu recovery remain open; no discovery row closed. See evidence/PK46.02-map-exit-to-windows-request.md.


## 2026-11-05 � PK46.02 0x01AF change-cart request

Added a production message-dispatch replay. It rejects types 0 and 6, sends each valid type 1�5 in the four-byte request frame, and fails when disconnected. Build and focused CTest passed with developer deployment OFF. Server state/rejection response remains open; no discovery row closed. See evidence/PK46.02-map-change-cart-request.md.


## 2026-11-05 � PK46.02 0x0208 friend invitation reply

Added a production Commerce::Action replay for accept/decline. Failed send preserves the prompt; successful accept and decline emit the expected 14-byte frames and clear it. Build and focused CTest passed with developer deployment OFF. Stale prompt timing and server result lifecycle remain open; no discovery row closed. See evidence/PK46.02-map-friend-reply-request.md.


## 2026-11-05 � PK46.02 global-chat request

Added a production serializer replay for global chat. Empty input, oversize frame and disconnected send fail; a valid request captures the active profile opcode, length, payload and terminator. Build and focused CTest passed with developer deployment OFF. Deployed profile, server policy and encoding compatibility remain open; no discovery row closed. See evidence/PK46.02-map-global-chat-request.md.

## 2026-11-05 — PK46.04 bounded 0x0248 server-side source review

Cross-checked the mail send request from map handler through inter-server queue, char-server recipient resolution, SQL transaction/attachment inserts, sender acknowledgement, and failure refund path. Recorded exact source locations and current file hashes in `evidence/PK46.02-map-mail-send-request.md`; the local external repository revision could not be read, so source provenance is hash-only and not pinned to PK01.02. No live service/database behavior was exercised and no coverage row was closed. Corrected two embedded NUL characters in the PK46.04 evidence note that had corrupted packet-ID text (`0x012A`, `0x0108`). Next: continue bounded claim-to-source/replay review; keep runtime/server provenance gates open.
## 2026-11-05 — PK46.04 bounded 0x00F3 parser/profile review

Compared the production client replay fixture (`Alice : hello`, active opcode/length/payload/NUL capture) with the selected 20080910 server registry and `clif_parse_GlobalMessage`/`clif_process_message` source. `0x00F3` frame shape agrees when the speaker prefix matches the authenticated character name; the parser rejects wrong prefixes, missing/embedded NUL or over-limit messages. The legacy `0x008C` profile maps to an unrelated fixed-length name request in the selected registry, so deployment profile resolution remains a gate. Source hash and limitations are in `evidence/PK46.02-map-global-chat-request.md`. No server runtime was exercised and no row was closed.
## 2026-11-05 — PK46.04 bounded 0x0208 server-field review

The selected registry and `clif_parse_FriendsListReply` agree on the 14-byte request fields: target account ID at offset 2, character ID at 6 (currently ignored by the handler), and u32 answer at 10. The target ID is supplied by the preceding 0x0207 invitation. The server validates pending request state, friend-list capacity, and optional `friend_auto_add`; an offline target returns without acknowledgement. Existing replay covers only client serialization. PK16 remains primary; server state/runtime behavior stays open. See `evidence/PK46.02-map-friend-reply-request.md`.
## 2026-11-05 — PK46.04 bounded 0x0134 vending-purchase cross-check

Reconciled the vending-purchase evidence note with the production sender replay already present in `PacketStreamReplay.cpp`. Its one-entry 12-byte fixture matches the selected 0x0134 parser layout and `clif_parse_PurchaseReq` passes seller AID, selected vending UID, and parsed rows to the transaction handler before clearing `vended_id`. Corrected stale note text claiming there was no production replay. The replay remains client-only; selected-server transaction outcomes, 0x0135 result handling, and malformed/multi-entry behavior are still open. See `evidence/PK46.02-map-vending-purchase-request.md`; PK38 remains primary and no row was closed.
## 2026-11-05 — PK46.04 bounded 0x012E close-vending failure review

Cross-checked the production client replay with selected server close source. The replay establishes two-byte request framing and local send-failure/success behavior only. The server handler no-ops if the session is not vending; otherwise SQL item deletion and shop-row deletion use short-circuit `||`, while server memory is cleared and close broadcast proceeds even after a query error. This exposes an unverified persistence failure boundary, not observed runtime behavior. Recorded the local vending.cpp hash in `evidence/PK46.02-map-close-vending-request.md`; SQL and remote broadcast remain open, no row closed.
## 2026-11-05 — PK46.04 bounded 0x01AF change-cart cross-check

Reconciled the change-cart evidence checklist with the production sender replay, then cross-checked the fixed u16 type field against the selected parser. Server skill/conditional-busy and level gates can silently return; `pc_setcart` also enforces its type range and push-cart skill. Existing replay covers client bounds/frame/send failure only, so server acceptance and asynchronous state notification remain open. PK37 remains primary; no row closed. See `evidence/PK46.02-map-change-cart-request.md`.
## 2026-11-05 — PK46.04 bounded 0x0178 identify cross-check

Corrected stale identify evidence text: a positive selection replay is present and captures a seeded un-identified item index; it also checks the client already-identified guard. Selected-server source requires `MC_IDENTIFY`, converts the signed index, silently rejects invalid item state after clearing menu/work state, and calls `skill_identify` for a valid item. The sender replay does not exercise the server or result/inventory-refresh path. See `evidence/PK46.02-map-identify-item-request.md`; no row closed.
## 2026-11-05 — PK46.04 0x018A replay frame correction

Compared the production sender against `PACKET_CZ_QUITGAME` and selected registry length 4. The replay had read only 3 bytes (`8A 01 00`), so its assertion could leave the trailing zero unread. Corrected it to consume `sizeof(PACKET_CZ_QUITGAME)` and compare `8A 01 00 00`; the focused evidence note now records the complete frame. The replay target rebuilt with deploy OFF; after the sandbox-default local connection failed before assertions, the loopback-enabled focused CTest passed 1/1 in 4.42 sec. Disconnect acceptance/refusal and recovery remain open.
## 2026-11-05 — PK46.04 bounded 0x00FC party-invite cross-check

Cross-checked the production sender fixture against selected 0x00FC registry/parser fields. The fixed six-byte request places target AID at offset 2; the selected server resolves that AID and then applies party-leader and invitation-policy checks. Production replay does not exercise those server gates or roster/result transitions. PK16 remains primary; no row closed. See `evidence/PK46.02-map-party-invite-request.md`.
## 2026-11-05 — PK46.04 bounded 0x00FF party-reply cross-check

Compared the production accept/reject replay frames with selected parser fields and `party_reply_invite`. Frames match the ten-byte party-ID/u32-flag layout. The server validates pending party state; successful acceptance requests a char-service add asynchronously, while rejection or stale state may only notify an online inviter. The client replay does not exercise those states or roster timing. PK16 remains primary; no row closed. See `evidence/PK46.02-map-party-invite-reply-request.md`.
## 2026-11-05 — PK46.04 bounded 0x00F9 party-create cross-check

Reconciled the party-create evidence checklist with the existing production sender replay, then compared its fixed 26-byte name frame with the selected parser. The server passes the name to asynchronous `party_create` and supplies zero option arguments; client option bits remain locally pending for later application. Replay covers ASCII truncation and send failure, not deployed multibyte encoding, asynchronous result order, or eventual option updates. PK16 remains primary; no row closed. See `evidence/PK46.02-map-party-create-request.md`.
## 2026-11-05 — PK46.04 bounded 0x0100 party-leave cross-check

Compared the production 32-byte disband replay with the selected two-byte leave parser and `party_leave`. The replay checks member-removal-before-leave serialization, but server leave may reject based on map/instance policy or queue asynchronous character-service state. Cross-service ordering and final roster consistency remain a stronger-review item already recorded in `evidence/PK46.02-map-party-leave-request.md`; no row closed.
## 2026-11-05 — PK46.04 bounded 0x0103 self-target review

Confirmed the selected map-side party-removal helper has no explicit self-target guard; a leader matching their own AID/name can reach an asynchronous character-service leave request tagged `PARTY_MEMBER_WITHDRAW_EXPEL`. Replay covers only ordinary member removal. Self-expulsion behavior remains deferred for stronger cross-service review. No discovery row closed; see `evidence/PK46.02-map-party-member-removal-request.md`.
## 2026-11-05 — PK46.04 bounded 0x0193 storage-close cross-check

Used the effective selected registry row to disambiguate repeated 0x0193 registrations in the versioned server header. The selected two-byte request dispatches to `clif_parse_CloseKafra`, which chooses normal/guild/premium close behavior from session storage state. Existing replay covers client bytes/open-state guard, not server mode or persistence. Keep notification ordering and UI lifecycle open; no row closed. See `evidence/PK46.02-map-storage-close-request.md`.
## 2026-11-05 — PK46.04 bounded 0x00ED trade-cancel cross-check

Updated the stale checklist to reflect existing explicit-close success/failure replay. Selected server source distinguishes pending invitations from active trades; active cancellation returns staged items/zeny and clears state on both online participants. The production replay covers only client pending-state changes and request bytes. Timeout/disappearance, active-offer rollback, late-response and disconnect races remain open under PK37. See `evidence/PK46.02-map-trade-cancel-request.md`.
## 2026-11-05 — PK46.04 bounded 0x00E8 fixture review

The 0x00E8 production sender replay uses client inventory index 65000. Selected server `trade_tradeadditem` receives a signed `int16` index and rejects negative/out-of-range slots, so the replay proves wire serialization, not server-valid offer acceptance. Client item-index to server-slot mapping and narrowing behavior require stronger version-specific review. Corrected evidence wording and marked the fixture limitation; no row closed.
## 2026-11-05 — PK46.04 bounded 0x00E8 currency branch review

Recorded the untested currency branch: item index zero, u32 amount, local `localZeny` update only after transport success, and no sender ack from the selected server; server balance/cap failures cancel the trade. A valid replay fixture needs selected-character money, while the available setter also mutates other session state. Deferred fixture setup/restoration for stronger review; no row closed. See `evidence/PK46.02-map-trade-add-item-request.md`.
## 2026-11-05 — PK46.04 bounded 0x00EF commit cross-check

Updated the focused checklist to reflect replayed remote-lock guard/send failure/frame/duplicate-pending behavior. Selected server commit waits for both trade sides and performs aggregate checks before staged transfer; transaction outcome and failure paths remain unverified. Item-add rollback/partial transfer and race behavior need stronger review under PK37. See `evidence/PK46.02-map-trade-commit-request.md`.
## 2026-11-05 — PK46.04 bounded 0x00EB trade-lock cross-check

Updated the trade-lock checklist to reflect the existing sender replay (send failure, frame, pending flag, local duplicate guard). Selected server `trade_tradeok` checks active/unlocked state and peer presence, then sets the lock and notifies both participants. Replay does not exercise those server transitions or lock/item-ack response ordering. Keep those gates open under PK37; no row closed.
## 2026-11-05 — PK46.04 bounded 0x00E6 trade-reply cross-check

Updated the acceptance/replay checklist: production sender replay already covers accept, decline, and failed-accept preservation. Selected server validates reciprocal partner state, resolves the target, and applies distance/session gates before opening the trade. Stale/disappeared target and timeout response timing remain open under PK37. See `evidence/PK46.02-map-trade-ack-request.md`; no row closed.
## 2026-11-05 — PK46.04 bounded 0x011B warp-skill cross-check

Reconciled the warp-skill note with its existing production sender replay and selected server source: fixed framing, cancel sentinel, and sender state transitions are covered. Live server/runtime outcomes and stale/asynchronous destination behavior remain open; no discovery row was closed.

## 2026-11-05 — PK46.04 storage-transfer stronger-review boundary

Recorded the paired `0x0094`/`0x00F7` transfer item for stronger review: request layouts and selected parser/cache effects are documented, while coupled cross-packet success ordering and replay remain open.

## 2026-11-05 — PK46.04 inventory-drop replay boundary

Recorded `0x0116` for stronger replay/correlation review. Request and selected parser contracts are documented, but production byte replay plus successful/unmatched removal acknowledgement ordering needs a verified index crosswalk; no discovery owner was closed.

## 2026-11-05 — PK46.04 emotion throttle source recheck

Confirmed the selected server parser against the PK01.02 `clif.cpp` hash. The one-second integer guard updates its timestamp on rejection; production timing and rendered effect behavior remain open.

## 2026-11-05 — PK46.04 vending-list gate cross-check

Confirmed parser/helper gates for `0x0130` and that the inspected helper has no proximity check. The `clif.cpp` parser matches PK01.02, but `vending.cpp` is not pinned in that snapshot; privacy/reachability, stale-board behavior, and replay stay open.

## 2026-11-05 — PK46.04 empty open-vending reset cross-check

Confirmed selected `0x01B2` parser clears pre-vend/work-in-progress on flag zero, then returns for an empty title before the open-vending call; no acknowledgement is sent on that empty close/reset path.

## 2026-11-05 — PK46.04 equip-ack correlation cross-check

Confirmed `HandleEquipItemAck` matches the documented behavior: only matching index clears the pending request, while any successful nonzero-index/nonzero-location ack updates wear state and rebuilds equipment appearance. Late/mismatched replay remains open.

## 2026-11-05 — PK46.04 card-composition response cross-check

Confirmed selected 0x017A server early returns and 0x017B list send only when eligible targets exist; the client opens a received list without request correlation. Delayed/duplicate response and insertion replay remain open.

## 2026-11-05 — PK46.04 trade-request replacement ordering

Recorded that the selected trade request clears the requester’s prior trade partner before busy-target, permission, and distance checks. The `trade.cpp` snapshot is not pinned in PK01.02; runtime race/order replay remains open.

## 2026-11-05 — PK46.04 guild-emblem no-response cross-check

Confirmed selected server sends 0x0152 only for a found guild with a nonempty emblem. Unknown guild and empty emblem return silently; client throttle, cache, decompression, and visible refresh replay remain open.

## 2026-11-05 — PK46.04 guild-position cross-service boundary

Recorded the map parser gates and the character-service AID/CID crosswalk. Leader changes use a separate CID path; malformed-tail and authorization provenance need stronger review across the unpinned service source.

## 2026-11-05 — PK46.04 global-chat framing cross-check

Confirmed selected pre-20151001 framing requires a single final NUL and validates the speaker prefix; embedded NUL with trailing bytes is rejected. Runtime profile selection/configuration and production replay remain open.

## 2026-11-05 — PK46.04 ground-skill range boundary

Recorded `0x0113` for stronger review: common layout/gates are documented, while signed-coordinate and skill-specific range/result semantics need broader server review and replay.

## 2026-11-05 — PK46.04 guild-alliance reply boundary

Recorded `0x0172` for stronger review: prompt correlation, guild permission checks, asynchronous result order, and stale/duplicate response replay remain open across service boundaries.

## 2026-11-05 — PK46.04 guild-invite reply correlation

Confirmed the reply uses current pending guild ID rather than a unique invite token, and server accepts any nonzero answer. Same-guild stale-prompt behavior needs replay; helper source is not hash-pinned in PK01.02.

## 2026-11-05 — PK46.04 guild-notice authorization cross-check

Confirmed parser master gate and marker cleanup, plus helper-side packet/session guild-ID equality before async forwarding. Helper source is not pinned in PK01.02; delivery and UTF-8 boundaries remain open.

## 2026-11-05 — PK46.04 guild-leave map-side cross-check

Confirmed map-side identity/map restrictions and pre-forward guild-bound trade cancellation. Leader policy is not enforced in that function and asynchronous rejection/state ordering remains for stronger cross-service review; source file is unpinned in PK01.02.

## 2026-11-05 — PK46.04 guild-position-info malformed-tail boundary

Recorded a stronger-review question: the parser steps through 40-byte records under `offset < declaredLength` without an explicit exact-tail check; dispatch bounds and partial-record impact require focused review.

## 2026-11-05 — PK46.04 whisper fanout boundary

Recorded `0x0096` for stronger end-to-end review: map-server fanout, request-ID aggregation, timeouts, and name/message encoding limits need cross-service replay.

## 2026-11-05 — PK46.03 LoginChannel field-read cross-check

Verified the selected raw-auth parser reads username/clienttype/password and derives IP from the socket session; transmitted version/IP/MAC/Gravity fields do not enter that path. Deployed profile applicability remains open.

## 2026-11-05 — PK46.03 character-entry send result

Verified the `0x0065` client sender ignores the transport return and unconditionally waits for the character list; response timeout and disconnect cleanup remain open.

## 2026-11-05 — PK46.03 unsupported login actions

Revalidated account-request and intro UI messages terminate in explicit not-implemented status branches with no packet send; kept them as negative-path evidence.

## 2026-11-05 — PK46.03 Login3 field-read check

Confirmed the MD5 parser ignores version/clientinfo, uses the socket-derived IP for logging, and rejects when `use_md5_passwds` is enabled. Deployed profile/configuration remains unverified.

## 2026-11-05 — PK46.03 character-create send result

Verified `0x0067` rejects disconnected calls but ignores transport failure when connected, then opens the wait window and reports a pending creation request. Server response and timeout recovery remain open.

## 2026-11-05 — PK46.03 Login4 parser route

Verified `0x027C` is registered to the shared MD5 parser with its packed size; version/MAC are not auth inputs, and the MD5-password configuration gate applies. Deployment profile remains unverified.

## 2026-11-05 — PK46.03 account-login variant crosswalk

Reconciled six login opcodes into selected raw-auth and MD5-auth parser families. Source routing and field reads are documented; deployed profile/settings and security compatibility remain open.

## 2026-11-05 — PK46.03 account pre-auth send failures

Verified failed optional 0x0200/0x0204 sends do not abort account authentication; failed 0x01DB sends return before waiting-state mutation. Deployed service requirements remain open.

## 2026-11-05 — PK46.03 character-delete boundary

Reconfirmed the reachable client/server mismatch (`0x0068`/46-byte/key[40] vs selected `0x01FB`/56-byte/key[50]); kept immediate deletion unsupported pending version-specific layout and peer review.

## 2026-11-05 — PK46.03 create-slot handoff

Recorded the selector's locally derived slot clamp and the mode's unchecked `wparam` storage before u8 serialization; server slot acceptance and stale/settings-derived selection need lifecycle review.

## 2026-11-05 — PK46.04 load-end parser gate

Cross-checked `0x007D` selected parser gates against the pinned server source; existing replay only proves sender framing/closed-send behavior. Bootstrap/retry lifecycle remains open and the discovery row stays UNREVIEWED.

## 2026-11-05 — PK46.04 NPC-contact parser cross-check

Verified selected `0x0090` target-AID dispatch and session gates; type byte is ignored, mob/player click becomes an action request, and placed NPC contact can be suppressed by progress/floating-NPC gates. UI modifiers and lifecycle replay remain open.

## 2026-11-05 — PK46.04 registered broadcast parser

Verified `0x0099` is a bounded message-to-`@kami` atcommand dispatch in the selected server parser. No current client sender was found in the existing bounded search; primary ownership/classification remain unchanged and open.

## 2026-11-05 — PK46.04 restart branch cross-check

Verified the selected `0x00B2` type-0 respawn and type-1 gated character-select branches; other values have no parser branch. Save-point sender replay exists, while character-select transition replay remains open.

## 2026-11-05 — PK46.04 NPC-menu parser cross-check

Verified `0x00B8` choice validation and cancel sentinel behavior in the selected parser. Client closes menu windows before send completion; failure recovery/script lifecycle replay remain open.

## 2026-11-05 — PK46.04 NPC-next continuation check

Confirmed `0x00B9` delegates active-NPC identity, timeout, and proximity checks to `npc_scriptcont`. Helper source is not pinned in PK01.02; stale-ID and send-failure/UI replay remain open.

## 2026-11-05 — PK46.04 status-increase server validation

Verified the unsigned request amount, stat-ID/positive-amount checks, increment cap, maximum/cost validation, and success mutation path; `pc.cpp` is unpinned in PK01.02. Boundary and rejection replay remain open.

## 2026-11-05 — PK46.04 user-count query

Verified `0x00C1` sends the requester a user-count response from `map_getusers()`. The client-side sender remains absent in bounded search; ownership/classification stay open.

## 2026-11-05 — PK46.04 NPC shop deal-type parser cross-check

Confirmed pinned `clif.cpp` trading/session gate and current unpinned `npc.cpp` proximity, shop-type, visibility, and raw deal-type branch behavior for `0x00C5`. Production replay/UI lifecycle remains open; the row remains UNREVIEWED.

## 2026-11-05 — PK46.04 NPC shop transaction caller cross-check

Confirmed the selected `0x00C8`/`0x00C9` callers floor variable-row counts, clear `npc_shopid`, and emit a result on both transaction and early-rejection paths. Existing duplicate-row partial-transaction risks still require production-path reproduction and stronger review before repair; both rows remain UNREVIEWED.

## 2026-11-05 — PK46.04 privileged request parser cross-check

Confirmed pinned `clif.cpp` maps `0x00CC`/`0x00CE` to privileged kick, monster-damage, NPC-unload, and kick-all paths. The bounded client search found no sender/action; PK47 ownership and UNREVIEWED classifications remain pending primary-owner confirmation.

## 2026-11-05 — PK46.04 whisper-ignore mutation cross-check

Confirmed pinned `clif.cpp` truncates the name field to 23 bytes and applies type 0 as add, every nonzero type as remove, with duplicate-add success, list-full/missing-remove failures, and `0x00D1` result sends. Persistence/encoding/replay remain open; PK16 ownership and UNREVIEWED status remain.

## 2026-11-05 — PK46.04 ignore-all/list request cross-check

Confirmed pinned `0x00D0` mutates session ignore-all/per-name state according to its type byte and `0x00D3` returns that session's per-name list. No current sender was found; client support/response consumption remain open, with PK16 ownership and UNREVIEWED status retained.

## 2026-11-05 — PK46.04 chat-room request parser cross-check

Confirmed pinned `0x00D5` creation gates and `0x00D9` room-ID/password delegation. Current client code leaves chat-room UI unwired and no sender was found; PK31 ownership and PK24/PK31 lifecycle review remain open, with both rows UNREVIEWED.

## 2026-11-05 — PK46.04 trade-request gate cross-check

Confirmed pinned `0x00E4` requester activity/chat exception, recipient no-ask and mail-writing responses, configured skill gate, then trade-request delegation. Invitation replacement and accept/reject/timeout replay remain open; row remains UNREVIEWED.

## 2026-11-05 — PK46.04 trade acknowledgement stale-partner cross-check

Confirmed `0x00E6` forwards the action byte; the current helper validates reciprocal session state, clears only the requester on stale/mismatched partner, cancels both sides for type 4, and ignores other action values. Helper source is unpinned; stale-partner replay remains open and the row remains UNREVIEWED.

## 2026-11-05 — PK46.04 trade-offer parser width cross-check

Confirmed `0x00E8` keeps the zeny amount signed 32-bit but narrows item amount to signed 16-bit after `server_index` conversion. Transaction/aggregate-lock replay remains open for stronger review; the row remains UNREVIEWED.

## 2026-11-05 — PK46.04 trade-cancel dispatch cross-check

Confirmed pinned `0x00ED` delegates to `trade_tradecancel`; timeout/disappearance, late invitation interleavings, and teardown replay remain open. The row remains UNREVIEWED.

## 2026-11-05 — PK46.04 party-create gate cross-check

Confirmed pinned `0x00F9` party-lock and configured skill gates, fixed name copy, and zero option arguments to `party_create`. Existing sender replay covers local pending state only; asynchronous result/option lifecycle remains open and the row remains UNREVIEWED.

## 2026-11-05 — PK46.04 party-leave parser cross-check

Confirmed pinned `0x0100` ignores payload and delegates to `party_leave(*sd, true)` after the session gate. Existing production replay covers client disband ordering; roster/map transition evidence remains open and the row remains UNREVIEWED.

## 2026-11-05 — PK46.04 party-member removal parser cross-check

Confirmed pinned `0x0103` forwards AID and fixed name to `party_removemember` after the session gate. Self-target consequences and multiple-removal/server ordering remain for PK15 review; the row remains UNREVIEWED.

## 2026-11-05 — PK46.04 party-option parser cross-check

Confirmed pinned `0x0102` requires the party leader and reads experience only, retaining the server's current item option. The client item's ignored-field behavior matches the note; option-update response/replay remains open and the row remains UNREVIEWED.

## 2026-11-05 — PK46.04 party-chat parser cross-check

Confirmed pinned `0x0108` delegates through shared message validation before `party_send_message`; current client chat submission has no party-chat request path. PK17 support review remains open and the row remains UNREVIEWED.

## 2026-11-05 — PK46.04 skill-up helper boundary cross-check

Confirmed `0x0112` reaches `pc_skillup`; normal skills require points, permanent ownership, and class-tree headroom before mutation/refresh, while guild/homunculus cases delegate elsewhere. `pc.cpp` is unpinned and failure/queue/UI replay remains open; the row remains UNREVIEWED.

## 2026-11-05 — PK46.04 stop-attack registration cross-check

Confirmed pinned `0x0118` stops the unit attack and clears continuous-attack state; the bounded client search found no sender. User cancellation and target teardown support remain unresolved; the row remains UNREVIEWED.

## 2026-11-05 — PK46.04 warp-skill selection dispatch cross-check

Confirmed pinned `0x011B` clears WIP before menu-skill identity validation, applies action gates with storage/Teleport exceptions, resolves the map name, and delegates to `skill_castend_map`. Server/runtime destination and stale-menu behavior remain open; the row remains UNREVIEWED.

## 2026-11-05 — PK46.04 memo/cart parser cross-check

Confirmed pinned `0x011D` gates memo on not-dead state; `0x0126`/`0x0127` enforce trade/cart/action/map restrictions before inventory-cart helpers. No current sender was found; support and state-transition review remain open, with rows UNREVIEWED.

## 2026-11-05 — PK46.04 storage/cart parser cross-check

Confirmed pinned `0x0128`/`0x0129` vending/cart/map gates, storage-flag routing, and distinct storage/cart index conversions. No current sender was found; PK37 transition review remains open and both rows remain UNREVIEWED.

## 2026-11-05 — PK46.04 cart-off compile-flag boundary

Confirmed pinned `0x012A` has different cart/vehicle clearing behavior under `NEW_CARTS`; deployed build configuration and state replay remain unverified. The row remains UNREVIEWED pending profile review.

## 2026-11-05 — PK46.04 alternate vending profile boundary

Confirmed pinned `0x012F` follows the legacy open-store prefix/layout branch, distinct from the alternate opcode path with a flag and WIP reset. No current sender was found; profile reachability remains unresolved and the row remains UNREVIEWED.

## 2026-11-05 — PK46.04 vending-list request gate cross-check

Confirmed pinned `0x0130` rejects requests during an NPC dialog, then dispatches the target ID. Seller reachability/privacy and stale-board behavior remain unverified; the row remains UNREVIEWED.

## 2026-11-05 — PK46.04 GM item/monster parser cross-check

Confirmed pinned `0x013F` dispatches money/item/monster commands, with monster resolution gated by command permission; no current sender was found. Preserve PK38 ownership and UNREVIEWED status pending profile/support review.

## 2026-11-05 — PK46.04 alternate map-move parser cross-check

Confirmed pinned `0x0140` converts the fixed request into an `@mapmove` atcommand; no gameplay sender was found. Preserve PK45 ownership and UNREVIEWED status pending admin/profile review.

## 2026-11-05 — PK46.04 NPC numeric-input parser ordering cross-check

Confirmed pinned `0x0143` stores the value and optionally refreshes idle time before script-continuation validation. Signedness boundary and script/UI replay remain open; the row remains UNREVIEWED.

## 2026-11-05 — PK46.04 NPC close-dialog parser ordering cross-check

Confirmed pinned `0x0146` requires active NPC state, optionally refreshes idle time, and delegates close continuation by GID. Client pre-send window closure and script/UI failure recovery remain open; the row remains UNREVIEWED.

## 2026-11-05 — PK46.04 GM mute request parser cross-check

Confirmed pinned `0x0149` routes its fields to a privileged `/mute` atcommand after type and target checks; no current sender was found. Preserve owner/support review and UNREVIEWED status.

## 2026-11-05 — PK46.04 guild information request cross-check

Confirmed pinned `0x014D` returns master/member info and `0x014F` reads a 32-bit request type, serves types 0–4 only for guild/BG members, and logs other values. Refresh response order and UI state remain open; preserve PK12/PK50 owners and UNREVIEWED rows.

## 2026-11-05 — PK46.04 guild mutation parser cross-check

Confirmed pinned `0x0153` emblem gates, `0x015B` expulsion-field delegation, `0x015D` guild-break key delegation, and `0x0165` guild-create map/clan gates. No current senders were found; preserve PK12 ownership and UNREVIEWED status pending helper/feature review.

## 2026-11-05 — PK46.04 guild member-position boundary cross-check

Confirmed pinned `0x0155` floors whole entries, dispatches positive position changes, and returns immediately after position-0 leadership transfer. Partial-tail and leadership/configuration behavior remain for PK12 review; the row remains UNREVIEWED.

## 2026-11-05 — PK46.04 guild-leave parser cross-check

Confirmed pinned `0x0159` forwards the fixed guild/AID/CID/reason fields to `guild_leave` after a session check. Identity and async membership/result ordering remain open; PK12 ownership and UNREVIEWED status remain.

## 2026-11-05 — PK46.04 guild-position metadata tail cross-check

Confirmed pinned `0x0161` uses `i < declaredLength` with 40-byte records and no visible exact-tail check after the guild-master gate. Partial-record risk needs malformed-length replay/PK12 review; the row remains UNREVIEWED.

## 2026-11-05 — PK46.04 guild-invite target cross-check

Confirmed pinned `0x0168` resolves the requested AID only to an online map session before `guild_invite`. Offline target, invitation replacement, response ordering, and membership replay remain open; PK12 ownership and UNREVIEWED status remain.

## 2026-11-05 — PK46.04 guild reply dispatch cross-check

Confirmed pinned `0x016B` forwards guild ID/answer and `0x0172` forwards account ID/answer to guild-service helpers without parser-level answer checks. Correlation and cross-server ordering remain open; preserve PK12 ownership and UNREVIEWED status.

## 2026-11-05 — PK46.04 guild notice/alliance parser cross-check

Confirmed pinned `0x016E` guild-master and language-marker handling; `0x0170` guild-master/guild-lock/online-target/noask gates before alliance delegation. Sender support and delivery/cross-server ordering remain open; both rows remain UNREVIEWED.

## 2026-11-05 — PK46.04 card-composition parser cross-check

Confirmed pinned `0x017A` and `0x017C` trading gates, inventory-index conversion/bounds, and helper dispatch. Stale selection, item eligibility/results, and UI replay remain open; both rows remain UNREVIEWED.

## 2026-11-05 — PK46.04 guild communication/alliance parser cross-check

Confirmed `0x017E` shared chat validation/routing and `0x0180`/`0x0183` guild-master/map/target gates before alliance helper dispatch. No current senders were found; owner support review remains open and all three rows remain UNREVIEWED.

## 2026-11-05 — PK46.04 exit/production/reset parser boundary cross-check

Confirmed `0x018A` logout refusal gates, `0x018E` selected-menu-skill allowlist, `0x0197` version-gated reset atcommands, and permission-gated `0x0198` map-cell mutation. Preserve owner/profile review and UNREVIEWED status for all rows.

## 2026-11-05 — PK46.04 local-broadcast/pet parser cross-check

Confirmed `0x019C` local-broadcast atcommand and `0x019D` hide atcommand routes, plus `0x019F`/`0x01A1`/`0x01A5` pet-helper delegation. No current senders were found; preserve PK23/PK47/PK27 ownership and UNREVIEWED statuses.

## 2026-11-05 — PK46.04 pet selection/emotion dispatch cross-check

Confirmed `0x01A7` taming-menu gates and egg-index dispatch/clear; `0x01A9` only dispatches pet emotion when a pet exists. Current player-emotion send path is distinct; preserve PK47 ownership and UNREVIEWED status.

## 2026-11-05 — PK46.04 arrow-selection menu-skill cross-check

Confirmed pinned `0x01AE` trading failure/clear behavior and menu-skill-dependent arrow/creation routing. Profile and zero/cancel semantics remain open; the row remains UNREVIEWED.

## 2026-11-05 — PK46.04 open-vending parser gate cross-check

Confirmed pinned `0x01B2` flag-0 path clears prevend/WIP before map/chat and title gates; empty title returns before vending helper. Open/failure result and UI recovery replay remain open; the row remains UNREVIEWED.

## 2026-11-05 — PK46.04 GM-shift registration cross-check

Confirmed both `0x01BA`/`0x01BB` registrations route through the same fixed-name `@jumpto` atcommand handler. No client sender was found; preserve PK47 ownership and UNREVIEWED status pending profile/admin review.

## 2026-11-05 — PK46.04 GM-recall registration cross-check

Confirmed both `0x01BC`/`0x01BD` registrations route through the same fixed-name `@recall` atcommand handler. No client sender was found; preserve owner/profile review and both UNREVIEWED statuses.

## 2026-11-05 — PK46.04 AutoSpell/NPC string parser cross-check

Confirmed `0x01CE` menu-skill gating and WIP/selection clearing; `0x01D5` empty-payload rejection, profile-dependent length adjustment, bounded string copy, and script continuation. Feature/profile/codepage and UI replay remain open; rows remain UNREVIEWED.

## 2026-11-05 — PK46.04 novice/alternate party-create parser cross-check

Confirmed `0x01E7` class/rest gating for doridori and `0x01E8` passes request pickup/share options after party-lock/skill/name gates, unlike `0x00F9`. Preserve PK47/PK15 ownership and UNREVIEWED status pending feature/profile review.

## 2026-11-05 — PK46.04 novice/adoption parser cross-check

Confirmed `0x01ED` Super Novice 10%-exp condition and `0x01F7`/`0x01F9` adoption session/invite dispatch. Adoption eligibility/consent lifecycle remains complex for PK24/PK30/PK50 review; preserve all three UNREVIEWED rows.

## 2026-11-05 — PK46.04 repair/friend parser cross-check

Confirmed `0x01FD` profile-selected layout and repair-menu/trading gates; `0x0202` online-name and reciprocal invite-state checks; `0x0203` AID/CID match and reciprocal online removal. Preserve PK38/PK16 ownership; profile, persistence, encoding, and replay remain open.

## 2026-11-05 — PK46.04 GM/PVP parser cross-check

Confirmed `0x0212` fixed-name mute command, `0x0213` permission/name/group-level checks, and `0x020F` requester-only PVP info response. No current senders were found; preserve PK47/PK39 ownership and UNREVIEWED status.

## 2026-11-05 — PK46.04 ranking request profile cross-check

Confirmed four ranking registrations call `clif_ranklist` under differing server `PACKETVER` minimums. Slash-command comments exist, but no current sender was found; effective profile/feature applicability remains open and rows remain UNREVIEWED.

## 2026-11-05 — PK46.04 effect/refine request cross-check

Confirmed `0x021D` less-effect session-state update and `0x0222` weapon-refine menu-skill/trading/index route. No current senders were found; preserve feature/profile review and UNREVIEWED status.

## 2026-11-05 — PK46.04 chat/storage/homunculus parser cross-check

Confirmed `0x00F3` validated chat/channel routing, `0x0193` state-based storage close, and homunculus/mercenary ownership checks before control actions. No current homunculus senders were found; keep feature review and rows UNREVIEWED.

## 2026-11-05 — PK46.04 mail-window request state-mutation boundary

Confirmed selected old-PACKETVER `0x0246` type 0 removes both draft item and zeny rather than simply opening a window. Keep PK32 ownership and UNREVIEWED status pending stronger mail lifecycle review.

## 2026-11-05 — PK46.04 mail-send gate cross-check

Confirmed selected old-PACKETVER `0x0248` requires char-service connectivity and minimum 69-byte framing before `mail_send`. Service delivery, attachments/zeny, and duplicate-result replay remain open; PK32 ownership and UNREVIEWED status remain.

## 2026-11-05 — PK46.04 auction/cooking/effect review boundary

Flagged the multi-stage auction escrow/bid/settlement family for PK50/stronger domain review; no current sender was found and no rows were closed. Cooking dispatch is profile-gated; cooking/feeling support remains unresolved and UNREVIEWED.

## 2026-11-05 — PK46.04 newer-party/service stronger-review gate

Flagged newer-party and associated service-only registrations with bounded no-sender results for profile/async lifecycle review; preserved owners and UNREVIEWED rows.

## 2026-11-05 — PK46.04 player-equipment view parser cross-check

Confirmed `0x02D6` online/same-map target, show-equipment/permission, and refusal branches; production frame replay exists but rendered equipment remains open. The row remains UNREVIEWED.

## 2026-11-05 — PK46.04 mail-return profile cross-check

Confirmed old-profile `0x0273` forwards only a matching sent-mail inbox item to char service; newer profile branches return unsupported. No sender was found; preserve PK32 ownership and UNREVIEWED status pending service review.

## 2026-11-05 — PK46.04 revive/mercenary parser cross-check

Confirmed `0x0292` delegates to item-based revive and `0x029F` only deletes an active mercenary for option 2. No current senders were found; preserve PK47/PK29 ownership and UNREVIEWED status.

## 2026-11-05 — PK46.04 walk parser gate cross-check

Confirmed pinned `0x00A7` dead/progressbar/action/run gates and configured packed-coordinate decode. Default PacketVer23 replay does not establish deployed-profile or live movement acceptance; the row remains UNREVIEWED.

## 2026-11-05 — PK46.04 targeted-skill profile boundary

Confirmed configured server-field dispatch for `0x0072`/`0x0438`; client PacketVer22/23 layouts differ, and `0x0072` collides with another profile's zone-entry request. Deployed profile and skill result review remain open; rows stay UNREVIEWED.

## 2026-11-05 — PK46.04 time-sync/ground-skill profile collision

Confirmed legacy client tick uses `0x007E` while selected server binds `0x007E` to a 102-byte ground-skill parser; newer client tick `0x0089` aligns. Reconcile deployed profile with PK08/strong reviewer before parity claims; rows remain UNREVIEWED.

## 2026-11-05 — PK46.04 change-direction parser cross-check

Confirmed pinned `0x0085` field mapping to `pc_setdir` and nearby direction broadcast; PacketVer22/23 sender capture matches. Deployed profile/live UI acceptance remain open; the row remains UNREVIEWED.

## 2026-11-05 — PK46.04 time/name request profile cross-check

Confirmed `0x0089` client-tick storage/time response and modern `0x008C` target-name lookup; legacy `0x008C` means global chat while `0x0094` is name request. Runtime response/profile review remains open and rows remain UNREVIEWED.

## 2026-11-05 — PK46.04 storage/name opcode collision

Confirmed PacketVer23 uses `0x0094` for inventory-to-storage while selected server binds it to `MoveToKafra`; a legacy client profile also uses it for name lookup. Reconcile effective profile with PK08/strong reviewer; affected rows remain UNREVIEWED.

## 2026-11-05 — PK46.04 zone-entry parser gate cross-check

Confirmed selected `0x009B` zone parser rejects already-logged sessions and reads account/character/auth token/tick fields by configured positions. Effective zone profile and end-to-end authentication replay remain open; the row remains UNREVIEWED.

## 2026-11-05 — PK46.04 item-use profile boundary

Confirmed `0x009F` item-use dead/busy/index/failure-ack gates and legacy collisions: `0x00A2` use-item vs selected name lookup, plus `0x009F` legacy take-item vs selected use-item. PacketVer22/23 use different opcodes/layouts. Reconcile profile with PK08 and replay results before closure; rows remain UNREVIEWED.

## 2026-11-05 — PK46.04 take/storage/ground-skill parser cross-check

Confirmed `0x00F5` same-map floor-item and mandatory failure-ack path; `0x00F7` storage-flag/index conversion; `0x0113` action/standing gates before ground-skill dispatch. Legacy profile and result/cache ordering remain open; rows remain UNREVIEWED.

## 2026-11-05 — PK46.04 drop/action/cash-shop/hotkey parser cross-check

Confirmed `0x0116` drop gates/index conversion, `0x0190` action-field delegation, `0x0288` NPC-shop/trading gates with versioned cash-shop layout, and `0x02BA` hotkey feature/version/index gates. Preserve PK38/PK08/PK40/PK47 owners; result/persistence/profile replay remains open.


## 2026-11-05 — PK46.04 quest, battle-chat, account-name, and progress-bar parser cross-check

Historical server source shows `0x02B6` forwards quest ID/active state to `quest_update_status`; `0x02DB` validates message processing, applies atcommand/no-chat/minimum-delay gates, then broadcasts and echoes; `0x01DF` forwards an account ID to account-name lookup with authorization left as a TODO; and `0x02F1` clears progress-bar state before script continuation. Client senders were not found in the bounded search. These rows remain UNREVIEWED pending selected-profile, owner, and runtime checks.


## 2026-11-05 — PK46.04 selected-source verification supersedes historical parser note

Correction: bundled eAthena is not an authority for this plan. Reopened selected rAthena and verified SHA-256 for `clif.cpp`, `clif_packetdb.hpp`, and `packets.hpp` against `source-baseline.json`. Selected-source findings: `0x02B6` dispatches quest state; `0x02DB` validates/forwards formatted battleground chat; `0x01DF` performs account-name lookup with authorization still an explicit TODO; `0x02F1` ignores inactive progress-bar requests and otherwise clears state before script continuation; `0x02D8` applies configuration types with pet/homunculus ownership and feature gates. The server Git revision is still unavailable, client senders were not found, and affected inventory rows remain UNREVIEWED.


## 2026-11-05 — PK46.04 memorial-dungeon and party-booking parser boundary

Pinned `clif.cpp` confirms `0x02CF` trade/dead/instance gates and force-destroy delegation; `0x0802` delegates registered level/map/job fields; `0x0806` acknowledges when deletion helper returns true. Client senders were not found. `party.cpp` is absent from the source hash manifest; its exploratory delete helper appears to return true without a record, so this is flagged for stronger pinned owner review rather than treated as a defect. Keep rows UNREVIEWED pending helper lifecycle, profile, results, and replay evidence.


## 2026-11-05 — PK46.04 memorial command layout and client-symbol recheck

`0x02CF` is fixed 6 bytes with packed 16-bit type and 32-bit command; its selected parser handles the force-destroy command. Exact request-symbol and opcode searches in the three client source areas returned no matches for memorial and party-booking requests. Enum/helper source provenance remains incomplete, so rows stay UNREVIEWED.


## 2026-11-05 — PK46.04 pinned party-booking and instance helper paths

Added exact hashes for selected `party.cpp`, `instance.cpp`, and `clif.hpp` to the bounded evidence. Party booking creates/rejects duplicates and deletion is idempotent at the parser acknowledgment boundary (success even when absent); instance destruction checks character/party-leader/guild-master ownership and DB destroyable status. No client senders were found. The prior helper-unpinned caveat is superseded; inventory rows remain UNREVIEWED pending client profile/UI and replay evidence.


## 2026-11-05 — PK46.04 chat-room request size parity

Selected server parser lengths for `0x00DE/0x00E0/0x00E2/0x00E3` match the client size table (variable/30/26/2 bytes). Server parsers delegate status, owner, kick and leave behavior; no client function callers were found, and the opcode matches in `GronPacket.cpp` are size declarations only. PK24/PK31 authorization/UI/encoding/replay review remains; rows stay UNREVIEWED.


## 2026-11-05 — PK46.04 homunculus/mercenary movement and attack parser cross-check

Pinned `clif.cpp` confirms fixed 9-byte `0x0232` checks the supplied entity against the requester's active homunculus/mercenary and rejects negative/current coordinates; fixed 11-byte `0x0233` validates the same ownership and forwards target/action to attack. Movement is enabled by the selected PACKETVER. No client senders were found; keep rows UNREVIEWED pending sender/UI/replay and owner evidence.


## 2026-11-05 — PK46.04 homunculus/storage-password/mail parser cross-check

Pinned parser evidence: `0x0231` delegates rename; `0x023B` is currently a no-op; on selected legacy profile `0x023F` gates inbox refresh and delegates to char service, while `0x0241` validates positive ID/operation and mail-reader inbox membership. Client senders were not found. Keep affected rows UNREVIEWED pending owner eligibility, mail service/replay and UI evidence.


## 2026-11-05 — PK46.04 legacy mail mutation gates

Pinned parser checks for `0x0243/0x0244/0x0247`: delete/get-attachment require connected char service, positive mail ID, allowed operation and inbox membership; deletion diverts messages with items/zeny, and otherwise updates unread state only after successful service request. Staging validates amount, converted inventory index/item and mail operation before helper dispatch. No client senders were found. PK32 owner review is needed for escrow/async/replay behavior; rows remain UNREVIEWED.


## 2026-11-05 — PK46.04 bounded pass and auction deferral

Cross-joined the 177 request inventory with the current self-review opcode mentions: five remaining unmentioned operations are `0x024C/0x024D/0x025D/0x024E/0x0251`, all primary-owned by PK40. Corrected an earlier misreferral to PK50 (quest journal); defer auction escrow/search/cancel/replay review to PK40. Continue PK46.03 next while PK46.04 remains IN_PROGRESS; no inventory classifications were closed.


## 2026-11-05 — PK46.03 invalid character-index production replay rerun

Confirmed deployment OFF and reran the focused packet replay. The sandbox attempt failed before the case due loopback denial; the authorized local-mock rerun passed 1/1 in 4.47s. `login_select_character_reject_invalid_index` asserts selection rejection, unchanged next submode, and zero emitted packet. This confirms that one gate only; other variants and the 0x0068/0x01FB delete mismatch remain open.


The same PK46.03 replay run passed `login_account_request`, `login_select_character_request`, `login_create_character_request`, and `login_accept_make_character` against their named PK44 fixtures. Other login variants and unsupported actions remain open.


## 2026-11-05 — PK46.03 bounded pass checkpoint

Fresh focused replay confirms invalid selection rejection and supported `0x0064/0x0066/0x0067` fixture paths. Leave PK46.03 IN_PROGRESS for destructive delete profile mismatch, SSO/security review, unsupported rename/scheduled-delete/paged-selection variants, and PK44/PK48-owned `0x08D4` safety. Move active work to PK46.02 per the user direction to defer complex paths and continue.


## 2026-11-05 — PK46.02 replay case inventory snapshot

Saved a compact replay report at `evidence/PK46.02-production-replay-2026-11-05.md`: 47 PK46.02 case records passed in the current 1/1 CTest run; 35 unique inventory opcodes match fixture names. The report preserves case, fixture, outcome, length, and captured bytes while explicitly treating name matches as a discovery aid only. All 177 request classifications remain UNREVIEWED.


## 2026-11-05 — PK46.02 corrected 0x0085 evidence state

Aligned C2S-0085 inventory evidence with its detailed note and replay snapshot: PacketVer22/23 request tuple matches the selected final registry and production replay passes; deployed-profile/live-UI acceptance remains open. Classification stays UNREVIEWED.


## 2026-11-05 — PK46.02 outbound replay evidence mapped to inventory

Refined the replay snapshot: 22 passing PK46.02 records captured outbound packet bytes for 22 distinct inventory opcodes. Added the snapshot reference to those rows only; all classifications remain UNREVIEWED. The broader 35-opcode fixture-name match includes cases without outbound captures and is not treated as sender evidence.


## 2026-11-05 — PK46.02 corrected 0x00A7 evidence scope

Aligned C2S-00A7 with its detailed note and replay snapshot: PacketVer22/23 wire tuple matches the selected server registry; PacketVer23 production capture passes. PacketVer22 capture and deployed-profile/live-UI acceptance remain open. Classification stays UNREVIEWED.


## 2026-11-05 — correction: PK46.02 replay assertion evidence

The prior note understated 14 matched cases that lacked a `captured_packet_hex` field. The replay log includes assertion text for 12 of them; those assertions describe send/frame emission and are preserved in the corrected replay report. Combined with 22 explicit-byte cases, 33 distinct inventory opcodes have direct outbound replay evidence. Two other fixture-name matches have no logged bytes or send assertion and remain discovery-only. Added report references to rows with direct evidence; all classifications remain UNREVIEWED.


## 2026-11-05 — PK46.02 status-increase success UI refresh path

Traced `0x00BC` success through selected-character stat mutation and `InvalidateStatusWindows` to `UIStatusWnd` rebuilding/drawing those same stat fields. Updated C2S-00BB evidence state; classification remains UNREVIEWED. Existing replay covers only the outbound request, leaving refusal/cost/repeated-click replay and visible runtime rendering open.


## 2026-11-05 — PK46.02 NPC contact type-byte boundary

Clarified C2S-0090 evidence: the client always sends contact Type 1; the selected server parser ignores that byte and dispatches using NPC ID. Production outbound capture passes. PK25 owns contact result/rejection/timeout and visible transition; classification remains UNREVIEWED.


## 2026-11-05 — PK46.02 NPC-next UI failure boundary

Documented `0x00B9` keyboard/mouse behavior: both clear the dialog action before send; keyboard propagates send result while mouse ignores it. `RequestNpcNext` returns the packet-send result. Updated the row evidence state; failure recovery, retry, stale ID, script timeout and response UI remain PK25 review questions, not confirmed defects.


## 2026-11-05 — PK46.02 NPC menu send-failure state

For `0x00B8`, the UI hides/closes before send; GameMode clears quest fields only after a successful close choice. The UI ignores the result. Updated the inventory evidence state; do not call this a defect without PK25 review of retry/reopen/script behavior. Classification remains UNREVIEWED.


## 2026-11-05 — PK46.02 emotion request owner deferral

C2S-00BF source note exposes client emotion IDs 0-80 versus selected server enum 0-87 and a time-boundary throttle needing replay; no emotion case appears in the current production replay log. Assigned the unresolved enum/animation/replay review explicitly to PK47; classification remains UNREVIEWED.


## 2026-11-05 — PK46.02 user-count query boundary

C2S-00C1 is a two-byte no-payload request; selected server checks the session and responds with current user count. No current client action/constructor was found. Updated the evidence state with the PK47 owner boundary; classification remains UNREVIEWED.


## 2026-11-05 — PK46.02 NPC shop deal-type sender boundary

C2S-00C5 client sender requires a nonzero shop NPC and deal type 0/1, emits the seven-byte selected-profile frame, and returns send success. The current replay log contains no shop-deal case. PK25 owner review remains for shop identity/state, invalid types, repeated switching, and response ordering; row stays UNREVIEWED.


## 2026-11-05 — PK46.02 NPC purchase transaction deferral

C2S-00C8 has no production purchase-list replay case in the current log. Duplicate limited-stock rows cross stock/zeny mutation and reply ordering, so PK25 must reproduce and review atomic transaction behavior before repair. Updated the inventory owner boundary; row remains UNREVIEWED.


## 2026-11-05 — PK46.02 NPC sell transaction deferral

C2S-00C9 has no production sell-list replay case. Repeated-row validation/mutation and result ordering need PK38 production-path review before repair; updated the inventory owner boundary and kept the row UNREVIEWED.

## 2026-11-05 - PK46.02 ignore-all request owner boundary

Updated the 0x00D0 inventory evidence state to reflect the selected-server immediate ignore-state mutation and 0x00D2 result trace already documented in PK46.02-map-private-message-ignore-requests.md. No client sender or supported client flow is established; PK16 retains support, persistence and client-list review. Classification remains UNREVIEWED.

## 2026-11-05 - PK46.02 ignore-list request owner boundary

Updated the 0x00D3 inventory evidence state to reflect the selected-server per-name list reply trace in PK46.02-map-private-message-ignore-requests.md. No client sender or supported UI flow is established; PK16 retains support, reachability and persistence review. Classification remains UNREVIEWED.

## 2026-11-05 - PK46.02 chat-room request family boundary

Aligned the six registered chat-room request rows (0x00D5, 0x00D9, 0x00DE, 0x00E0, 0x00E2, 0x00E3) with PK46.02-unimplemented-chat-room-request-paths.md: client GameMode explicitly leaves chat-room UI/interaction unwired and hasChatRoomContext false. PK24/PK31 still own support/negative-scope review; all rows remain UNREVIEWED.

## 2026-11-05 - PK46.02 trade lifecycle owner gate

Deferred the coupled 0x00E4/0x00E6/0x00E8/0x00EB/0x00ED/0x00EF trade lifecycle from this crosswalk pass. Reliable acceptance requires PK37/PK50 review of invite timeout, cancellation ordering, delayed/duplicate lock, item/currency races and commit rollback across multiple requests. Existing request traces and replay emission evidence remain pointers only; all rows remain UNREVIEWED pending transaction-level replay and owner review.

## 2026-11-05 - PK46.02 skill-up evidence boundary

Refined the 0x0112 inventory state to match PK46.02-map-skill-up-request.md: PacketVer23 production replay captures the four-byte skill-up sender. UI eligibility/repeated-click behavior, selected-server point consumption/refusal, and 0x010F skill-list apply ordering remain PK23 review gates; the row remains UNREVIEWED.

## 2026-11-05 - PK46.02 warp-skill request evidence boundary

Refined the 0x011B inventory state to match PK46.02-map-warp-skill-selection-request.md: production replay covers its 20-byte sender, invalid-destination handling, and preservation of pending selection on send failure. Selected-server skill-state authorization and teleport outcome remain PK23 review gates; classification stays UNREVIEWED.

## 2026-11-05 - PK46.02 cart-off evidence boundary

Refined 0x012A inventory evidence to match PK46.02-map-cart-off-request-012A.md: PacketVer23 production sender replay passed. Cart/mount-state UI transition, send-failure feedback, and deployed-server NEW_CARTS configuration remain open; classification stays UNREVIEWED.

## 2026-11-05 - PK46.02 vending-list reachability gate

Updated the 0x0130 inventory state to reflect the existing request/0x0133 response trace and its unresolved PK37 review: standard UI reachability is unproven, and the selected parser has no proximity gate. Staleness/privacy behavior still needs replay. This is a source-trace boundary, not a security finding or closure; classification remains UNREVIEWED.

## 2026-11-05 - PK46.02 vending-purchase evidence boundary

Refined the 0x0134 inventory state to match PK46.02-map-vending-purchase-request.md: production replay covers missing-connection and over-stock client guards. Transaction mutation/results, silent returns, duplicate/out-of-order replies and window clearing require PK38 review. Classification remains UNREVIEWED.

## 2026-11-05 - PK46.02 NPC numeric-input evidence boundary

Updated 0x0143 inventory state to distinguish the passing production ten-byte sender capture from unverified conversion boundaries. Signed/unsigned edge cases, stale NPC IDs, dialog timeout/cancel and following script behavior remain PK25 review gates. Classification remains UNREVIEWED.

## 2026-11-05 - PK46.02 NPC close evidence boundary

Updated 0x0146 inventory state to match PK46.02-map-npc-dialog-close-request.md: production six-byte sender capture passed, and GameMode clears quest/NPC identifiers before send. Close versus STOP behavior, stale IDs, send failure, timeout and dialog recovery remain PK25 owner replay gates. Classification remains UNREVIEWED.

## 2026-11-05 - PK46.02 guild refresh request boundary

Aligned 0x014D and 0x014F inventory states with PK46.02-map-guild-info-request.md. Production refresh replay confirms the 2-byte permission request precedes the no-guild return; with a guild ID, five 6-byte subtype requests are emitted. Partial-send handling, response ordering/duplicates, missing guild data and rendered UI reset remain open under PK12/PK50 ownership. Both rows remain UNREVIEWED.

## 2026-11-05 - PK46.02 guild-emblem owner deferral

Deferred closure of 0x0151 to PK50 owner review. Its two independent send paths share a per-guild throttle, while acceptance depends on cache hit/miss, send failure, unknown/empty emblems, malformed compressed data, guild switches and response/cache replacement. A source trace alone cannot reliably establish this cross-caller lifecycle. Row remains UNREVIEWED.

## 2026-11-05 - PK46.02 guild-position trust boundary

Refined 0x0155 inventory state to reflect the selected map master-flag gate, partial-entry flooring, and inter-server forwarding of guild/member identifiers. Downstream identity authorization and malformed-length behavior need PK12 owner review; this source trace is not an exploitability finding. Classification remains UNREVIEWED.

## 2026-11-05 - PK46.02 guild-leave evidence boundary

Refined 0x0159 inventory state to capture the confirmed non-master 54-byte client request and selected map-side identity/restriction checks. Reason encoding boundaries, service completion/result ordering and post-leave UI state remain PK12 replay gates; classification stays UNREVIEWED.

## 2026-11-05 - PK46.02 guild-position metadata boundary

Refined 0x0161 inventory evidence to match PK46.02-map-guild-position-info-request.md: the 44-byte request and character-service 0x303B update layout are source-traced. UI/server master-permission agreement, numeric/name boundaries, send failure and async guild-info refresh ordering remain PK12 review. Classification remains UNREVIEWED.

## 2026-11-05 - PK46.02 guild-invite evidence boundary

Refined 0x0168 inventory state: production 14-byte invite sender capture passed and selected invite/reply gates are source-traced. Permission and target boundaries, invite correlation, acceptance/rejection replay and resulting membership completion remain PK12 review; row remains UNREVIEWED.

## 2026-11-05 - PK46.02 guild-invite reply correlation gate

Updated 0x016B inventory state to capture the selected parser behavior: the reply contains guild ID/answer but no unique invitation token; server correlation checks pending guild ID and treats any nonzero answer as acceptance, while client emits 0/1. Stale same-guild reply behavior and intended correlation semantics require PK12 replay/owner review. This is an open question, not a confirmed defect; classification remains UNREVIEWED.

## 2026-11-05 - PK46.02 guild-notice evidence boundary

Refined 0x016E inventory state: fixed 186-byte request, map-side master gate, guild-ID check, legacy language-marker normalization and inter-server forwarding are traced. UTF-8 subject/body boundaries, server delivery and guild-window refresh remain PK12 replay gates. Row remains UNREVIEWED.

## 2026-11-05 - PK46.02 guild communication reachability gate

Refined 0x0170/0x017E inventory evidence from PK46.02-map-guild-chat-alliance-gaps.md. Bounded search found no direct senders; selected parser gates are traced (guild-master/map-lock for alliance requests; speaker-prefix/message handling for guild chat). Generic dispatch and actual UI/chat-mode reachability remain PK12 review. No absence or support claim is closed; rows remain UNREVIEWED.

## 2026-11-05 - PK46.02 alliance reply evidence boundary

Refined 0x0172 inventory evidence: client prompt branch sends a fixed ten-byte frame with prompt ID and answer, and selected parser/alliance decision path are source-traced. Accept/decline, stale/duplicate prompt and cross-server completion ordering need PK12 replay/owner review. Classification remains UNREVIEWED.

## 2026-11-05 - PK46.02 identify-item replay boundary

Updated 0x0178 inventory state to match PK46.02-map-identify-item-request.md: PacketVer23 production replay covers signed cancel index -1, positive index 30000, disconnected rejection and already-identified rejection. Active server identify-skill gate and inventory/result UI behavior remain PK38 review; row remains UNREVIEWED.

## 2026-11-05 - PK46.02 card-composition owner gate

Deferred 0x017A/0x017C closure to PK38 transaction review. The request/result sequence spans card eligibility, four-byte card-list response parsing, slot/index conversion, server insertion validation, seven-byte result parsing and client item-cache update; silent branches and delayed/duplicate results need production-path replay. Source traces alone do not establish state parity; both rows remain UNREVIEWED.

## 2026-11-05 - PK46.02 exit-to-Windows evidence boundary

Refined 0x018A inventory state: production four-byte send replay passed and failed send clears pending disconnect state. Selected server accept/refuse, timeout fallback, menu and disconnect transition remain PK47 review; outbound capture is not end-to-end logout proof. Row remains UNREVIEWED.

## 2026-11-05 - PK46.02 change-cart evidence boundary

Updated 0x01AF inventory state from PK46.02-map-change-cart-request.md: production replay covers fixed four-byte request, valid cart types 1-5, rejects 0/6, and closed-connection failure. UI closes the selector on local send success; selected-server skill/level rejection and cart/effect notification remain PK37 review. Row remains UNREVIEWED.

## 2026-11-05 - PK46.02 vending-open lifecycle owner gate

Deferred 0x01B2 closure to PK37 review. The same opcode carries full shop-open payloads and an empty flag-0 close request; selected server restrictions can return silently before 0x0136, which changes client pending/active state. Open/close races, inventory validation, and response ordering require production lifecycle replay. The 0x012F alternate remains a separate profile/reachability question. Rows remain UNREVIEWED.

## 2026-11-05 - PK46.02 NPC string-input evidence boundary

Updated 0x01D5 inventory state to match PK46.02-map-npc-string-input-request.md: production variable-frame replay captured ASCII `Hi` with NUL and total length. Deployed ACP compatibility, non-ASCII conversion/truncation, send-failure UI recovery and script boundary behavior remain PK25 review; row remains UNREVIEWED.

## 2026-11-05 - PK46.02 alternate party-create applicability gate

Deferred 0x01E8 closure to PK15 profile/reachability review. The selected registry accepts a fixed 28-byte CreateParty2 layout, while the traced current client party-create action uses 0x00F9 with a different 26-byte layout. Determine whether this is supported by any applicable client profile or is only a compatibility registration; no sender or unsupported-scope conclusion is inferred. Row remains UNREVIEWED.

## 2026-11-05 - PK46.02 friend-request evidence boundary

Updated 0x0202 inventory state to match PK46.02-map-add-friend-request.md: production replay captures the fixed 26-byte request with a 23-byte name. Non-ASCII encoding, duplicate/already-friend server outcomes, invite/reply receive behavior and visible UI feedback remain PK16 review; classification remains UNREVIEWED.

## 2026-11-05 - PK46.02 friend-reply evidence boundary

Updated 0x0208 inventory state to match PK46.02-map-friend-reply-request.md: production accept/decline 14-byte sends replay, and closed-connection failure preserves the prompt. Selected parser fields are target AID/CID/answer; stale/mismatched reply, server result, friend-list effects and UI refresh remain PK16 review. Classification stays UNREVIEWED.

## 2026-11-05 - PK46.02 storage-close evidence boundary

Updated 0x0193 inventory state to match PK46.02-map-storage-close-request.md: production replay verifies the open-storage guard (closed emits no bytes; open emits two-byte 0x0193) and selected server dispatch by storage type is traced. Close notification ordering, duplicate requests and visible window lifecycle remain PK47 review; row remains UNREVIEWED.

## 2026-11-05 - PK46.02 global-chat profile boundary

Updated 0x00F3 inventory state to match PK46.02-map-global-chat-request.md: PacketVer22/23 production replay captures valid `Alice : hello`; selected parser length, speaker-prefix and NUL rules are traced. The legacy client profile selects 0x008C, which conflicts with the selected registry mapping, so deployed profile compatibility remains a PK23 gate. Row remains UNREVIEWED.

## 2026-11-05 - PK46.02 mail-window opcode semantic mismatch

Updated 0x0246 inventory state to capture the production zero-filled four-byte request and selected server behavior: for the selected pre-20150513 branch, type 0 removes a draft item/zeny. This does not prove the UI action opens a composer and may be a client/server semantic mismatch requiring PK32 compatibility review. Composer/attachment flow, server denial, cancel/reset and send-failure UX remain open; row remains UNREVIEWED.

## 2026-11-05 - PK46.02 auction-family owner gate

Deferred the mapped auction requests (0x024B/0x024C/0x024D/0x024F/0x0251/0x025D) to PK40. The bounded search found no direct current-client senders; applicability/generic dispatch is unresolved, and registration, item escrow, search, bid, cancel and close behavior form a coupled auction state machine. PK40 review must establish supported scope and transaction-level replay before any classification. Rows remain UNREVIEWED.

## 2026-11-05 - PK46.02 mail-send evidence boundary

Updated 0x0248 inventory state to match PK46.02-map-mail-send-request.md: production variable-frame sender and client result-routing/local-state replay passed. This does not verify server/char-server delivery, persistence, attachment/zeny refund or retry behavior; those remain PK32 review. Row remains UNREVIEWED.

## 2026-11-05 - PK46.02 auction owner-gate correction

Correction to the earlier auction-family diary entry: the full PK40 auction set in PK46.02-map-auction-feeling-cooking-gaps.md is 0x024B/0x024C/0x024D/0x024E/0x024F/0x0251/0x025C/0x025D. The same note also lists 0x0254 (PK47 feeling) and 0x025B (PK38 cooking); those are not PK40 auction rows and retain their own owner review. All remain UNREVIEWED.

## 2026-11-05 - PK46.02 skill-target profile applicability gate

Deferred the 0x0072/0x0438 targeted-skill family to PK23 review. PacketVer22 sends a 25-byte skill-target request on 0x0072; PacketVer23 uses 0x0438; a separate legacy profile assigns 0x0072 to connection. The selected registry contains distinct mappings. Enumerate all send callsites and confirm the deployed profile before assigning parity; rows remain UNREVIEWED.

## 2026-11-05 - PK46.02 opcode 0x007E profile distinction

Corrected the 0x007E inventory cross-reference to include PK08.02-contract-selection.md. PK08.02 selects 102-byte Profile23 ground-skill text on 0x007E, matching the selected 20080910 server parser; the legacy map profile uses the same number for time sync, while nonlegacy time sync uses 0x0089. Preserve profile separation; retry/deployed-profile parity remains PK08/PK47 review. Row remains UNREVIEWED.

## 2026-11-05 - PK46.02 use-item profile matrix boundary

Updated 0x009F inventory evidence to reflect PK46.02-map-use-item-request.md: PacketVer22 uses 14-byte 0x009F, PacketVer23 uses 8-byte 0x0439, and the explicit legacy profile uses 0x00A2. Selected use acknowledgements include 0x00A8/0x01C8. Production end-to-end item-use/result correlation and deployed profile review remain PK38; row stays UNREVIEWED.

## 2026-11-05 - PK46.02 ground-item profile collision

Updated 0x00F5 inventory state to match PK46.02-map-take-item-request.md: PacketVer22/23 use the selected eight-byte 0x00F5 layout; explicit legacy uses six-byte 0x009F, but selected 0x009F is a 14-byte use-item parser. Pickup-ack and effective-profile parity require PK38 review; row remains UNREVIEWED.

## 2026-11-05 - PK46.02 shortcut-update evidence boundary

Updated 0x02BA inventory state to match PK46.02-map-shortcut-slot-update-request.md: production replay captures the 11-byte slot-5 request. UI mutates local shortcut state before send; multi-slot ordering, server persistence/validation and rollback/retry after send failure remain PK47 review. Row remains UNREVIEWED.

## 2026-11-05 - PK46.02 player-equipment request boundary

Updated 0x02D6 inventory evidence to match PK46.02-map-view-player-equipment-request.md: production context-action replay captures the six-byte target-AID request. Hit-testing, permission/refusal/offline/cross-map results and visible equipment rendering remain PK38 review; row stays UNREVIEWED.

## 2026-11-05 - PK46.02 continuous-attack profile boundary

Updated 0x0437 inventory state to reflect PK46.02-map-action-request.md: PacketVer23 uses seven-byte 0x0437 matching the selected parser; PacketVer22 uses a distinct 19-byte 0x0190 contract. Production click/retry behavior and server attack-state response still need PK50 replay; row remains UNREVIEWED.

## 2026-11-05 - PK46.02 zone-profile owner gate

Deferred 0x0436 closure for stronger profile review. ZoneConnectProfile is selected independently from map gameplay profile; effective 20080910 deployment selection and all supported compact/padded layouts must be reconciled against the selected server registry before parity is claimed. Row remains UNREVIEWED.

## 2026-11-05 - PK46.02 targeted-skill profile boundary

Updated 0x0438 inventory state to match PK46.02-map-skill-target-request-0438.md: PacketVer23 sends a 10-byte request matching the selected parser; common sender rejects zero skill/level/target. PacketVer22 has a separate 25-byte 0x0072 form. Per-skill callsites, effective profile and result behavior remain PK23 review; row stays UNREVIEWED.

## 2026-11-05 - PK46.02 PacketVer23 use-item boundary

Updated 0x0439 inventory state to distinguish its 8-byte PacketVer23 request, which matches the selected parser, from PacketVer22 0x009F. Use acknowledgements/result correlation and deployed profile selection remain PK38 review; row remains UNREVIEWED.

## 2026-11-05 - PK46.02 party-booking owner gate

Deferred 0x0802/0x0806 to PK43 review. The bounded source search found no direct current-client senders; determine whether newer party-booking registration/deletion is supported through another profile or generic dispatch before classifying the selected server registrations. Both rows remain UNREVIEWED.

## 2026-11-05 - PK46.02 deferred gates; PK46.03 resumes

Current inventory check found 177/177 PK46.02 rows classified UNREVIEWED. Remaining work requires primary-owner profile applicability, raw-buffer/UI reachability, or transaction/replay review; no classifications are closed by source summaries. Per the user instruction to document over-complex work and move on, PK46.02 remains IN_PROGRESS and active focus moves to PK46.03. Resume PK46.02 after owner gates become actionable.

## 2026-11-05 - PK14.04 verification resumes with lifecycle gates open

PK05.05 remains deferred for production acknowledgement/transition-signal design; PK14.06 remains deferred for map-A/B, same-map warp, failed move/reconnect and character-selection lifetime coverage. PK46.03 destructive delete/SSO/unsupported-action/0x08D4 and PK46.02/PK46.04 owner gates remain open. Per the plan, current focus moves to PK14.04: refresh integrated build/replay evidence and state exact visual/lifecycle gaps; do not certify completion while these gates remain.

## 2026-11-05 - PK14.04 fresh integrated verification

Deploy-off build succeeded; authorized focused packet replay passed 1/1 in 4.53s. PK14.05 roster replacement and PK14.06 production OnExit marker cases passed. Exact assertions, replay binary hash/time, sandbox denial, and remaining map/warp/reconnect/character-selection plus visible native/Qt gates are recorded in evidence/PK14.04-verification-boundary.md. PK14.04 remains IN_PROGRESS.

## 2026-11-05 - PK14.04 bounded verification recorded; PK05.05 active

PK14.04 fresh deploy-off build and focused replay passed; PK14.05 roster replacement and PK14.06 OnExit marker assertions passed. Visual native/Qt behavior and map-A/B, same-map warp, failed move/reconnect and character-selection transitions remain open, so PK14.04 stays IN_PROGRESS. Following the plan, active focus moves to PK05.05 R1; PK14.06 and packet crosswalks remain open under their gates.

## 2026-11-05 - PK05.05 transition gate retained; PK14.06 active

PK05.05 remains deferred: accepted return/exit acknowledgements can disconnect and queue mode changes while the same packet loop may continue into receive failure; safe R1 handling requires an explicit production-used transition signal and caller-boundary replay. Per the plan, focus moves to PK14.06 R3 marker lifetime, with PK14.04 visual/integration gates still open.

## 2026-11-05 - PK14.06 source-path recheck; PK14.04 resumes

Focused inspection confirmed that `HandleMapChange` calls `ApplyMapChangeSessionState` before the same-map reuse or server-move reconnect branch; the helper invalidates party positions before changing map and position. `CLoginMode::OnExit` and `CGameMode::OnInit` also invalidate coordinates. This confirms source ordering, not production behavior: no replay exercises map-A/B, same-map reuse, reconnect failure, or character-selection change. PK14.06 stays IN_PROGRESS with that transition matrix deferred for production-caller replay design/review. Per HANDOFF, active focus moves to PK14.04; its visible UI and lifecycle gates remain open.

## 2026-11-05 - PK14.04 bounded integration review; PK05.05 resumes

PK14.04's fresh deploy-off build and production-framed replay are recorded in `evidence/PK14.04-verification-boundary.md`. Remaining acceptance requires rendered native/Qt movement, removal and layering evidence plus production map-A/B, same-map warp, failed reconnect and character-selection transitions. The current replay does not provide those visible client or transition fixtures; helper-only tests would not meet the work item. Keep PK14.04 IN_PROGRESS and advance active focus to PK05.05, the next handoff item.


## 2026-11-05 - PK05.05 source-path gate; PK46.03 resumes

Focused source review confirmed that `RecvPacket(false)` becomes `receiveDrained`; `OnUpdate` only stops early if the pending-disconnect timeout fires, otherwise world processing continues. No loop-result signal records that packet dispatch accepted a return/exit acknowledgement. Implementing a safe signal requires coordinated dispatch, pending-action, framing/error and teardown semantics with production-caller replay, so keep PK05.05 IN_PROGRESS and defer source changes to focused design/review. Active focus advances to PK46.03 per HANDOFF.


## 2026-11-05 - PK46.03 fresh focused replay checkpoint

The current `packet_stream_replay_checks` passed 1/1 in 4.59 seconds after the sandbox denied its first local-loopback attempt and the identical authorized rerun. The log confirms PASS for invalid character-index rejection and the exercised legacy login/account/select/create/make-character paths. Binary: 20,088,420 bytes, SHA-256 `bbbd39e6d6a95365bcf45002153e1af36cf4a92e40903ea6abbaa5d8619cb49b`. PK46.03 remains IN_PROGRESS: delete-layout, SSO/security, unsupported actions and PK44/PK48-owned `0x08D4` remain unverified/owner-gated.


## 2026-11-05 - PK46.02 PacketVer23 use-item production send

Added a replay through `RequestUseInventoryItem` and verified the selected PacketVer23 frame `39 04 E7 FD 44 33 22 11` (item index 64999, target AID `0x11223344`). Deploy-off app and replay targets built; focused CTest passed 1/1 in 4.68s. Binary SHA-256 `bc232175d85894058ca5b2e9f00448885683ea3dc8f913fb3369ea32351529ac`. Updated the 0x0439 evidence state but kept its classification UNREVIEWED: acknowledgement/result behavior and explicit legacy/deployed profile applicability remain with PK38. PK46.02 is the active continuation target; all 177 rows remain open.


## 2026-11-05 - PK46.02 targeted-skill production send

Added `skill_target_production_send` through `SendUseSkillToIdPacket`; rebuilt deploy-off app/replay targets and the focused CTest passed 1/1 in 4.53s. Captured the exact PacketVer23 0x0438 frame `38 04 07 00 34 12 88 77 66 55`. Binary SHA-256 `bd8513073a20c88910b77fb6ba1ebdbf45ae5e081e72bac3792c1559f8d033c7`. Updated PK23-owned evidence and left classification UNREVIEWED; profile applicability and response parity remain open.


## 2026-11-05 - PK46.02 ground-item take production send

Added `take_item_production_send` through `SendTakeItemRequestPacket`; rebuilt deploy-off app/replay targets and the focused CTest passed 1/1 in 4.67s. It verified zero-ID and closed-connection rejection and captured `F5 00 00 00 44 33 22 11` for the PacketVer23 0x00F5 request. Binary SHA-256 `c755513c0593ddd2d2d65ee8ec8a8fb4bb605871d0af895f4fbc49da249dd89e`. Updated the PK38-owned evidence; inventory row remains UNREVIEWED for acknowledgement/cache and profile gates.


## 2026-11-05 - PK46.02 bounded sender pass; PK46.03 resumes

Three low-risk production sends now have focused replay coverage: PacketVer23 `0x00F5`, `0x0438`, and `0x0439`. The combined deploy-off build succeeded and local-loopback CTest passed 1/1 in 4.67s; combined binary SHA-256 `c755513c0593ddd2d2d65ee8ec8a8fb4bb605871d0af895f4fbc49da249dd89e`. No inventory classifications were closed. All 177 rows remain UNREVIEWED; `0x0437` needs PK50-owned production click/retry and attack-state coverage, and other rows still need their primary-owner profile/UI/raw-buffer evidence. PK46.02 remains IN_PROGRESS; active focus advances to the next planned review, PK46.03.


## 2026-11-05 - PK46.03 owner gates retained; PK46.02 resumes

Rechecked the current PK46.03 crosswalk: its fresh replay covers legacy account/login/select/create/make-character cases only. The selected-profile delete request differs in opcode and size from the current client sender and needs a version-specific contract plus independent destructive-request fixture; SSO requires credential-security review; unsupported actions lack proven current-client wire identities; `0x08D4` remains PK44/PK48-owned. Keep PK46.03 IN_PROGRESS and do not broaden-patch these paths. Active focus returns to PK46.02, whose 177 classifications remain UNREVIEWED despite three new outbound sender replays.


## 2026-11-05 - PK46.02 parser-match subset owner gate; PK46.03 next

The exact selected-parser-match subset in the PK46.02 inventory now has passing production send replays for 0x00F5, 0x0438 and 0x0439. The remaining 0x0437 action request requires the PK50-owned production click/retry and attack-state path, so no helper-only substitute or classification change is made. All 177 inventory rows remain UNREVIEWED pending their primary owners. Continue with PK46.03's separately recorded login/character owner gates.


## 2026-11-05 - PK46.03 combined login replay recheck; PK46.02 next

Rechecked the current combined replay log: 26 distinct `login_*` cases all PASS (account login, character-server entry, select/create and invalid-index rejection included); the suite passed 1/1 in 4.67s with the deploy-off combined executable. This does not cover destructive delete, SSO/security, unsupported current-client requests, or PK44/PK48-owned `0x08D4`. Keep PK46.03 IN_PROGRESS and continue to PK46.02 per HANDOFF.


## 2026-11-05 - PK08.04 transport blocker narrowed

Focused review of the current replay log found `ground_skill_text_request_send_capture` already drives the production `CGameMode::SendSkillToGroundWithText` route through `CRagConnection`, checks invalid-input rejection, captures the independent 102-byte fixture, and verifies no trailing bytes. Updated the stale PK08.04 evidence/work item and BLOCKED entry: transport is covered; only current UI reachability remains open because source contains a declaration and definition but no caller. PK08.04 stays BLOCKED and no discovery row is closed. Active continuation remains PK46.02.


## 2026-11-05 - PK08.04 production send-result verification

Strengthened `ground_skill_text_request_send_capture` with an explicit PacketVer23/0x007E active-profile assertion and a valid-request/closed-connection failure assertion. The deploy-off build of `open_midgard` and `packet_stream_replay_checks` succeeded; focused CTest passed 1/1 in 4.67s. Current replay binary is 20,094,884 bytes, SHA-256 `923ca42bf2c39422d1ea00995256e504ce5c7c7a4ecdf47a16192b9726e618ae`, last write `2026-11-05T09:25:58.282621Z`. Transport acceptance is now covered; PK08.04 remains BLOCKED only on the absent current UI caller/reachability path.


## 2026-11-05 — second primary-agent review and stronger-deferral adjudication

Reviewed HEAD 9aa752ab plus dirty runtime/replay work. Current deploy-off app/replay targets build; authorized local-loopback CTest passed 1/1 in 4.56 seconds with 155 PASS JSON records. Actual-guard source probe confirmed selected inventory/cart, actor/fallback and cast identities. An isolated syntax check of committed Session.cpp failed, proving PK14.05's current passing binary depends on unstaged placement/dependency fixes. Reopened PK14.05; added first-priority PK14.07 reproducible-checkpoint repair. R1 remains unimplemented but now has an explicit accepted-ACK precedence/mode-generation/preamble design; R3 has an approved production-caller seam and incremental offline-state regression. Added R5 timed-cast announcement repair PK23.05 and bounded PK24.05 exact effect/resource authority investigation. The actual enum distinguishes EF_HEAL(312) from 331/332; no selected-live double-heal defect is established.

Completed scoped stronger source/security/model review for profile/01EF mapping, preliminary map echo, SSO/slot-move/Ref-MAC, rename/deletion, cast/emblem, configuration/GAT and quest snapshots. Confirmed two external server defects and prepared bounded fix/test specifications without external changes or hostile network replay. Remaining actor/resource/status/mail/historical/UI work is explicitly not certified. Updated active plan/HANDOFF/STATUS and original deferral notes with the new dispositions; no discovery classifications changed, unrelated dirty work and old evidence/history preserved. Active step PK14.07; order then PK05.05, PK14.06, PK14.04, selected source-ready feature repairs and resume PK46.02. Evidence: REVIEW-2026-11-05.md/.json and SECURITY-REVIEW-2026-11-05.md. This is scoped PK48.02 work, not whole-pass certification. Documentation validation results are recorded after this update. Stable documentation subject: docs(packet): adjudicate second review and reproducible repair gates.

Second-review documentation validation: plan checker PASS (51 packages, 238 steps, 990 discovery owners), git diff --check PASS, and current-summary/card/link checks PASS. Final exact enum crosscheck records EF_HEAL=312 and EF_HPTIME/EF_SPTIME=331/332; PK24.05 is an authority investigation, not an asserted selected-live double-heal repair. No runtime behavior was changed by the reviewer.

## 2026-11-05 — remaining stronger-review gates resolved

Completed the requested stronger source/design work for NPC class/resource lifetime, full ShowEFST branches and shared ownership, timed 0x043F applicability, exact legacy-mail records and pending transactions, and 331/332 effect/resource authority. Added canonical contracts, 22-row branch matrix, eight independent mail wire examples plus sequence requirements, compiled selected-header probe and pinned provenance. Corrected the earlier padded-hex negative search: 0x043f exists but its timed emitter is inactive for selected 20080910; changed only that coverage row. Retain original-executable 331/332 behavior; no selected-live healing defect established. Updated work items and removed generic strong-review waits. PK22.03/PK24.05 are READY_TO_COMMIT source dispositions, other reviewed implementation leaves IN_PROGRESS for design work only. Active PK14.07 priority unchanged. No runtime/server/installed-client edits or commits made. PE/Ref/current effect comparison and independent header probe passed; fixtures are source-derived examples, not production replay. Evidence: STRONG-UNBLOCK-2026-11-05.md/.json. Global PK48 and actual feature/UI acceptance remain open.

Follow-up validation: selected-header probe compiled and passed; PE/Ref/current effect comparison passed; 22 unique ShowEFST branch rows and eight independent mail records checked; pinned sources unchanged; local contract/work-item links resolved; plan checker PASS (51 packages, 238 steps, 990 owners); git diff --check PASS. Recorded unreachable current icon aliases 188/235/264 with exact Ref dispositions and a bounded repair requirement in PK22.04. Runtime fixes remain implementation work; no new production replay or rendered acceptance is claimed.


## 2026-11-05 — PK14.07 candidate separation deferred; PK05.05 resumes

Confirmed the HEAD syntax error and misplaced roster handler/test structure from the second review. The dirty tree has the fixes, but the replay closure also depends on unstaged PK14.06 methods in Session/GameMode/UI and the test file has 1,988 unrelated added lines. A minimal candidate cannot be certified by staging whole files; no staging or commit was done. Recorded the exact dependency and required reviewer decision in `evidence/PK14.07-reproducible-roster-checkpoint.md`. PK14.07 is BLOCKED; active focus moves to PK05.05 per the updated handoff.


## 2026-11-05 — PK05.05 seam deferred; PK14.06 active

Re-read the current plan, handoff, review adjudication and PK05.05 card. Probed the production receive/update path and confirmed the reviewed design spans packet dispatch, mode operations, socket/framing state, pending disconnect intent, durable login status and the OnUpdate world-work boundary. The existing replay harness and its production integration files have broad unrelated uncommitted edits; a reliable acceptance test needs a narrow receive/update seam that drives the same preamble as OnUpdate. A preliminary wiring sketch was fully removed; no runtime edits, build, test, staging or commit were retained. Added [PK05.05 implementation boundary](evidence/PK05.05-implementation-boundary-2026-11-05.md). PK05.05 remains IN_PROGRESS. Resume at PK14.06 per the current continuation order.

Validation: `python tools/check_packet_plan.py` passed before the checkpoint synchronization (51 packages, 238 numbered steps, 990 discovery owners, 21 findings, 19 ignored registrations).


## 2026-11-05 — PK14.06 caller seam deferred; PK14.04 active

Re-read the adjudicated PK14.06 caller replay design and inspected the production `HandleMapChange` and `OnNotifyZonesvr` paths plus the existing marker-exit replay. The current exit replay exercises actual invalidation, but map/warp and zone-server caller replays are absent. The map handler directly controls same-map branch selection, reconnect, loading presentation and mode handoff; a reliable replay needs the reviewed narrow production-used orchestration seam. Added [PK14.06 transition boundary](evidence/PK14.06-transition-boundary-2026-11-05.md). No runtime code, build, test, staging or commit for PK14.06. PK14.06 remains IN_PROGRESS; continue with PK14.04.


## 2026-11-05 — PK14.04 current-tree verification

Built `packet_stream_replay_checks` and `open_midgard` with `build-mingw-preset` (MinGW Release, replay tests ON, developer deploy OFF). Focused CTest's sandbox-default run failed at local `Connect`; the authorized loopback rerun passed 1/1 in 4.59s with 155 JSON records and no failure records. `party_marker_game_mode_exit` passed its production OnExit assertions. Recorded binary hashes/timestamps and the full acceptance boundary in [PK14.04 current-tree verification](evidence/PK14.04-current-tree-verification-2026-11-05.md). This is dirty-tree evidence only: no candidate isolation, actual map-change/warp/zone-server caller replay, or native/Qt rendered evidence. PK14.04 remains IN_PROGRESS.


## 2026-11-05 — PK09.02 source mapping correction; PK09.03 active

Applied the adjudicated profile correction: packetver23 no longer assigns historical normal-cart 0x01EF as an equipment-inventory alias; the commerce registration remains intact. Built `open_midgard` and `packet_stream_replay_checks` with MinGW Release and developer deployment OFF. The authorized loopback CTest passed 1/1 in 4.55s. This does not exercise the four route/state isolation cases, so PK09.02 remains IN_PROGRESS and PK09.03 is active for production-framed route verification. Evidence: [PK09.02 selected-profile correction](evidence/PK09.02-selected-profile-correction-2026-11-05.md).


## 2026-11-05 — PK09.03 route replay passed; candidate commit deferred

Added category-preserving inventory list replacement and a production-framed/router replay for selected IDs 02E8, 02D0, 02E9 and 02D2. It checks both inventory-list orders, both cart categories, malformed trailing data and empty normal list. The replay and app build pass; verbose CTest reports 156 JSON records, zero failures, and the PK09.03 route case. Artifact identities and exact evidence are in [PK09.03 four-route replay](evidence/PK09.03-four-route-replay-2026-11-05.md). The involved Session/packet/replay files contain unrelated pending PK14 changes, so the minimal candidate cannot be isolated for the required separate commit here. PK09.02/.03 remain IN_PROGRESS; continue with PK23.05.


## 2026-11-05 — PK23.05 implementation deferred; PK24.05 active

Reviewed the adjudicated cast-announcement behavior and current `HandleSkillCastAck` branches. The focused acceptance spans timed non-monster announcements, 334/335 target text, 57/62 and monster exclusions, local instant deferral followed by rejection, missing actors, cancellation and repeated/out-of-order acknowledgements. Without exact message-99 parameters and actor fixtures, a direct bubble call is not reliable. Added [PK23.05 implementation boundary](evidence/PK23.05-implementation-boundary-2026-11-05.md); no source edit or test. Proceed to PK24.05, whose source-authority evidence is ready for a bounded documentation commit.


## 2026-11-05 — PK24.05 authority confirmed; resume PK46.02

Re-ran `python tools/tests/check_notify_effect3_packet.py`: PASS. The PE/Ref/source comparison confirms EF_HEAL=312 versus EF_HPTIME/EF_SPTIME=331/332; legacy self-resource and RNG behavior remains justified, and no selected-live double-heal defect is established. PK24.05 remains READY_TO_COMMIT. Its package evidence/work item link to shared untracked review and hash artifacts spanning other packages, so those are not staged into a mixed commit. Resume PK46.02 per the continuation plan.

## 2026-11-05 — reviewed cleanup and reproducible runtime commit

Reviewed the latest selected inventory/cart correction and production replay plus accumulated runtime/build/research changes. Fixed incremental offline/online party marker resurrection in PreservePartyMapPosition; extended the actual framed/socket/router regression to prove invalidation and fresh-position restoration. Exported staged tree b06e5f025b53f6a9b92cfc849bc06e949dc6e806 to a separate workspace-local source snapshot and configured/built fresh app/replay targets with deployment OFF and cached Lua/zlib sources. Authorized loopback CTest passed 1/1 in 4.65 seconds with 156 PASS JSON records. All 1,471 exported files remained unchanged; all 19 staged runtime/build/test inputs match after Git CRLF normalization. Evidence and hashes: REVIEW-CLEAN-2026-11-05.md/.json and replay.jsonl. Committed as fde621a1, packet(PK14.07): restore reproducible roster repair checkpoint. The user's clean-slate request authorizes this single accumulated reviewed closure; individual unfinished packages are not certified.

PK14.07 and scoped PK14.05 are complete; PK09.02/.03 changes now have a reproducible commit but broader acceptance remains open. PK14.06 incremental stale-marker work is fixed, while actual transition callers remain open. PK05.05 and PK23.05 are still unimplemented; no rendered or deployed-server acceptance is claimed. Active step returns to PK05.05 with no generic dirty-file/strong-review gate. Preserve all accumulated research; corrected five mixed-encoding source notes, nine archived relative links and the request inventory BOM, and include the previously ignored fixture generator with a narrow ignore exception. Request crosswalk and changed/new documentation links checked. Subsequent evidence checkpoints update only documentation and source-disposition status.

## 2026-11-05 — PK22.03 dedicated source checkpoint

Completed only selected 20080910 timed-status 0x043F inactivity and historical-layout separation. Guarded probe/source proof and corrected earlier evidence are committed; historical client handling remains. No runtime timed-status feature, whole PK22 completion or PK48 certification is claimed. Commit subject: packet(PK22.03): checkpoint selected timed-status applicability. Active PK05.05 and implementation order unchanged.

## 2026-11-05 — PK24.05 dedicated source checkpoint

Completed the bounded numeric effect/emitter/PE authority investigation. Preserve legacy local 331/332 resource/RNG behavior; no selected live double-heal defect established. Runtime behavior remains unchanged. PK24.03/.04 sequence/lifetime acceptance and future exact custom-emitter evidence remain separate; this completes no runtime healing repair or whole PK24 package. Commit subject: packet(PK24.05): resolve effect resource authority. Active PK05.05 and implementation order unchanged.

## 2026-11-05 — PK05.05 production terminal-receive repair

Implemented explicit CModeMgr Switch/Quit operation generation, dispatcher batch stop, and a one-shot production-used CGameMode receive/update preamble before actor/bootstrap/world/audio work. Accepted handoffs retain precedence over EOF/timeout; unknown/malformed framing and unexpected EOF clear pending return/exit intent and request Login exactly once with durable specific status. Open/no-data/refusal continues; timeout requires actual drain, not packet-budget exhaustion. Existing lifecycle cleanup is reused. Added bounded Luna-drafted tests, reviewed/corrected by the primary agent, covering exact status/operation/intent, suppressed suffix, accepted ACK+EOF, repeated actual OnUpdate, external handoffs and reconnect reset. Both deploy-off app/replay targets built; authorized loopback CTest passed 1/1 in 4.57 seconds with all 157 JSON records PASS. Source/staged blob and binary hashes plus records are retained under PK05.05-map-disconnect-repair. All eight runtime/test inputs match the tested staged closure. Commit: packet(PK05.05): leave map on terminal receive failure. PK05.05 scoped checkpoint complete; no rendered/deployed or full PK48/PK49 certification. Active step advances to PK14.06.

## 2026-11-05 — PK14.06 actual transition caller repair

Implemented the approved per-mode loading/reconnect/handoff seam while retaining production parsing, session mutation, loaded-map selection and reuse cleanup. Pending disconnect clearing remains in the real handler; failed reconnect clears stale return intent. Fixed immediate OnNotifyZonesvr marker invalidation before new character/map identity, not only at later exit. Luna drafted bounded caller fixtures; primary agent reviewed and strengthened ordering, loaded-state/server-move and same/different-map character branches. Actual framed/router map replay, real same-map ACK loopback capture, both reconnect results, malformed isolation and singleton character PollNetwork followed by real OnExit pass. Deploy-off app/replay targets build; final authorized loopback CTest passed 1/1 in 4.86 seconds, 158 PASS records. Six source/test input hashes, binary hashes/times and JSON records are retained in PK14.06-transition-caller-replay evidence. Dedicated commit: packet(PK14.06): bind party markers to map lifetime. Scoped headless caller checkpoint complete; rendered/native Qt and full PK48/PK49 remain open. Active PK14.04. A separate PK09 cart tail-field defect/fixture is queued, not included in this checkpoint.

## 2026-11-05 — bounded PK09.04 cart field repair

Selected source audit found Commerce equipment cart parsing discarded WearState and bindOnEquipType while the inventory path retained them. Primary agent added only those assignments; preserved raw bind word in legacy m_isYours, with no inferred boolean ownership or guessed cart-ammo normalization. Compiled independent guarded server-header probe asserts normal/equipment strides 22/26 and wear/expiry/bind offsets 8/20/24. Luna drafted literal fixtures; primary reviewed and tightened malformed-value preservation. Actual queued receive extraction/framing/router/Commerce checks pass for equipment and normal tails, malformed list and empty category. Both deploy-off targets build; authorized CTest passed 1/1 in 4.88 seconds, 159 PASS records. Source/server/shim/probe/binary hashes and JSON records retained in PK09.04-cart-wire-fields evidence. Dedicated checkpoint: packet(PK09.04): preserve selected cart wear and bind fields. PK09.04 remains IN_PROGRESS for broader source/variant/UI/lifecycle acceptance; active PK14.04 unchanged.

## 2026-11-05 — PK14.04 integrated/native-QML acceptance

Implemented a production-used targeted minimap adapter entry point and an actual Windows memory-HDC/production GameOverlay.qml capture test using synthetic map art and bundled font. Initial real pixels exposed native red/blue channel reversal and map-edge marker bleed; fixed RGB-to-COLORREF conversion and native map clipping. Retained diagnostic before captures/logs and final 18-scene native/QML matrix. Actual OnProcess/paint, real adapter/image builder/QML repeater, production framed roster/member/position/map handlers, reconnect outcomes and session exit now prove movement with no stale pixels, roster preservation/loss, offline/online, sentinel, map filtering, layer order and bounds. All three final contact sheets visually inspected. Added the existing standalone party-state test to CTest with assertions active in Release. Guarded selected-server probe independently asserts 0107 size/signedness/offsets. Four targets build with deployment OFF; integrated CTest passed 3/3 in 7.22 seconds, all 159 packet records and 18 native/QML scenes PASS, zero QML warnings/errors. Exact input/binary/asset/plugin/image hashes retained in PK14.04-rendered-acceptance evidence. Classified only AUDIT-0x0107 APPLICABLE_AND_FULLY_IMPLEMENTED_VERIFIED under new PK14.PARTY-POSITION contract; independent PK48/deployed provenance not certified. Dedicated commit: packet(PK14.04): verify native and Qt party marker rendering. Active PK23.05.

## 2026-11-05 — PK23.05 cast-announcement repair

Restored Ref Zc_useskill_ack non-monster regular-branch mode99 with zero ordinary extra and destination AID for 334/335, preserving freeze/monster exclusions and local instant deferral. A literal production-router regression failed before repair and now verifies 14 named text/expiry/identity/tuple/exclusion/cancel/repeat checks using real actor head bubbles and metadata. Full integration found the pre-existing 0x0110 nonzero-result freeze-release regression; restored Ref unconditional unfreeze before result branching and strengthened the existing 23-packet fixture to check rejection-only pose reset. Fixed missing conditional Qt link in the skill replay target. Guarded selected-server probe verifies 24-byte 013E offsets and uint32 element/delay. All targets build deploy-off; full CTest 106/106 PASS in 26.30s, preserving 159 packet records and 18 native/QML scenes. Evidence and exact provenance: PK23.05-cast-announcement-repair. Broad PK23/PK48/PK49/rendered balloon/live profile acceptance remains open. Dedicated commit: packet(PK23.05): restore timed cast announcements. Active PK09.04.

## 2026-11-05 — PK09.05 fresh item snapshots at map entry

Selected server inventory/cart senders omit empty categories; primary Ref initialization clears old lists before load-end acknowledgement. Real OnInit regression exposed retained inventory; separate real same-map reuse regression caught the bypass before its production ACK. Shared ResetForMapEntry now clears session inventory/storage and map-backed commerce state on both paths, closes actual native storage, preserves local panel positions/tab, and leaves authoritative actor appearance intact. Literal four-route rows, omitted equipment category, repeated real entry and existing singleton ACK/world/player/view assertions pass. All targets build deploy-off; full CTest 106/106 PASS in 26.20s with 160 PASS packet records and retained 18 native/QML scenes. Evidence/source/binary/server/diagnostic provenance in PK09.05-map-entry-items. Split PK09.04 remainder into this completed leaf and PK09.06 primary Ref name-colour/UI leaf; raw bind 1 is not bound, 2 is bound, unidentified colour has precedence. No guessed badge, newly closed discovery row or broad PK48/49/live certification. Dedicated commit: packet(PK09.05): reset item snapshots on map entry. Active PK09.06.

## 2026-11-05 — PK09.06 source-defined item hover colours

Traced primary GetItemDisColor through MakeHelpBalloon byte conversion: identified bind2 yellow, other identified white, unidentified gray. Added shared item colour, native inventory/storage painters and production-used explicit native bodies for capture in the forced-Qt build; propagated hover payloads through inventory/storage/equipment/item shortcuts and fixed equipment bind-only visual invalidation. Qt commerce has real source-coloured hover help rather than uniform footer names, with pointer exit and row-removal cleanup. Production projection first failed before repair. Source-extracted Knife/Dagger metadata rows plus real literal selected equipment decoding now verify six combinations across five real paint paths, 30 frames and zero QML warnings/errors. Final contact and representative native/Qt frames visually inspected. All targets build deploy-off; full CTest 107/107 PASS in 33.40s with 160 packet records and preserved 18 minimap scenes. Exact source/binary/reference/font/plugin/image hashes retained. No new numeric discovery closure or full art/live/PK48 certification. Dedicated commit: packet(PK09.06): restore source-defined item hover colours. Active PK09.04 remaining umbrella verification.

## 2026-11-05 — PK09.04 bag projection and selected field matrix

Reproduced then fixed Qt worn-equipment bag rows/counts to match native and primary Ref, retaining central records and ammunition exception. Production snapshot/equip/unequip success/failure, native data, actual QML delegates and four bag frames pass. Added literal complete normal/equipment inventory field checks, category orders, malformed/empty isolation, ammo notification/list refresh/replacement/unequip; extended normal-cart malformed/empty cases and actual historical 01EF dispatch safety. Fresh guarded header probe asserts all four list IDs, record sizes and field offsets; normal WearState is allowed location. All targets build deploy-off; CTest107/107 PASS in 34.28s, 161 packet records, retained 30 item-colour/18 minimap frames. Evidence/provenance in PK09.04-bag-projection. Remaining broad status/delta/UI, historical/independent/deployed limits stay open. Dedicated bounded commit: packet(PK09.04): align worn equipment bag projection.

## 2026-11-05 — PK09.02 selected verification checkpoint

packet(PK09.02): verify selected alias ownership. Current runtime source blobs and binaries match the committed bag/field provenance: 107 tests and 161 packet records pass. Supersedes the historical dirty-tree gate; retains broad PK09.04, historical/deployed and independent-review limits. No runtime change or numeric classification update.

## 2026-11-05 — PK09.03 selected verification checkpoint

packet(PK09.03): verify four selected list routes. Current runtime source blobs and binaries match the committed bag/field provenance: 107 tests and 161 packet records pass. Supersedes the historical dirty-tree gate; retains broad PK09.04, historical/deployed and independent-review limits. No runtime change or numeric classification update.

## 2026-11-05 — PK37.05 cart lifecycle review

Reviewed the uncommitted cart fixtures, corrected selected fixture semantics, reproduced metadata loss and fixed same-index adds, contents/presence ownership and unknown transfer failure notices. Added real actor-option regression for all five masks and remote/local isolation, actual QML Cart-button clear/reopen and authoritative counter/weight frames, plus selected header assertions. All targets build deploy-off; final CTest107/107 in 34.96s, 164 packet records, zero item QML errors; prior renders remain passing. Added bounded PK37.05; PK37.01–.04 and PK09.04 UI variants stay open. No discovery closure or live/deployed acceptance. Dedicated commit: packet(PK37.05): preserve cart contents and presence lifetimes.

## 2026-11-05 — PK09.07 ordinary name contexts and quantity consumers

Reproduced computed list socket suffixes in actual inventory hover, repaired shared hover naming while preserving list defaults, and routed Qt row names through source-defined identification/type predicates. Literal selected refined/unidentified packets and quantity nine/one/empty replacements verify native data, actual QML names/delegates, seven retained frames, hover cleanup and zero QML errors. **107/107 CTest in 37.63s**, **164 PASS packet records**, four name/three quantity frames; retained 30 colour/four bag/three cart/18 minimap scenes and zero item QML errors. Independent source audit also identified remaining card-ID grouping/postfix repeats, special names and detailed expiry work; no broader package or numeric row closure. Dedicated commit: packet(PK09.07): distinguish hover and list item names.

## 2026-11-05 — PK09.08 generic card names

Reproduced same-label distinct card-ID merging through real production/UI fixtures. Restored card-ID aggregation, postfix repeat word position, descriptor-table union and first-slot gate, removing invented label fallback. Primary initialization confirms postfix label transfer clears the prefix. Source-extracted four-socket Knife/card tables verify 17 cases through native inventory/storage and actual Qt inventory/cart/storage labels, including empty-row/hover teardown. **107/107 CTest in 50.71s**, **164 PASS packet records**, 17 card cases and 57 card frames (six native, 51 Qt), zero item QML errors. All targets build deploy-off; prior renders pass. Named/crafted/pet sentinels and detailed expiry/removal remain parent work, with broader packages and discovery classifications open. Dedicated commit: packet(PK09.08): restore card ID and postfix name grouping.

## 2026-11-05 — PK45.05 character-name dependency for special item names

Primary/server trace exposed character IDs being treated as actors by 0194. Reproduced actual actor-cache overwrite, separated receive/cache/timer ownership, and implemented final selected 00A2/15/CID@11 request plus failed-send/duplicate/retry/reply behavior. Fragmented input, CID/actor collision, unseen CID, empty/zero guards, socket capture and actual OnInit/OnExit/same-map cleanup pass. Guarded header probe confirms 0194/30/CID@2/name24@6; source config/registry hashes match pinned selected baseline. **107/107 CTest in 51.15s**, **166 PASS packet records**, two new character-name cases, zero item QML errors. All targets build deploy-off; existing UI regressions pass. No numeric row closure; special named/crafted item UI remains next, followed by expiry/removal. Dedicated commit: packet(PK45.05): isolate character name lookup from actors.

## 2026-11-05 — PK09.09 special item names and producer refresh

Reproduced missing forged producer/element/star names in actual QML rows. Restored primary marker ordering and message-table values with source-selected pet intimacy adaptation, isolated CID lookup and stationary delayed-name refresh; retained detail model and native invalidation work without item resends. Selected itemtype normalization sends pet eggs as armor four and weapons as five; corrected fixtures now follow that ABI. Primary Sort/native proof corrected raw-type-seven historical Qt rows to Other, while selected pets stay in Equipment. Twenty variants, delayed/replaced names, controlled empty-message loading, actual paint and cleanup pass. **108/108 CTest in 71.09s**, **166 PASS packet records**, 20 special-name variants and 77 new frames (eight native, 69 Qt), zero item/special QML errors. All targets build deploy-off; existing regressions pass. Source/synthetic guard scopes remain distinct. Expiry/removal, other type/context variants and broader plan/independent/deployment work remain open. Dedicated commit: packet(PK09.09): restore special item names and producer refresh.

## 2026-11-05 — PK33.05 rental expiry review

Reviewed the uncommitted rental leaf against selected server and primary detail/message/decrement sources. Restored framing, absolute dates and owner-bound detail lifetime; corrected actual Qt popup stacking, scaled placement, long-description containment and drawing-context ownership. **109/109 CTest in 76.96s**, **167 PASS packet records**, 26 rental frames and zero rental QML errors. All targets build deploy-off; independent header probe confirms selected fields. Full PK09.04/PK33 and independent/deployed acceptance remain open. Dedicated commit: packet(PK33.05): restore rental expiry display and removal.

## 2026-11-05 — PK37.06 commerce item detail contexts

Reproduced the vending/cart identification mismatch with original routing. Primary decompiled merchant/trade handlers confirm copied-row detail semantics. Added explicit source resolution, index-free remote offer ordinals, required ID guards and actual right-click entry points, preserving snapshots through local/source removal. Corrected tooltip overlay stacking. **110/110 CTest in 89.32s**, **167 PASS packet records**, 20 commerce detail frames and zero commerce-detail QML errors. Independent selected header probe confirms literal context snapshots. All targets build deploy-off. Source/DB audit prepares visible inventory Cash18/category/radio work; full detail placement, broad transactions and independent/deployed acceptance remain open. Dedicated commit: packet(PK37.06): isolate commerce item detail contexts.

## 2026-11-05 — PK09.10 primary item tabs

Old visible commerce groups fail on source Cash18 and historical1/11. Shared initialized primary All groups now serve native inventory/storage and Qt commerce, excluding unknown types without mutating authoritative rows. Twelve pinned source-derived type cases and12 explicitly synthetic historical/unknown probes verify actual tab actions/slots/delegates, worn rules, malformed/empty/replacement refresh and stationary hover cleanup. Primary radio fields have no connected UI controls; no guessed controls were added. **111/111 CTest in 91.37s**, **167 PASS packet records**, 24 item-type probes, 10 category frames and zero category QML errors. All targets build deploy-off. New primary detail oracle establishes retained/default placement and identifies the next same-key toggle/force-update leaf. Full PK09.04 and independent/deployed acceptance remain open. Dedicated commit: packet(PK09.10): share primary item tab classification.

## 2026-11-05 — PK09.11 primary item detail selection

Old always-update behavior fails repeated actual QML inventory selection. Restored raw-name/refine/four-card Select toggle versus explicit ForceUpdate, pre-open visibility guard, owned close/persistence/recreation and existing owner binding. Tests cover ignored fields, noncanonical raw-name identity, each key word, native/Qt contexts and125% output; current NPC force gestures remain unimplemented and separate from API coverage. **112/112 CTest in 99.44s**, **167 PASS packet records**, 23 detail-selection cases, eight detail frames and zero detail-toggle QML errors. All targets build deploy-off; no packet/discovery closure. Next work maps NPC catalog versus deal-row snapshot owners and source-coupled list fallbacks. Full PK09.04 and independent/deployed acceptance remain open. Dedicated commit: packet(PK09.11): restore item detail toggle and forced updates.

## 2026-11-05 — PK37.07 NPC descriptions and viewport review

Restored catalog and selected buy/sell snapshot descriptions through actual native WindowProc input. Fixed unreachable final deal rows, scrolled header selection, stale shrinking offsets, reopened viewports and overlapping Qt price headers. Literal selected C4/C6/C7 setup, copied unidentified refine/card keys, synthetic owner collisions and125% input pass. Root traced primary list actions to drag setup and retained the legacy grid/signature fallback boundary; no speculative gesture was added. **113/113 CTest in 112.54s**, **167 PASS packet records**, 15 NPC detail cases, 15 generated frames and zero NPC-detail QML errors. No discovery closure or full PK09.04/PK37/PK48/49 acceptance. Continue remaining item/context and broad NPC requests, price bounds/row multiplication, transaction failure and independent/deployed work. Dedicated commit: packet(PK37.07): restore NPC shop item descriptions.

## 2026-11-05 — PK37.08 exact NPC amounts

Source audit confirms unsigned32 prices, populated secondary quote words including zero and MAX_AMOUNT30000. Fixed signed narrowing, zero fallback, signed row multiplication, aggregate INT_MAX saturation, overflowing quantity addition and unlimited zero sell stock. Native painters share explicit HDC boundaries; full15-digit rows and odd12884897590032705 total render exactly through QString properties/roles in Qt. Reintroducing old aggregate saturation fails the new C6 regression. **114/114 CTest in 114.40s**, **167 PASS packet records**, seven NPC amount cases, eight amount frames and zero NPC-amount QML errors. Boundary prices, many-row aggregate and availability mutations are explicit synthetic/source-layout fixtures, not legal server shop/inventory claims. Broad NPC eligibility/sends/results/teardown, full PK09.04 and independent/deployed gates remain open. Dedicated commit: packet(PK37.08): preserve exact NPC shop amounts.

## 2026-11-05 — PK37.09 NPC transactions and strong-source adjudication

Root reviewed/corrected Luna source/fixture drafts, traced primary local cancel and immediate pane closure, fixed full-argument and row/quote guards, signed16 request lengths, nonstackable limits/delay-consume prompts, stale count callbacks, immediate failed sends and ordered reply lifetime. Six production loopback/router records and actual native/Qt quantity UI pass; map entry/exit clear receipts, and newer NPC contexts retain them without being mutated by older replies. Reintroduced empty C8/C9 cancel fails the new packetless regression. The later market-stock branch is inactive for20080910; duplicate client submissions are rejected without claiming an external server repair. **115/115 CTest in 121.44s**, **173 PASS packet records**, six NPC transaction replay records, four quantity UI cases/16 type probes, four frames and zero NPC-quantity QML errors. No discovery closure/full PK09.04/PK37/PK46.02/PK48/49 acceptance. Continue full item context/saved tabs/geometry and broader NPC eligibility/recovery/authoritative updates/asynchronous failure plus status/mail/historical/deployed work. Dedicated commit: packet(PK37.09): guard NPC shop transactions and replies.

## 2026-11-05 — PK09.12 strong startup review

Caught the late primary startup overwrite omitted by the initial persistence interpretation; implemented cold-start1/default0 versus same-process retained selection. Native and Qt share canonical state; real close/deletion stores tab and recreation uses runtime state. Deliberately restoring the last disk tab fails the regression. **116/116 CTest in 140.49s**, **173 PASS packet records**, 29 fresh child processes, 19 generated frames and zero startup-child QML errors. Original geometry and whole-record coupling remain strong-agent work; no packet/discovery closure or whole-plan completion. Dedicated commit: packet(PK09.12): share source inventory tab startup and retention.

## 2026-11-05 — PK09.13 source inventory geometry

Strong review and implementation restore whole-record validation, source resize clamp/truncation, raw versus normalized original height,17-pixel mini, full dimensions, native/Qt resize input and explicit owned close/recreation. A bounded Luna fixture audit was reviewed and corrected against initialization order and live height. Position-only fallback fails the regression. **117/117 CTest in 147.37s**, **173 PASS packet records**, seven fresh geometry child processes, nine invalid-record cases, seven resize quantization probes, eight geometry frames and zero geometry-child QML errors. Modern Qt palette remains; inventory geometry/input now fits the source compact shape. Detail geometry/default placement and inventory map/show recreation remain next; no packet/discovery closure or whole-plan completion. Dedicated commit: packet(PK09.13): restore inventory rectangle and mini lifecycle.

## 2026-11-06 — PK09.14 parent detail geometry and viewer

Strong review and implementation restore cached complete detail records, source120/149/144 branches, strict recreation, source placement and fixed clipped viewer with native/Qt scrolling. Visible slots follow descriptor count and marker rules; reused display output no longer retains stale slots. Native child deletion clears captured pointers. Bare CR layout now advances. Dedicated fresh-process/actual-paint tests and updated source-position/grey-name oracles pass: **118/118 CTest in 154.96s**, **173 PASS packet records**, eight fresh detail child processes, nine detail frames and zero detail-child QML errors. Separate card WID82 and hidden/prefix input semantics were traced and remain required, along with map/show and broader packages. No packet/discovery closure or whole-goal completion. Dedicated commit: packet(PK09.14): restore item detail geometry and clipped viewer.

## 2026-11-06 — PK09.15 strong review and separate card details

Accepted separate WID82 snapshot/placement and source hidden-band nonzero-prefix hits, independent of painting. Native/Qt right-click, hover, close, lifecycle and viewport balloons now pass; strong review also fixed popup priority and native tooltip layering. Corrected fixture setup/synchronization and blocked click-through assumptions. All three deliberate regressions fail before restoration. Final verification: 119/119 CTest in 166.57s; 173 PASS packet records; nine fresh card-detail children; zero QML errors. Full PK09.04 and broad packages remain open. Commit: packet(PK09.15): separate slotted card details and source hit testing.

## 2026-11-06 — PK09.16 inventory show/map reconstruction

Strong review distinguished explicit inventory close from bulk teardown and restored the source remembered-open bit, delete-first button/Alt-E and basic-UI reconstruction. Production actual OnInit replay and fresh native/Qt fixtures verify startup show override, raw hidden reuse, full/mini geometry/tab, decoded row identity and explicit close/no reopening. Both deliberate regressions fail before restoration. Final verification: 120/120 CTest in 169.39s; 173 PASS packet records; four fresh show-state child processes (including one seed); three retained frames; zero child QML errors. Full PK09.04, broad packages and independent/deployed gates remain open. Commit: packet(PK09.16): restore remembered inventory across map teardown.

## 2026-11-06 — PK09.17 inventory acknowledgement repair

Strong source review repaired pickup retry cleanup, strict equip outcomes and unknown-row guards, additive metadata retention, authoritative use counts, routed-mode ownership and original notice loading. New receive and actual native/Qt tests cover the state/notice transitions; five intentional regressions fail after correcting the notice-cache whitelist omission. Shared offscreen Qt runtime fixes nested replay application lifetime. Final verification: 121/121 CTest in 174.02s; 174 PASS packet records; 7 GUI cases and 12 retained frames; zero QML errors. Acquisition notification and full PK09.04/broad/independent/deployed acceptance remain open. Commit: packet(PK09.17): repair inventory acknowledgement state and notices.

## 2026-11-06 — PK09.18 acquisition notification review

Completed strong source/PE review and the pending native/Qt item notification; corrected fixture offsets, ownership, font/geometry and Qt child-property conflict. Three intentional regressions rejected. 122/122 CTest in 171.33s; 174 PASS packet records; 9 notification GUI cases and 8 retained frames; zero QML errors. Strong WID58 item gate cleared; money/full item/context/effect/broad/independent/deployed acceptance remains open. Commit: packet(PK09.18): restore item acquisition notification.

## 2026-11-06 — PK09.19 item-use feedback

Strong review completed source potion selection/audio paths, repeated-effect replacement, auxiliary effect, experience notices and basic recovery feedback. Corrected bounded-agent source/fixture/lifetime assumptions. Four intentional regressions rejected. 123/123 CTest in 173.21s; 176 PASS packet records; 220 source rows checked across all65536 item IDs; 5 GUI cases and8 retained frames; zero QML errors. Next013D signedness/producer reconciliation, full item/effect/context and broad/independent/deployed gates remain open. Commit: packet(PK09.19): restore source item-use feedback.

## 2026-11-06 — PK09.20 signed recovery

Resolved selected field/order and PE arithmetic; restored canonical signed/modulo deltas, zero feedback, unknown silence, shared RNG and truthful UI maxima. Added active/standalone source-derived units; legacy broad unit project remains a documented migration gap. Five intentional regressions rejected. 126/126 CTest in 176.61s; 177 PASS packet records; source-derived recovery/resurrection unit and source checks; 5 GUI cases and8 retained frames; zero QML errors. Full plan remains active. Commit: packet(PK09.20): restore signed recovery deltas and feedback.

## 2026-11-06 - PK48.02 strong review / clean-slate checkpoint

Reviewed305c48cf and accumulated test infrastructure. Restored all-target standalone build, retained production extraction guards and repaired three fixture setups. Root build succeeds and126/126 CTest pass in175.36s; standalone448/460 in73.61s, twelve failures retained with explicit source/semantic assignments. PK09.20 evidence hashes match. Corrected chat handler location and notice-versus-expand continuation. No runtime changes, live/deployment certification or completed feature claims. [Review](evidence/REVIEW-2026-11-06.md), [results](evidence/REVIEW-2026-11-06.json). PK48.02 remains IN_PROGRESS. Commit: `packet(PK48.02): restore standalone checks and record review gaps`.

## 2026-11-06 - PK48.02 effect repairs / PK45.06 corpse gate

Strong source/PE review repaired projection depth clamping, defined original SphereWind2 int64/low-word conversion, freeze/stone SPR choice and unknown receive-emotion action0 fallback. Reviewed Luna test migrations, preserved independent literal/source expectations and captured missing ignored guild test compiler source. Final full root126/126 in174.40s; standalone459/460 in72.64s, sole remaining actor corpse failure. [Evidence](evidence/REVIEW-2026-11-06-followup.md), [provenance](evidence/REVIEW-2026-11-06-followup.json). Allocated active PK45.06 IN_PROGRESS with exact source/ownership/render acceptance. Corrected false state10 claim. PK09.04/full plan/PK48/49 remain open. Commit: `packet(PK48.02): repair effect semantics and narrow remaining review gap`.

## 2026-11-06 - PK45.06 reviewed sprite checkpoint; model branch pending

Restored independent sprite corpse resources/rendering and source completed-motion/queued-hit retirement. Corrected PE clock factor/storage (expiry6120ms), original Render vtable identity and double depth arithmetic. Production replay covers ownership, ID reuse, duplicate/adjacent removal and teardown; six actual Vulkan frames prove art/fade/clip geometry. Full root127/127 in174.27s; standalone460/460 in72.44s. Review reran build, production full/focused replay, source check and capture verifier successfully. [Evidence](evidence/PK45.06-sprite.md), [provenance](evidence/PK45.06-sprite-provenance.json). Separate CGrannyCorpse3d remains pending under the same IN_PROGRESS step; updated exact strong-agent continuation. No full package/certification/goal closure. Commit: `packet(PK45.06): restore independent sprite corpse lifecycle`.

## 2026-11-06 - PK45.06 separate GR2 corpse lifecycle complete

Root verified PE byte alpha and immediate virtual state3 handoff, implemented private model/animation/mesh ownership with shared resource pins, removed invented model timers and restored source model transforms/shading/cutout. High-resolution animation time starts after setup and advances independently of processing count. Production framing/queued-hit/retirement, same-ID reuse, cache teardown, exact counters and owning sweep pass;16 native cases cover six models, alias, animation birth/progression/final,60-second gap, cutout/expiry and independent PE geometry. Final root127/127 in174.52s; standalone460/460 in72.36s. [Evidence](evidence/PK45.06-gr2.md), [provenance](evidence/PK45.06-gr2-provenance.json). PK45.06 completed; active step returns to PK09.04. Parent PK45.02 retains live non90 transform and broad attack/appearance review; no numeric row, full package, independent/deployed or whole-goal closure. Commit: `packet(PK45.06): restore independent GR2 corpse lifecycle`.

## 2026-11-06 - PK09.21 reviewed main chat state checkpoint

Strong source/PE review corrected draft height and signed-cycle assumptions, restored source main state/anchor/manager routes and shared native/Qt rectangles, and fixed the reproduced Qt frame-capture regression. Literal, managed-window and actual Qt editor-focus checks pass; eight native/QML frames cover compact and expanded states with zero QML errors. Final root130/130 in181.02s; standalone461/461 in72.91s. [Evidence](evidence/PK09.21-main-state.md), [provenance](evidence/PK09.21-main-state-provenance.json). Updated strong/bounded-agent instructions for the unfinished sticky/mini/history, routing, capture and placement integration. PK09.21 stays IN_PROGRESS; no parent/discovery/independent/deployed/whole-plan closure. Commit: `packet(PK09.21): restore main chat state transitions`.

## 2026-11-06 - PK09.21 detached family ownership checkpoint

Root implemented managed histories137/138 and mini140, transient sticky save/restore, source creation ordering/delete-or-create behavior, independent copied rows/half-trim, cached rectangles and native/Qt controls. Hidden-HWND WindowProc cases prove actual outside-client release, cancellation, replacement capture and inline detach under parent capture. Eleven UI frames and zero QML errors retained. Full root130/130 in181.22s; standalone461/461 in72.73s; both all-target builds succeed. [Evidence](evidence/PK09.21-family.md), [provenance](evidence/PK09.21-family-provenance.json). Strong review corrected the inferred backlog gate to the13-entry battle-option filter, with retained PE addresses; exact source producer/parser integration remains next. Full history interactions/options, restart/main restore, active Qt/scale and broad acceptance remain open. PK09.21 stays IN_PROGRESS; no parent/discovery/independent/deployed/goal closure. Commit: `packet(PK09.21): restore detached chat family ownership`.

## 2026-11-06 - PK09.21 typed source notice routing checkpoint

Root implemented explicit source routes/types, talker/sentinel/split parsing, independent240-row main streams and unsplit detached mirrors, separate non-replayed backlog and main-instance receipt generations. Selected pickup/equip frames retain type5/6 and reviewed colors; known pickup failure now performs manager3 even with a real empty language resource. Retained auxiliaries project/interact without main availability. Luna supplied bounded source inventories/producer fixture; root replaced direct internal-state fixture swapping, added managed-owner/empty-resource proof and verified PE literals/arguments. Historical show-only mutation rejected; final source restored byte-for-byte. Root132/132 in182.29s; standalone462/462 in74.93s; both all-target builds pass,13 UI frames/zero QML errors. [Evidence](evidence/PK09.21-notices.md), [provenance](evidence/PK09.21-notices-provenance.json). Source primary tabs/240-row rendering, remaining producers, controls/options/persistence and active Qt/scale/locale acceptance remain open. PK09.21 stays IN_PROGRESS; no parent/discovery/independent/deployed/goal closure. Commit: `packet(PK09.21): route typed chat notices`.

## 2026-11-06 - PK09.21 source primary history review checkpoint

Reviewed and integrated source ST/BT rendering,240-row data versus50/256 diagnostic owners, shared native/Qt source scrolling/selection/talker input and represented no-room008D/008E/0109/017F/EXP source metadata. Strong source/PE review corrected tab-switch virtual Resize and active Indonesia name-color assumptions; fixed duplicate overlapping tab hit and native premultiplied text/transparent labels. Migrated stale inventory/EXP UI fixtures while retaining exact packet/text/color/render checks. Final deploy-off all-target build, focused9/9 in8.88s, root134/134 in180.69s and standalone463/463 in73.39s pass. Twenty native/QML frames, zero QML errors, before/after alpha pixel evidence, source/PE dumps and hashes retained in [primary evidence](evidence/PK09.21-primary.md) and [provenance](evidence/PK09.21-primary-provenance.json). Continuation starts with detached wrapping/visual cap and controls, options139 and persistence; remaining producers, bubble rendering, active Qt/scales/art/locale and broad gates stay open. PK09.21 remains IN_PROGRESS; no parent/discovery/independent/deployed/whole-plan closure. Commit: `packet(PK09.21): render source primary chat histories`.

## 2026-11-06 - PK09.21 detached visual history checkpoint

Root restored independently capped logical messages and pixel-wrapped visual rows, source character-unit/CRLF/marker arithmetic, width and resizer replay, shared native/QML scrolling/selection/talker input and correct direct37 COLORREF conversion. Strong review corrected a bounded inventory's misleading helper/cast/page assumptions before integration. Main history double-click now retains child capture and scrollbar double-click cannot target a row; actual UI fixture double-clicks include matching button-up. Production owner checks, literal UTF-8/safe-progress cases and real hidden-HWND thumb/outside-release checks pass.150-M message reflows7-to4 rows;28 native/QML frames and zero QML warnings/errors retained. Deploy-off all-target build passes; root136/136 in183.66s, standalone464/464 in74.06s, focused9/9 in4.23s. [Evidence](evidence/PK09.21-wrapped.md), [provenance](evidence/PK09.21-wrapped-provenance.json). Next: source options139 with source-ready inventory, then persistence/main cached restore; exact source bitmap/font/shadow/color-tag/codepage, remaining producers, bubbles, active Qt/scales and broad/independent/deployed gates remain open. PK09.21 remains IN_PROGRESS; no parent/discovery/whole-plan closure. Commit: `packet(PK09.21): restore detached visual chat histories`.

## 2026-11-06 - PK09.21 battle-message options checkpoint

Root implemented manager139/source127 toggle versus135 save-close,13 session filters with selected PE IDs1277..1288/1331 and title1289, positional resources with known-empty preservation, actual startup preferences and separate-process write/read proof. Source14-item range includes a blank final row;134 remains inert. Actual Qt editor Escape preserves draft/focus while closing options; mouse-down close cannot orphan capture. Strong visual review fixed scale and peer-layer defects, retained before evidence, expanded checks to every overlapped checkbox and unified main/history/mini/options scale parent. Mapped controls now win modern frame-resize borders; managed and real top-edge input checks pass. Final deploy-off build, root140/140 in184.41s, standalone465/465 in74.96s and focused13/13 in5.80s pass;39 frames/zero QML errors retained. [Evidence](evidence/PK09.21-options.md), [provenance](evidence/PK09.21-options-provenance.json). Root corrected historical primary label IDs to selected1291/1292 and documented next exact shared scrollbar arrow/thumb/relative-drag work, followed by primary/detached labels, remaining window persistence/main restore and producers. Full art/codepage, bubbles, active Qt/global overlap, broad scales/independent/deployed gates remain open. PK09.21 stays IN_PROGRESS; no discovery/parent/whole-plan closure. Commit: `packet(PK09.21): restore battle message options`.

## 2026-11-06 — PK09.21 source scrollbar strong review and checkpoint

Baseline d9428eed; selected source/PE and unchanged20080910 server context. Implemented main/detached/options arrow/page/thumb regions, source subclass10px versus base13px arrows/fixed13px width, relative drag with actual clamped movement and retained captured-child identity/dimensions. Strong review corrected the earlier primary trim assumption: Recalc clamps bar position only; text can remain stale and blank. Separate offsets and bounds-safe projection now preserve this quirk; actual WindowProc up input reconciles it. Horizontal review establishes disabled history scrolling; no horizontal behavior or repeat timer invented.

Evidence: PK09.21-scroll.md and hash-pinned provenance, selected PE/source slices,45 positive native/QML frames/five results and raw managed case. All-target deploy-off build passes; focused15/15 in6.16s, root142/142 in180.47s, standalone466/466 in77.74s. The initial blank-pane ink failure counted border pixels and was a fixture issue; final text-interior count passes. Subject: `packet(PK09.21): restore source chat scrollbar controls`. Trackers and continuation instructions updated; PK09.21/PK09.04, broad/independent/deployment/whole-plan gates remain open. Next: selected ST/BT labels1291/1292, restart geometry/toggle/main cached restore, remaining producers/bubbles and art/locale/active Qt acceptance.

## 2026-11-06 — PK09.21 source labels and window restore/preferences

Baseline23a655e1; unchanged selected20080910 source/profile. Two bounded Luna tasks supplied language fixtures/code and read-only restore/next-producer inventories; root reviewed/integrated and implemented source cache/show behavior. Main/detached language IDs1291/1292 retain empty-versus-missing semantics; mini text remains a neutral bitmap replacement. Strong PE review proves startup status is write-only and physical height precedes whole-rectangle placement validation/SetState. Shutdown full teardown0x4DD720 stores owners then writes geometry; Ref's duplicated ExceptChat alias is not authoritative. Source geometry, sticky/mini/show flags survive a fresh process; basic reconstruction honors remembered hidden main. Source cache writes are separate from immediate modern style preferences.

Actual native/Qt cached123-to-state4 placement and localized/empty owner labels are checked. Visual review fixed a real long-caption spill with per-owner clipping; native Qt titles accept UTF-8. A headless viewport inconsistency was corrected. Invalid out-of-fit writer coordinates and an old hardcoded-BT item-use expectation were fixture issues; their corrections retain source identity/range behavior. Evidence PK09.21-restore.md/provenance includes selected source/PE, raw independent writer/reader cases,47 positive frames and five results. Deploy-off all-target build passes; focused21/21 in14.57s, root146/146 in180.92s, standalone466/466 in73.69s. Subject: `packet(PK09.21): restore chat labels and window preferences`.

PK09.21/PK09.04, discovery, broad/independent/deployed and whole-goal gates stay open. Next implement chat-room owner/route5 and membership lifetime with original008D/008E main/bubble suppression, then dedicated per-name whisper policy/type2 fallback and remaining producers/bubbles. Work-item source anchors and preservation rules are updated; exact skin/codepage/active Qt/global overlap/broad scales remain unaccepted.

## 2026-11-06 — PK09.21 room reception/routing reviewed partial checkpoint

Baseline202cee11; subject `packet(PK09.21): restore chat room reception and routing`. Integrated bounded parser/receipt fixtures and independently reviewed source/PE ownership/geometry. WID28 now receives D6/D7/D8/DA/DB/DC/DD/DF/E1 through production framing, separate room-owner map and metadata32/33. Room speech suppresses main/bubbles; black0, authoritative counts, duplicate arrival order versus sorted rebuild and selected-source controls/cache survive native/Qt projection.

Review corrected real OS capture-release omission and generic parent-only Resize fidelity; source resize maximum504x376 and unchanged-size no-op have literal regressions. Selected actual sends are F3/96/E3. Normal outgoing world and room text now waits for server echoes before bubbles. Fixtures initially assumed unquantized maximum, classic8C and speaker prefixes in bubbles; each correction follows independent source/server behavior, without weakening lifetime or wire assertions. Existing standalone scaffolds were updated for actor metadata cases and room ground-click context.

Deploy-off all-target build passes; focused23/23 in15.66s, root148/148 in181.28s, standalone467/467 in75.70s. Final raw room lifecycle/ack cases pass.55 positive native/QML frames and six result records retained, with debug failure frames excluded. Original EXE was never run/copied, Ref/server remain read-only, no installed deployment/live account used. See [room evidence](evidence/PK09.21-room.md) and exact provenance.

Added concrete strong-agent continuation instructions for actor title anchor/input, join wait/password, create/change dialogs and role-dependent owner controls, with selected-server wire authority. Metadata is not world-title rendering. Dedicated whisper, remaining producers/bubbles, editor history/full talk-type/charset/rate limits, exact art/locale, active Qt/global overlap and broad scales remain open. PK09.21 stays IN_PROGRESS with null completion subject; no discovery/broad/independent/deployed gate or whole-plan completion is claimed.

## 2026-11-06 — PK09.21 room title/join/password reviewed partial checkpoint

Baseline2ea36ef0; subject `packet(PK09.21): restore room titles and joining`. Actor32/33 now own real managed world titles with safe actor/manager teardown, source bitmap compositor, text/count/type3/ellipsis behavior, projected screen caches and normalized/cached anchor math. Render owns lastTlvert fields; bootstrap/cell assignments no longer overwrite them. Public/non3 and private/password actions dispatch selected D9 through source waiting/active-room guards. Strict1000ms and wrap-safe1001ms retry, retained wait after failed send, no optimistic ownership and actual DB/title teardown have production localhost proof.

Password WID29 retains source geometry/message15/masked8-byte input, focused rather than invented global modal ownership, release-inside bitmap button with maximum dimensions across states and native-sized current art, uncovered-frame dragging, outside release/cancel and safe deletion during capture. Actual native/Qt checks cover bitmap colors, shortening, masked input, retained-window focus,125% pointer conversion and actual owner removal. Root final asset-path review fixed the missing CP949 interface lookup; conflicting flat shadow BMPs now prove original-key priority. Root reviewed/integrated Luna pure request builders/literals and source inventory. An incorrectly25-byte E2 fixture and a125% fixture scale omission were corrected using independent wire/UI evidence. Root rejected the proposed people option7 as an exception-state misread; source choices remain2,3,5,10,15,20. Body-extraction endpoint changed to the actual next source function; extracted image behavior is unchanged.

Final deploy-off all-target build passes; focused23/23 in16.46s, root148/148 in182.26s, standalone467/467 in74.32s. Three final production room-only records pass.63 positive native/QML frames, seven result JSONs and explicit synthetic BMP/message fixtures retained. A transient unchanged source-check IndexError, isolated rerun and subsequent full pass are documented without altering that check. Original EXE only read/hashed/statically disassembled; Ref/server read-only; no installed-client/live account/deployment. See [entry evidence](evidence/PK09.21-entry.md) and exact provenance.

Continue create/change and role-dependent owner/member controls: source message boxes/insult filter and pending metadata/send timing need full integration; pure D5/DE/E0/E2 builders are not production actions. Then dedicated whisper, remaining producers/bubbles, full editor/skin/font/codepage, active Qt/global overlap and broad scales. PK09.21 remains IN_PROGRESS with null completion subject. No discovery row, broad actor, independent/deployed or whole-goal gate is closed.

## 2026-11-06 — PK09.21 creation/settings strong review and clean checkpoint

Integrated WID27/30, source validation message boxes, real manner loader/matching, D5/DE sends and pending metadata boundaries. Strong review corrected missing OS-language folder precedence, self-DD Change teardown and hidden active-shop eligibility. Preserved source byte caps, private minimum, radio mouse-down, count0/idFFFFFFFF, label20 prefill and TrickDead ownership quirks. New actual native/QML input/capture/modal/125% checks supplement real localhost wire/lifecycle tests.

Deploy-off build passes; focused4/4 in8.50s, root149/149 in25.45s, standalone468/468 in12.81s; four raw room replay cases pass. Ten new positive frames and one result JSON retained. See [settings evidence](evidence/PK09.21-settings.md) and provenance. Subject: `packet(PK09.21): restore room creation and settings`.

PK09.21 remains IN_PROGRESS with null completion subject. Next strong integration is role-dependent owner/member menus and mode51/52, then dedicated whisper. Exact form/message/radio artwork, source viewer wrapping, full editor/command/codepage, active Qt/global overlap, broad scales, discovery and independent/deployed gates remain open. Work-item and handoff instructions distinguish represented behavior from remaining acceptance.

## 2026-11-06 — PK09.21 room member actions reviewed partial checkpoint

Source member right release now opens role-dependent managed native/QML menus, preserving selection, source menu-index dispatch, self/info/settings branches and friend/server restrictions. Mode51/52 send E0/E2 with receipt-owned roles/membership; friend21 sends0202; info3 uses unsupported191. Room export sanitizes filenames, uses first-unused%03d, reproduces embedded Windows LF translation and explicitly expands main before the green saved-path notice. Source popup18 conflicts with port NPC18 and uses a dedicated10018 owner instead. A final selected-PE audit preserves the independent menu after room deletion, with current-room lookup and safe stale-index rejection; captured release and100/125% rendering are checked.

Deploy-off build passes; root149/149 in27.39s, standalone468/468 in13.69s; five final raw room cases pass. Four new frames and a result JSON retained. Strong review corrected prior plan/fixture action-ID, self/settings and rounded-transform assumptions and a production export-byte defect. See [member-actions evidence](evidence/PK09.21-actions.md) and provenance. Subject: `packet(PK09.21): restore room member actions`.

Dedicated whisper is next; bounded source inventory is ready for strong PE/ownership review. PK09.21 remains IN_PROGRESS with null completion subject. No parent, broad producer/editor/locale/art/active Qt/global scale, discovery, independent or deployed closure is claimed.

## 2026-11-06 — PK09.21 dedicated whisper strong review and clean checkpoint

Integrated exact-name whisper UI owners, independent copied history/edit state and manager lifetime; source0097/helper/type2 fallback and0098 branch ownership; private0096 guards, auto-prefix reply, fresh-process options/shared geometry, local context exclude/include and privileged source autosave. Strong source/PE review corrected heuristic-versus-membership assumptions, incoming pending-metadata clobbering, source edit-history replacement/advance, active-modal stacking and the older draw-slot annotation (+50 draw; main text+54; +4C no-op). Safe bounded string and app-data path adaptations remain explicit.

Deploy-off all-target build passes; focused6/6, root154/154 in27.98s, standalone469/469 in12.39s, direct production whisper and fresh-process write/read PASS. Twelve new native/QML frames and result JSON retained. See [whisper evidence](evidence/PK09.21-whisper.md) and provenance. Subject: `packet(PK09.21): restore dedicated whisper routing`.

PK09.21 remains IN_PROGRESS with null completion subject. Continue private whisper menus/manual export/report and mode114/mode26 callers, UIAutoMessageWnd configuration/aliases, messenger controls, full source/global/main send11/talk-type/commands and ignore-list workflow. Remaining producer/editor/art/locale/active Qt/global scale, discovery, independent and deployed gates remain open. No whole-plan completion or numeric discovery closure is claimed.

## 2026-11-06 — PK09.21 private whisper menu/export strong integration

Implemented source private popup49, mode114 menu state/default caller -> mode26, current-name save lookup and storage-owner refusal. Added shared name-only AddFriendCharName for exact0202 without actor-ID guard, preserving24 bytes and receipt-owned roster. Normal export reproduces retained rows, embedded Windows LF translation, sanitized/collision paths and notices. Strong selected-PE review preserves report44E720's success no-op: Report/local-character path and notice without creating a report file. Integrated six source save/report aliases; full parser/exit integration remains open. Native right capture/cancel/release and100/125% passive QML popup checks pass.

Deploy-off build; focused3/3 in10.50s; full root154/154 in33.88s; standalone469/469 in15.36s; direct production whisper PASS. Four new menu frames retained;125% QML visually inspected. See [workflow evidence](evidence/PK09.21-whisper-workflow.md) and provenance. Subject: `packet(PK09.21): restore whisper menus and exports`.

Luna read-only inventories resolve auto-message commands,100-byte configuration, IDs940–944 and explicit Escape versus generic deletion. Root must review source UIY placement, login reset and actual native/QML ownership/input next. PK09.21 remains IN_PROGRESS with null completion subject. No broad/parent/discovery/independent/deployed or full-goal closure is claimed.

## 2026-11-06 — PK09.21 auto-message configuration strong integration

Implemented WID116, selected100-byte editor and source button assets, toggle aliases/message47 bridge, submit/Escape/generic-delete separation and accepted account0069 default940. Strong selected-PE review resolves UIY4E72E0 center versus misleading constructor alias, primary talk type152 versus secondary153, selected language immediates and SetTextByMaxChar space padding at a cut multibyte boundary. Shared bounded first-token normalization also corrects case-sensitive private save/report commands. Root reviewed Luna's pure helper and kept complete parser/IME/global focus/art gates open.

Deploy-off build; focused4/4 in11.71s; full root156/156 in36.49s; standalone470/470 in13.75s; actual0097->0096 auto-message replay PASS. Eight new native/QML frames with no QML warnings; selected and125% frames visually inspected. See [auto-message evidence](evidence/PK09.21-auto-message.md) and provenance. Subject: `packet(PK09.21): restore auto-message configuration`.

Next actual UI family is messenger UIFriendOptionWnd68, from bounded Luna source leads. Root must inspect selected labels, messenger entry/caller and cached geometry, then integrate controls without runtime preference coupling or click/close persistence. Full source global/main commands/send11, general ProcessTalkType, ignore-list workflow, remaining producers/editor/charset/art/active Qt/global map/scales, discovery and independent/deployed gates remain open. PK09.21 remains IN_PROGRESS with null completion subject; whole-plan goal remains active.

## 2026-11-10 — deployed world-entry regression repaired

User-provided OldRO log/binary showed map acceptance followed by same-world0091 and a perpetual loading screen. Root traced PK05.05's terminal operation-generation latch against CModeMgr's old type/name-only reload check. Explicit same-world Switch now retires the old mode once, matching primary Mode.cpp lifecycle, and preserves new requests from OnInit. The real mode-loop regression uses production0091/CRagConnection/router/preamble and checks queued-tail resumption and stable replacement updates.

Deploy-off application build; root157/157 in38.56s; standalone470/470 in14.60s; dedicated replay PASS. Sanitized deployed excerpt, source/PE, logs and hashes are in [world-entry evidence](evidence/PK05.05-world-entry.md). Subject: `packet(PK05.05): honor same-world mode handoffs`. OldRO executable unchanged; candidate built, live retry after redeploy pending. User's deploy-ON cache setting restored after validation. Full goal remains active and PK09.21/messenger controls remain next; broad/independent/deployed acceptance is not closed.

## 2026-11-10 — messenger whisper preference controls

Implemented the actual friend-tab168 entry, independent WID68/child147 options, selected355/361/359/362 resources, mouse-down notifications and maximum state-bitmap bounds. Whole280x120 cached geometry/defaults, close snapshots, fresh-process persistence, modal/OS capture cancellation, captured child teardown and messenger/options independence match primary source. Root owns integration and selected PE checks; Luna supplied read-only source/asset and final behavior audits. Native/passive-QML stacking includes raising the messenger above an overlapping options pane.

Deploy-off all-target build, focused6/6 in14.96s, full root158/158 in195.92s, standalone470/470 in73.63s and direct actual0097 preference replay PASS. Sixteen new frames and seven declared synthetic assets are retained in [messenger preference evidence](evidence/PK09.21-friend-options.md) and provenance. Root visually inspected native/QML open frames. User's deploy-ON preference restored by configure only; OldRO unchanged and live world-entry retry remains pending. Subject: `packet(PK09.21): restore whisper preference controls`.

Root reviewed the next common-chat caller/GetTalkType/ProcessTalkType prelude and added concrete source instructions to PK09.21: source11 whisper selection, original-text/history order, unconditional47 even after parser -1, command-on/off prelude and distinct room/normal routing require implementation. Full command pipeline/ignore/producers/editor/art/Qt global/map/scales and independent/deployed acceptance remain open. PK09.21 remains IN_PROGRESS with null completion subject; the full goal stays active.

## 2026-11-10 — source chat parser reconstructed

Root reconstructed the complete source tokenizer/GetTalkType result model,292 selected no-parameter registrations (209 inserts +83 push_back,49 MsgStr aliases) and82 parameter aliases. Luna provided bounded tokenizer/PE inventories; strong review corrected artificial129/209 cutoffs, omitted.data literals, four InstantEvent exclusions, dynamic alias precedence and selected static atoi wrapping. Primary registration order/type tags cross-check all292 values. Canonical data and a provenance-checking generator accompany both generated includes.

Production auto recognition now uses this catalog; an actual replay proves earlier MsgStr544/type29 can shadow /am instead of bypassing source precedence through a whitelist. Private first-token handling shares the full tokenizer. Final deploy-off build, root158/158 in34.77s, standalone472/472 in15.21s, direct auto replay and generator check pass. [Evidence](evidence/PK09.21-chat-parser.md) and provenance retain inputs/logs/source/PE. User deploy-ON restored by configure only; OldRO unchanged. Subject: `packet(PK09.21): reconstruct source chat parsing`.

General caller/manager buffer/parser-side effects, unconditional47/whisper11 and full ProcessTalkType/prelude are still integration work; a pure parser result model is not their acceptance. Continue those next, then ignore/producers/editor/charset/font/art/global Qt/map/scales and all broad/independent/deployed gates. PK09.21 remains IN_PROGRESS with null completion subject; whole-plan goal remains active.

## 2026-11-10 — source chat caller and core dispatch wired

Actual common Enter now stores raw/duplicate history30 and changes text before validation/send, with source browsing overwrite and focus order. Main/room/private parser callers share a manager buffer and exact47/11 ownership; parser mutations precede dispatch and negative offsets retain old data. Core normal/room/modifier routing, public/party/guild/battle/private requests and shared guards/repeat/clear behavior are implemented. Sixty registered emotion routes, pending/cooldown, volume, private intercepts, head/body and novice request, stance toggle/state/target/burrow and party leave have production checks.

Root corrected a source inventory that crossed cast-call emotion cases, a send-profile aggregate insertion caught by the full name-request replay, and the intuitive-but-wrong assumption that /sit and /stand demand different poses. The virtual CPlayer override proves both toggle by state. Final deploy-off build; root160/160 in34.46s; standalone473/473 in14.19s; focused2/2 and direct replay/generator PASS. See [submission evidence](evidence/PK09.21-chat-submission.md) and provenance. Subject: `packet(PK09.21): wire source chat submission`. User deploy-ON restored by configure only; OldRO unchanged/live retry pending.

Continue23 original startup flag pairs/command-list UI, remaining source cases and ignore/D2/D4 workflow, main/room report/export and hi action, full global6 special/similarity behavior, other producers/editor/charset/font/art/global Qt/map/scales and broad/independent/deployed gates. PK09.21 remains IN_PROGRESS with null completion subject; the full goal stays active.

## 2026-11-10 — PK09.21 main/room save/report dispatcher

Baseline659592a5; selected primary client source plus static PE. Main51 now dispatches manager6 and writes separate complete normal/battle histories with source filenames, suffixes and saved notices. Room52/116 reach manager7/18; report filenames use the local character name and selected44E720 success stub creates no file. Main115 reaches selected manager17 no-op after the47 prelude. Private caller intercepts stay separate. [Evidence](evidence/PK09.21-chat-exports.md) and [contract](contracts/PK09.CHAT-STATE.md) record production Enter/room/parser/prelude/manager checks, literal file bytes, collision/no-overwrite, absent owners and directory failures.

Deploy-OFF all-target build, application160/160 in195.20s, standalone473/473 in73.49s and focused1/1 in0.29s pass. Restored deploy ON by configure only; installed OldRO unchanged and no live login/visual acceptance claimed. Subject: `packet(PK09.21): restore main and room chat exports`. Remaining work:23 startup/registry flag bindings/command-list UI, remaining ignore/pending/ack and hi150 cases, full global6 special branches, editor/charset/render and broad/independent/deployed acceptance. No discovery row or whole leaf is newly closed.

## 2026-11-10 — PK09.21 whisper-ignore request/reply integration

Baseline00900741; primary source, selected static PE and read-only hash-pinned PACKETVER20080910 server snapshot. Implemented dispatcher15..19 request frames, pending/name-before-guard/failure order and source17 post-attempt clearing of local exclusions. D1/D2/D4 now use actual source-selected pending/notice/list handlers. A later commerce D1 override/generic stub was found by production replay and removed. Preserve single overwritten name rather than FIFO, unsolicited/unknown replies, known-empty resources,24-byte names, signed/truncated list semantics and local context edits separate from slash replies. [Evidence](evidence/PK09.21-chat-ignore.md).

Deploy-OFF all-target build, focused1/1 in0.31s, application160/160 in35.37s and standalone473/473 in15.22s pass. Parser/dispatch data and plan consistency are separately checked. Restored deploy ON by configure only, installed OldRO unchanged, no live transaction or visual acceptance claimed. Subject: `packet(PK09.21): restore whisper ignore requests and replies`. Next:23 startup/registry flag VALUE bindings and command-list UI, remaining commands/hi150/global6/editor/render, broad/independent/live/deployed gates. No full leaf or discovery owner newly closed.

## 2026-11-10 — PK09.21 source startup flags and owner lifetime

Baselineb275af2d; root verified all23 actual selected PE stores and primary value/default expressions. Startup now loads/persists source effect/miss/NoCtrl options, represents missing source fields/defaults, and appends23 ordered snapshots from actual session/audio/Tip owners after port saved-audio/UI initialization. Source/modern noCtrl/fog ownership stays distinct. Repeated calls append; hidden Tip presence is true. Fixed a prior owner-clear error in AddWindowFront; real deletion now clears command/Tip pointers. [Evidence](evidence/PK09.21-command-flags.md) includes all23 default/live-value and production bridge/lifetime checks; UI inventory remains explicitly preparatory.

Deploy-OFF all-target build, focused1/1 in0.35s, application160/160 in36.08s and standalone473/473 in14.51s pass. Parser/dispatch data and plan consistency checked separately. Deploy ON restored by configure only; installed OldRO unchanged. Subject: `packet(PK09.21): initialize source command flag state`. Next: full three-tab command-list UI, selected table labels/enums/click150 review and missing actions, complete legacy option semantics, hi150/global6/editor/render/broad/independent/live/deployed gates. No full leaf or discovery owner is newly closed.

## 2026-11-10 — PK09.21 greeting dependency and shared reply/language state

Baseline1ade6fd5; primary source and selected static PE. Implemented Hi150's default/custom ordered friend attempts, parser terminal spaces, unconditional per-attempt counter/notice and captured display text. Corrected prior mode-local counter storage to one process-global signed byte with source40 cap/wrap and retained private-owner acknowledgement precedence. Implemented private charset prefix/length boundaries, semantic service flag policy and login save/suspend/restore. [Evidence](evidence/PK09.21-friend-greeting.md). Root reviewed79 command-table numeric/caption rows,17 exact key captions and96 pointer stores, correcting shopping address/Alt spacing and confirming common23-count UI behavior plus stored caption/type layout. Full UI remains to implement.

Deploy-OFF all-target/final replay builds, focused1/1 in0.32s, application160/160 in35.11s and standalone473/473 in14.45s pass. Data/plan checks recorded separately; deploy ON restored configure-only and OldRO unchanged. Subject: `packet(PK09.21): restore friend greetings and shared reply state`. Next: full three-tab command window/remaining actions, full global6/other channel charset/non-ASCII/editor/render/legacy options/broad/independent/live/deployed gates. No full leaf or discovery owner newly closed.

## 2026-11-10 — PK09.21 command toggles/composites and live options

Baseline408d7a23; primary source and selected static PE. Implemented21 direct/composite actions with modulo32 scalar subtraction, show-name booleanization, locale-gated raw byte writes, inverse noTalk notices, first-match prelude and global child47 ordering. Source BGM close/replay retains selected map filename; sound changes preserve driver mode. Real option60 refreshes live values without save/apply; creation does not overwrite commands with stale preferences. Reviewed modern NoCtrl/fog integration now uses canonical session owners, superseding their earlier temporary separation. Source action consumers/name rendering remain open. [Evidence](evidence/PK09.21-command-toggles.md).

Deploy-OFF all-target/final replay builds, focused1/1 in0.94s, application160/160 in34.27s and standalone473/473 in14.35s pass; parser/data and plan consistency pass separately. Qt-coordinate input is exercised; native checkbox branch is source reviewed but not executed by these configured suites. Restored deploy ON configure-only, OldRO unchanged. Subject: `packet(PK09.21): integrate command toggles and live options`. Continue full three-tab UI, real snow8/Tip42, remaining actions/global6/consumers/legacy options/codecs/editor/render and broad/independent/live/deployed gates. No full leaf or discovery owner newly closed.

## 2026-11-10 — PK09.21 effect command and snow lifecycle

Baseline6fa4a3ad; primary source and selected static PE. Implemented effect8 and six-byte021D, exact lowercase Xmas map gate, actual global weather/effect handle, delayed stop/reset/process ownership and actor destruction safety. Corrected prior preparatory idempotence prose from selected649AD4 JLE: >300 remaining launches again; <=300 skips. Source arithmetic wraps modulo32. Existing cloud routes and snow emitter remain; generated effect coverage now names Weather.cpp. [Evidence](evidence/PK09.21-weather-command.md). Root also resolved Tip startup default ambiguity through selected initial DWORD1 plus successful-registry-path forced zero; full Tip integration remains next.

Deploy-OFF all-target build, focused1/1 in0.90s, application160/160 in34.49s and standalone473/473 in14.37s pass. Actual actor/world/packet replay is separate from source-body controller wrap tests and visual/live acceptance. Parser/data and plan consistency checked separately; deploy ON restored configure-only and OldRO unchanged. Subject: `packet(PK09.21): restore effect command and snow lifecycle`. Continue actual Tip42 and full command window, remaining actions/global6/consumers/legacy options/codecs/editor/render/broad/independent/live/deployed gates. No full step or discovery owner newly closed.

## 2026-11-10 — PK09.21 Tip data and selected startup dependency

Baselineef782302. Implemented actual CTipOfTheDay resource/CRT owner, source OS-language folder policy, LF/#/NUL/raw-byte/lazy/append/index behavior and explicit safe undefined-tail/empty-index dispositions. ReadRegistry's existing INI adapter now preserves selected successful-store-open forced OFF; SaveSourceTipOptions persists the raw immediate global checkbox value. Startup policy is deliberately distinct from persisted ON. [Evidence](evidence/PK09.21-tip-data.md) retains selected UI175/176/nine bitmap/folder data and bounded38-byte/seven-row viewer leads for the next full Tip window.

Deploy-OFF all-target build, focused1/1 in0.89s, application160/160 in36.29s and standalone473/473 in15.42s pass; parser/data and plan consistency pass separately. Deploy ON restored configure-only; OldRO unchanged. Subject: `packet(PK09.21): restore source tip data and startup options`. Actual Tip42/factory38/once guard/viewer/navigation/native/passive Qt UI remains missing; no generic placeholder is added. Continue that dependency, full command window, remaining actions/global6/consumers/legacy options/codec/editor/render and broad/independent/live/deployed gates. No full step or discovery owner newly closed.

## 2026-11-10 — PK09.21 actual Tip window and source controls

Baselinef3f63540. Implemented WID38/command42 with actual source loader/viewer/buttons/checkbox/startup once ownership, navigation/wrap/reset, source byte/color rows and native/passive Qt input/render lifetime. Root PE corrected preparatory dimensions to height125/six/page5 and confirmed default Enter180/cancel117; MSVC sentinel insertion appends and late checkbox wins long-label overlap. Main-chat focus return and captured-owner teardown are covered. [Evidence](evidence/PK09.21-tip-window.md) retains eight native/Qt frame pairs from declared source-key synthetic assets. No original executable was run.

Deploy-OFF all-target build, focused2/2 in2.77s, application161/161 in35.04s and standalone473/473 in13.87s pass; zero QML errors; data/plan checks separately verified. Deploy ON restored configure-only; OldRO unchanged. Subject: `packet(PK09.21): restore Tip window and source controls`. Continue full command window; rich Tip item/charset/subclass/CR/skin/font/locale and remaining actions/global6/consumers/legacy options/codec/editor/render/broad/independent/live/deployed scope remain open. No full step or discovery owner newly closed.


## 2026-11-10 — PK09.21 command table seed and root route corrections

Baselineb11ee593. Added immutable selected-byte tables with strict generator validation of79 numeric stores and96 caption-pointer bindings. Generator/negative provenance checks and C++17 header syntax compilation pass. Root selected-PE review resolves Merchant87-to-Up38, six advertised no-op rows, DetailLevel screen-state behavior, VK121/F10 misidentification and undefined derived offset+78. [Evidence and continuation](evidence/PK09.21-command-table-seed.md) retains root route data and explicitly preparatory hover/action inventories. Full UI/controller/help/action-recipient/native/passive Qt integration remains open; no broad or leaf acceptance claimed. No application rebuild/deployment; user preference and OldRO hash unchanged.


## 2026-11-10 — PK09.21 command request/turn actions

Baseline1a8d246f. Root implemented8 missing action-table handlers through actual mode recipients/packet sends: WHO/MEMO/BLACKSMITH/ALCHEMIST/TAEKWON/HUNTING, BANGBANG and PVPINFO. Selected PE resolves PVP account+character IDs and direction raw WORD/BYTE fields/signed remainder; source WHO service identity differs in numeric enum representation. [Evidence and continuation](evidence/PK09.21-command-actions.md). Focused1/1 in0.84s, application161/161 in37.10s and standalone473/473 in15.60s pass after deploy-OFF all-target/standalone builds. Deploy ON restored configure-only; OldRO unchanged. HELP14/WHERE9 remain among22 action handlers; full command UI/recipients/help balloons/native-passive Qt and broader plan remain open. Preparatory help inventory now retains70 ordered rows plus branch-specific random tail; do not emit all74 or consume the wrong number of random draws. No full step or discovery owner newly closed.


## 2026-11-10 — PK09.21 source HELP/WHERE and map-name owner

Baseline4537f651. Root implements actual HELP14 and WHERE9 paths and session-owned raw source map-name loader/query. Selected PE resolves74 MsgStr pushes/call targets,70-row order/branch-specific shared RNG, X/Z/integer-half/floor, @ prefix surgery, comment//, fallback187 and archive-aware file existence. All22 action-table types now have handlers at declared scope; full command UI/recipients/help balloons/native-passive Qt and broader aliases/text/producer acceptance remain open. [Evidence and continuation](evidence/PK09.21-help-where.md). Deploy-OFF all-target/standalone builds; focused1/1 in1.15s, application161/161 in35.67s and standalone473/473 in15.16s pass. Deploy ON restored configure-only; OldRO unchanged. No full step or discovery owner newly closed.

## 2026-11-10: command controller and shared help owner

Restored factory112/controller/native/passive Qt and mode-owned per-frame help; reviewed shorter-table UB dispositions, source scrollbar/hover/frame contracts, party permission and actual owner retirement. Focused2/2 in2.50s, complete application162/162 in39.82s and standalone473/473 in16.33s pass. Six capture pairs retained. Three generators and world-entry regression pass. Deployment remains read-only; user preference restored ON by configure only. Next real recipients68/74/87, full party positive-confirm and remaining full-plan gates. Evidence: `evidence/PK09.21-command-controller.md`. Subject: `packet(PK09.21): restore command controller and shared help owner`.

## 2026-11-10: real pet request prerequisites

Implemented source mode158/163 senders01A5/26 and01A1/3, preserving raw name truncation/NUL and low-byte action semantics with zeroed undefined stack padding. Actual peer tests cover short/empty/boundary/overlong/DBCS/embedded-NUL names, actions0..4 and truncation, null rejection, no local mutation and disconnected failures. Selected PE and hash-matched server parser/helper evidence retained. Application162/162 in35.36s and standalone473/473 in15.13s pass. Actual PetInfo74 UI/state/receive/lifetime and whole PK27/PK09.21/plan remain open. Evidence: `evidence/PK27-pet-control-requests.md`. Subject: `packet(PK27): restore source pet request recipients`.

## 2026-11-10: pet live receive and accessory resources

Restored source runtime widths/history/identity,01A4 subtypes0..5 with actual world lookup/state0/resource loader, starvation predicates, static PE-backed raw accessory ACT mapping and balanced override locks. Equipment formats00A4/02D0 preserve last eligible wire row, identification independence and stale index behavior; generic inventory mutations do not derive/reset it. Alternate property summary remains explicitly pending and preserves normal state. Dedicated replay passes; all application163/163 in35.22s and fresh standalone473/473 pass. Actual PetInfo controller and whole PK27/PK09.21/packet plan remain open. Evidence: `evidence/PK27-pet-live-state.md`. Subject: `packet(PK27): restore pet state and accessory reception`.

## 2026-11-10: actual PetInfo controller and presentation

Restored actual factory88/PetInfo74, source job gate/geometry, raw-byte rename boundaries and managed confirmation, popup feed/performance/accessory/return actions and exact remembered egg cleanup even on failed immediate sends. Shared modal ownership and native/passive Qt/input/capture lifetimes are covered. Root visual inspection found direct DIB alpha blending ignored DC clips; rectangular clips now intersect, empty clips no-op and complex/mapped DCs use GDI. Portrait composition preserves source ACT/SPR mapping and native premultiplied RGBA. Deploy-OFF all-target and standalone builds pass; focused2/2 in2.99s, application164/164 in196.95s and standalone473/473 in75.32s. Seven native/Qt pet frame pairs, source portrait/label proofs and generator/plan checks are retained in [current PetInfo evidence](evidence/PK27-pet-window.md) and its adjacent provenance/logs. Deploy ON restored configure-only; OldRO unchanged. Full PK27/PK09.21 and all broader gates remain open. Next alternate signed01A2 summary must extend existing port UIItemInfoWnd (source factory12), preserving separate illustration preview; then mode26 menu and remaining PK27 lifecycle,68/87 and full-plan gates. Subject: `packet(PK27): restore actual pet information window`.

## 2026-11-10: alternate pet property shared detail recipient

Restored signed01A2 alternate path in existing source item-detail owner, preserving normal pet state and copied item/art. Generated selected-PE hardcoded CP949 labels/state helpers and explicit UTF8 native/passive Qt projection, with safe codepoint wrapping; ordinary selection resets sentinel. Root reviewed source ownership, snapshots, text fields and lifetimes; actual replay covers signed boundary/width/band/name/reuse/item transitions, including DBCS. Deploy-OFF all-target and standalone builds pass; focused2/2 in9.73s, application164/164 in198.60s and standalone473/473 in75.45s. Ten native/Qt frame pairs include three alternate-summary states. Separate Windows non-Qt UTF8 drawing/wrapping/color/clip check passes. Generator/source and plan checks pass separately. Deployment ON restored configure-only; OldRO unchanged. Full PK27/PK09.21 and broader gates remain open. Next actual mode26 pet context-menu producers/IDs29..33, remaining capture/feed/egg/rename/follow/reset, command68/87 and full-plan gates. Evidence: `evidence/PK27-pet-summary.md`. Subject: `packet(PK27): restore alternate property summary recipient`.

## 2026-11-10: source owned-pet actor menu

Restored the picked actor producer, primary native menu18 behavior and mode26 selectors [33,29,30,32,31], preserving requests0/1/2/4/3, confirmations and exact local return cleanup even on failed sends. Root PE/source review and visual inspection caught/corrected a hidden Qt popup plane; an actual visible-pixel assertion prevents geometry-only acceptance. Deploy-OFF all-target/standalone builds pass; focused2/2 in4.27s, application164/164 in200.57s and standalone473/473 in74.87s. Thirteen native/Qt frame pairs and zero QML warnings cover the previous pet owners plus actor menus and ownerless feed confirmation. Selected-PE selector/MsgStr order and source/plan checks pass. Deploy ON restored configure-only; OldRO unchanged. PK27/PK09.21 and the whole plan remain open. Next capture019E/01A0, feed01A3, egg01A6/01A7 and actor action/dialogue01AA. Source correction:01AA is not rename response; rename/property receipts use01A2 and request01A5. Full picker/global stacking/font/skin/input and all broader gates remain open. Evidence: `evidence/PK27-pet-context-menu.md`. Subject: `packet(PK27): restore owned pet actor context menu`.

## 2026-11-10: pet egg list and snapshot selector

Restored01A6/source factory90/mode159/01A7 with actual native/passive Qt. Root PE review corrects earlier clear/reset and generic-key assumptions:75 is ignored, repeats append snapshots/duplicates and retain selection/offset/position; Enter/Escape do not submit/cancel and double-click confirms the old selected row. Selection restores the copied item, marks damaged/eggIndex and closes even if send fails; no derived pet job. Deploy-OFF all-target/standalone builds pass; focused2/2 in5.70s, application164/164 in201.47s and standalone473/473 in73.97s. Eighteen native/Qt frame pairs and zero QML warnings include five egg-selector states, source scrollbar/drag/cancel capture and actual125% OK input. Selected-PE/source and plan checks pass separately. Deploy ON restored configure-only; OldRO unchanged. PK27/PK09.21 and the whole plan remain open. Next complete capture019E→skill10000→actor41/90→player42→mode76/factory91→click mode162/019F→01A0, plus feeding01A3/actor action-dialogue01AA and remaining lifetimes. Mode160 is01A8, not capture. Preserve all prior proofs and broader gates. Evidence: `evidence/PK27-pet-egg-selector.md`. Subject: `packet(PK27): restore source pet egg selector`.

## 2026-11-10: capture request sender and strong-agent source review

**Capture request checkpoint (2026-11-10):** [Mode162/019F sender and strong-agent continuation](evidence/PK27-pet-capture-request.md) adds the exact6-byte request and socket/failure replay. All-target deploy-OFF build and application164/164 in50.89s pass. Selected-PE review resolves SlotMachine assets and animation boundary/timer traps. 019E targeting/approach, factory91,01A0 result, animation and reset integration remain open. DeployON restored configure-only; OldRO unchanged. PK27/PK09.21 and the whole plan remain open. Subject: `packet(PK27): add source pet capture request sender`. Previous egg-selector commit08dcec82 is clean.

## 2026-11-10: source pet taming owner and result

**Current pet taming checkpoint (2026-11-10):** [Factory91/mode76/01A0 owner](evidence/PK27-pet-taming.md) implements source one-attempt click, repeated owner/result semantics, native/passive Qt sprite projection and source frame/timer boundaries. Deploy-OFF all-target and standalone builds pass; focused2/2 in6.40s, application164/164 in53.44s (-j4) and standalone473/473 in16.17s (-j8). Twenty-two native/Qt frame pairs and zero QML warnings include four taming states. DeployON restored configure-only; OldRO unchanged. PK27/PK09.21 and the whole plan remain open. The next strong-agent work is019E arming, primary mouse picking/consumption and approach; server-driven capture is not yet complete. Subject: `packet(PK27): restore source pet taming window and result`.

## 2026-11-10: primary capture targeting and approach

**Current capture targeting checkpoint (2026-11-10):** [Primary019E capture loop](evidence/PK27-pet-capture-target.md) connects source level0 arm, actor predicates, approach and mode76 to the existing first-click162/019F/01A0 owner. Real socket and Vulkan billboard/picker/WindowProc evidence pass; NPC continuous turn/contact and primary source decisions are retained. Deploy-OFF all-target and standalone builds pass; application164/164 in51.90s (-j4) and standalone473/473 in14.33s (-j8). Twenty-two native/Qt frame pairs and zero QML warnings pass, plus real Vulkan texture/picker/WindowProc capture input. DeployON restored configure-only; OldRO unchanged. PK27/PK09.21 and the whole plan remain open. Shared proceed interruption/reentrancy, companion identity producers/AI and broader picker/cursor/lock gates remain strong-agent work. Subject: `packet(PK27): connect source pet capture targeting and approach`.

## 2026-11-10: shared player skill proceed and fallback

**Current shared player skill checkpoint (2026-11-10):** [PT_SKILL ownership and mode76 fallback](evidence/PK27-player-proceed.md) preserves old proceeds after rejected41 while90 overwrites level, dispatches through42/76/13 and restores temporary0147 storage. Deploy-OFF all-target and standalone builds pass; focused2/2 in7.01s, application164/164 in52.31s (-j4) and standalone473/473 in14.00s (-j8). Twenty-two native/Qt frame pairs and real Vulkan capture input pass. DeployON restored configure-only; OldRO unchanged. PK27/PK09.21 and the full plan remain open. General mode126, shortcut/list arming, attack/move/ground/touch lifetimes, companion identity/AI and broader picker/cursor/lock gates remain strong-agent work. Subject: `packet(PK27): preserve shared player skill proceed semantics`.

## 2026-11-10: automatic skill selectors and level data

**Current automatic skill checkpoint (2026-11-10):** [0147/126/79 selectors and level policy](evidence/PK27-autorun.md) replaces identify-only dispatch with exact source types/canonical self targets and repairs numeric SP-table tokens. Deploy-OFF all-target and standalone builds pass; focused2/2 in7.20s, application164/164 in53.46s (-j4), standalone473/473 in15.87s (-j8). Twenty-two native/Qt frame pairs and real Vulkan capture input pass. DeployON restored configure-only; OldRO unchanged. PK27/PK09.21 and the full plan remain open. General pending mouse/cursor/actor/ground/touch consumption, shortcut/list producers and broader interruption/companion/picker lifetimes remain strong-agent work. Subject: `packet(PK27): route automatic skills through source selectors`.

## 2026-11-10: canonical touch-skill action dispatch

**Current touch-skill checkpoint (2026-11-10):** [Canonical103 and virtual29/150](evidence/PK27-touch-proceed.md) restores source action dispatch and corrects a false base-no-op verifier assumption. Deploy-OFF all-target and standalone builds pass; focused2/2 in6.87s, application164/164 in51.51s (-j4), standalone473/473 in12.76s (-j8). Twenty-two native/Qt frame pairs and real Vulkan capture input pass. DeployON restored configure-only; OldRO unchanged. PK27/PK09.21 and the full plan remain open. General pending input, shortcut/list producers, ground65/66/77 and broader companion/picker/lifetime gates remain strong-agent work. Subject: `packet(PK27): restore canonical touch skill action dispatch`.

## 2026-11-10: source pending actor input and signed weight

**Current source pending-input checkpoint (2026-11-10):** [Source actor selectors and weight state](evidence/PK27-source-pending.md) connects stored selector/range/raw levels to actor/unit input and restores signed00B0 weight state/notices. Deploy-OFF all-target and standalone builds pass; focused3/3 in14.78s, application164/164 in53.56s (-j4), standalone473/473 in12.71s (-j8). Twenty-two native/Qt frame pairs and real Vulkan capture plus temporary-skill world input pass. DeployON restored configure-only; OldRO unchanged. PK27/PK09.21 and the full plan remain open. Next migrate legacy shortcut/list producers and remove the positive-level pending fallback, then ground65/66/77/text owners and broader companion/cursor/lifetime gates. Subject: `packet(PK27): connect source pending actor input and weight state`.

## 2026-11-10: source skill activation and saved levels

**Current source skill activation checkpoint (2026-11-10):** [Source UI producers and saved levels](evidence/PK27-skill-producers.md) restores backend list/shortcut126 dispatch, per-character raw levels and strict selector ownership. Skill tree window/model/catalog/QML/geometry are unchanged. Deploy-OFF all-target and standalone builds pass; focused3/3 in16.75s, application164/164 in56.44s (-j4), standalone473/473 in12.78s (-j8). Twenty-two native/Qt frame pairs and real Vulkan capture/pending input pass. DeployON restored configure-only; OldRO unchanged. PK27/PK09.21 and the full plan remain open. Next canonical ground65/66/77 and125/220 text owners, then broader companion/control/lifetime gates. Subject: `packet(PK27): restore source skill activation and saved levels`.

## 2026-11-10: canonical ground proceed and text owners

**Current ground proceed checkpoint (2026-11-10):** [Canonical65/66/77 and text owners](evidence/PK27-ground-proceed.md) replaces legacy ground transport, preserves raw levels/cells and adds source125/220 prompts84/101 through144. Skill tree unchanged. Deploy-OFF all-target and standalone builds pass; focused3/3 in17.85s, application164/164 in56.92s (-j4), standalone473/473 in13.17s (-j8). Twenty-two prior native/Qt frame pairs, two added native text-prompt frames and real Vulkan ground/capture/pending input pass. DeployON restored configure-only; OldRO unchanged. PK27/PK09.21 and the full plan remain open. Next text color-chip/keyboard/drag/focus/lifetimes, companion/control/picker and remaining plan gates. Subject: `packet(PK27): restore canonical ground proceed and text owners`.

## 2026-11-10 Ground prompt input and palette

**Current ground prompt checkpoint (2026-11-10)**. Dragging, pointer release, typing and available palette sampling passed native replays. All 164 application checks and 473 standalone checks passed. The focused suite passed 3 checks in 18.69s, with native input and two palette renders. The full packet plan remains active. [Evidence](evidence/PK27-prompt-input.md)

## 2026-11-10 Ground prompt character input

**Current ground prompt text checkpoint (2026-11-10)**. Native and Qt text now reach the focused ground prompt. All 164 application checks and 473 standalone checks passed. The focused suite passed 3 checks in 16.99s. Live IME, selection, and source packet encoding remain open. [Evidence](evidence/PK27-prompt-text.md)

## 2026-11-10 Ground prompt service encoding

**Current ground prompt encoding checkpoint (2026-11-10)**. Ground text now uses the service codepage on the wire and UTF-8 in the editor and skill bubble. All 164 application checks and 473 standalone checks passed. The focused suite passed 3 checks in 17.03s. [Evidence](evidence/PK27-prompt-codec.md)

## 2026-11-10 Ground prompt selection

**Current ground prompt selection checkpoint (2026-11-10)**. Talkie Box and Graffiti now support caret navigation, Shift selection, and replacement by typed text or an IME commit. All 164 application checks and 473 standalone checks passed. The focused suite passed all 3 checks. [Evidence](evidence/PK27-prompt-selection.md)

## 2026-11-10 Ground prompt clipboard

**Current ground prompt clipboard checkpoint (2026-11-10)**. Talkie Box and Graffiti now route the original clipboard shortcuts through the selected service codepage. All 164 application checks, 473 standalone checks, and 3 focused checks passed. [Evidence](evidence/PK27-prompt-clipboard.md)
