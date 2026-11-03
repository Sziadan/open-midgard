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

## 2026-11-03 — PK01.03 review checkpoint

Applied the canonical identity key to P01–P05 and documented candidate splits: raw character account-ID handshake, cross-cutting connection framing, map S2C 0x01F2 and 0x01A2, and separate map C2S 0x0190/0x007E layouts. The exact strong-review decisions are recorded in `evidence/PK01.03-review-needed.md`. This step remains BLOCKED: candidate identities are not approved contracts, no permanent child IDs were allocated, and no dependent implementation may start. Stable subject: `packet(PK01.03): record P0 contract review requirements`. Next: strong protocol review, then finish PK01.03 and PK01.04.
