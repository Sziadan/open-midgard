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
