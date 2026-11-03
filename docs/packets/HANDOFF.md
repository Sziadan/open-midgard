# Resume packet implementation

Next action: **PK01.04 independent verification**. Read `../PACKET_IMPLEMENTATION_PLAN.md`, this file, `STATUS.md`, `state.json`, `BLOCKED.md`, `contracts/INDEX.md`, and `evidence/PK01.03-applicability-approval.md`. Recheck effective configuration, final assignment order, the P0 identities, and all 990 ownership rows. Applicability is approved; semantic implementation remains open.

Completed checkpoint: PK00.01 at baseline `af41f6484cb7ff31825296babe4b913aa36c4c67` (`main`, tracking `origin/main`, clean tree). Evidence: `evidence/PK00.01.md`. The old handoff's `9c158616` HEAD and approximately 80 dirty/untracked paths no longer describe this checkout. The current runtime commit already contains the guild/commerce changes previously noted as dirty.

PK00.02 completed read-only inventory at HEAD `8c81549bb9d4bf89ac59f53b99827ce07ccc3377`; see `evidence/PK00.02.md` and `evidence/PK00.02-manifest.json`. It records hashes for all tracked files and ignored source inputs, plus path/size metadata for all Ref GRF assets. Preserve ignored inputs; do not broadly stage, reset, clean, deploy, or run the installed client. Existing build caches are inconsistent; only `build-mingw-preset` currently shows `RO_ENABLE_DEV_DEPLOY=OFF`, and its output is not fresh evidence.

PK00.03 selected runtime commit `af41f6484cb7ff31825296babe4b913aa36c4c67` already present at `origin/main`; PK00.04 clean-built it using `build-mingw-preset` with `RO_ENABLE_DEV_DEPLOY=OFF`. PK01.01 specified research structure. PK01.02 reproduced effective 20080910 config and the final server registry from hash-pinned packet sources; evidence is in `evidence/PK01.02.md`. The server source folder has no `.git`, so its source commit/origin and deployed-server provenance remain unknown. PK01.03 applicability contracts were approved after independent strong review; downstream implementation contracts and deployed-server parity remain open.

Effective server source baseline in the audit is PACKETVER/PACKETVER_SAK_NUM `20080910`, source `C:/Users/sziad/source/repos/rathena-master`; client default profile 23. Reconfirm later. Running server binary provenance is not established.

After PK00, follow PK01→PK02→PK03, then framing repairs PK04–PK09. PK50 quests is scheduled by dependencies. Server-side trade correction in PK13 requires separate explicit authorization; client-only timeout is not the fix.

Validate plan/tracker with `python tools/check_packet_plan.py`. Before builds use the documented MinGW Qt preset with `-DRO_ENABLE_DEV_DEPLOY=OFF` and verify the cache. Focused replay commands must be established in PK03. No automatic deployment/server restart or live account transactions.

After each completed leaf, commit its evidence, state/STATUS, progress and handoff with only explicit owned paths. Report the leaf, concrete result, verification limits, commit SHA, and next exact step.
