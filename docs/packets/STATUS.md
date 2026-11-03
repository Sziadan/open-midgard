# Packet implementation status

Planning checkpoint prepared 2026-11-03. **No runtime work package is certified complete.** PK00.01 completed a research-only baseline ownership checkpoint; no runtime code is certified.

- Canonical task state: state.json. Current active step: PK02.04.
- PK02.01 through PK02.04 are complete; next: PK03.01 specify production replay and test harness scope.
- PK00.02 inventoried 1,198 tracked files with hashes and ignored references/assets; the authoritative external rAthena checkout was inaccessible during that checkpoint. It became readable for PK01.02, which recorded source hashes. See `evidence/PK00.02.md`, `evidence/PK00.02-manifest.json`, and `evidence/PK01.02.md`.
- PK00.03 selected runtime commit `af41f6484cb7ff31825296babe4b913aa36c4c67`, already at `origin/main`; only packet-plan documentation differs in the working branch. Exact deploy-off build instructions are recorded in `evidence/PK00.03.md`.
- PK00.04 clean-built the client with `RO_ENABLE_DEV_DEPLOY=OFF`; see `evidence/PK00.04.md` for commands, fresh binary hash, and warnings. This does not certify packet behavior.
- PK01.01 specified source inputs, per-contract output, independent byte-oracle schema and review gates; it approved research structure only. PK01.02 reproduced effective `PACKETVER=20080910` and the 603-entry final registry from the readable source snapshot; its source hashes, raw assignments, final table, and macro report are in `evidence/PK01.02*`. The snapshot lacks Git metadata, and deployed-server binary provenance is still unknown.
- PK01.03 completed strong-reviewed applicability for seven contracts and recorded one open historical variant. Contracts and limits are in `contracts/INDEX.md` and `evidence/PK01.03-applicability-approval.md`. Semantic implementation contracts, historical reachability, fixtures, and deployed-server provenance remain open; no runtime implementation is certified.
- All21 findings, all19 ignored registrations and all990 discovery rows have research owners. Ownership is not semantic validation.
- Work package count:51; numbered steps are in WORK_PACKAGES.md. Expand broad family/review steps into permanent per-contract child IDs before coding.
- S tasks require strong agents; M tasks require approved contracts/review; L candidates are explicitly identified inside cards.
- Every completed step requires a separate commit and user progress update. Update this file, state.json, PROGRESS and HANDOFF in that commit.
