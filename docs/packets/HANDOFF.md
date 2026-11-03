# Resume packet implementation

Next action: **PK00.02 (S)**. Read `../PACKET_IMPLEMENTATION_PLAN.md`, this file, `STATUS.md`, and `state.json` first. Do not start protocol edits until the baseline is reproducible.

Completed checkpoint: PK00.01 at baseline `af41f6484cb7ff31825296babe4b913aa36c4c67` (`main`, tracking `origin/main`, clean tree). Evidence: `evidence/PK00.01.md`. The old handoff's `9c158616` HEAD and approximately 80 dirty/untracked paths no longer describe this checkout. The current runtime commit already contains the guild/commerce changes previously noted as dirty.

PK00.02 must inventory tracked and ignored runtime dependencies read-only. Preserve ignored inputs, especially `Ref/`, `tools/index_skill_packet_emitters.py`, `tools/patches/rathena-wait-for-complete-packet.patch`, `tools/GuildSkillWndRefreshTests.cpp`, build directories, and installed/runtime artifacts. Their relevance/ownership is not yet established. Do not broadly stage, reset, clean, deploy, or run the installed client.

Effective server source baseline in the audit is PACKETVER/PACKETVER_SAK_NUM `20080910`, source `C:/Users/sziad/source/repos/rathena-master`; client default profile 23. Reconfirm later. Running server binary provenance is not established.

After PK00, follow PK01→PK02→PK03, then framing repairs PK04–PK09. PK50 quests is scheduled by dependencies. Server-side trade correction in PK13 requires separate explicit authorization; client-only timeout is not the fix.

Validate plan/tracker with `python tools/check_packet_plan.py`. Before builds use the documented MinGW Qt preset with `-DRO_ENABLE_DEV_DEPLOY=OFF` and verify the cache. Focused replay commands must be established in PK03. No automatic deployment/server restart or live account transactions.

After each completed leaf, commit its evidence, state/STATUS, progress and handoff with only explicit owned paths. Report the leaf, concrete result, verification limits, commit SHA, and next exact step.
