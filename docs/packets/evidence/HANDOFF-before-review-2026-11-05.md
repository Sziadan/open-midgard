# Packet continuation handoff

## Execute in this order
1. Continue [PK46.02](../evidence/PK46.02-c2s-map-request-inventory.csv) IN_PROGRESS; all 177 rows remain UNREVIEWED pending primary-owner profile/raw-buffer/UI/replay gates. PacketVer23 0x00F5/0x0438/0x0439 requests now have production sender replays; 0x0437 remains PK50-owned, and acknowledgement/result and legacy-profile gates remain with PK38.
2. Keep [PK46.03](../evidence/PK46.03-login-character-crosswalk.md) IN_PROGRESS for delete-layout, SSO/security, unsupported-action and PK44/PK48-owned 0x08D4 gates; its combined replay passes 26 login cases but does not cover these boundaries.
3. Keep [PK05.05](../work-items/PK05.05.md) IN_PROGRESS; source-path recheck confirms the need for a designed production-used acknowledgement transition signal.
4. Keep [PK14.04](../work-items/PK14.04.md) IN_PROGRESS; fresh build/replay passed, but rendered native/Qt and production lifecycle gates remain pending.
5. Keep [PK14.06](../work-items/PK14.06.md) IN_PROGRESS for R3 lifecycle repair; source-path recheck is recorded, but transition callers still need behavioral replay coverage.
6. Keep [PK46.04](../evidence/PK46.04-bounded-self-review.md) IN_PROGRESS; eight PK40-owned auction requests remain owner-gated.

Read [the review and continuation rules](../evidence/REVIEW-2026-11-04.md) before coding. They record R1-R3, production paths, validation limits and file-preservation rules. Do one leaf and one owned commit at a time. Strong independent review remains PK48; no new prior approval gate is introduced.

## Baseline and preservation

Review baseline: HEAD `2a9ba7982bc6899ba62720989ef0799e77d321eb`, dirty tree. Re-read current status/diffs because work may have advanced. Existing runtime/replay changes, coverage annotations and numerous uncommitted PK45/PK46 evidence notes are not disposable. Do not revert or stage unrelated work. Inspect existing replay changes and retain cases; additive regressions or a separate focused target are permitted. Unresolvable ownership blocks only the conflicting edit, not all independent verification.

Historical PK05.01–.04 and PK14.02/.03 checkpoints remain recorded; they do not certify repaired integration. PK14.03's machine state has been reconciled to its existing `3ff44a3e` commit and evidence. R1–R3 remain open in new permanent leaves. No discovery classification was changed by this review plan.

## Verified environment and commands

The review rebuilt the current dirty app and replay successfully with developer deployment OFF. The focused replay passed 1/1 in 3.12 seconds with local-loopback permission. The previous blanket Windows SDK/build blocker does not describe this verified MinGW environment. Obtain fresh results for every changed binary.

```powershell
cmake --preset mingw-qt-x64 -DRO_ENABLE_DEV_DEPLOY=OFF -DRO_BUILD_PACKET_REPLAY_TESTS=ON
# Confirm RO_ENABLE_DEV_DEPLOY:BOOL=OFF in build-mingw-preset/CMakeCache.txt.
cmake --build --preset build-mingw-qt-x64-ci --target packet_stream_replay_checks open_midgard -j 6
ctest --test-dir build-mingw-preset -R '^packet_stream_replay_checks$' --output-on-failure
python tools/check_packet_plan.py
```

If loopback is denied by the sandbox, use the tool's escalation request for this local mock-server test. Do not disable cases or use a live server. Never deploy to or run the installed OldRO client, transact on live accounts, modify/restart rAthena, or kill the user's client for these checks.

## Remaining work and history

All unrelated technical gaps remain open in state.json/BLOCKED and their evidence. In particular, broader Login/GameMode/Qt lifecycle, selected-profile questions, actor/status/resources, commerce/mail/quest features, PK45–PK47 coverage, deployed-server provenance and PK48/PK49 acceptance are not closed. A source trace is not feature implementation; a helper assertion is not proof of its production caller.

Prior handoff and status text is preserved verbatim in [handoff history](../evidence/HANDOFF-before-review-2026-11-04.md) and [status history](../evidence/STATUS-before-review-2026-11-04.md). Those documents contain superseded instructions and must not select the next active step. Append detailed diaries to PROGRESS or evidence, then replace this current summary rather than adding competing continuation paragraphs.
