# Packet continuation handoff

**Active step: PK23.05.** PK14.04 now has integrated source/state/caller and actual native/QML pixel acceptance: [report](evidence/PK14.04-rendered-acceptance.md), [provenance](evidence/PK14.04-rendered-acceptance.json), [canonical 0x0107 contract](contracts/PK14.PARTY-POSITION.md). Dedicated commit subject: `packet(PK14.04): verify native and Qt party marker rendering`. PK14.01–PK14.07 are complete at their selected-source scopes; independent PK48/PK49 and deployed-server provenance remain open.

## Continue here

1. [PK23.05](work-items/PK23.05.md): implement exact cast-ACK message-99 announcements using the actual Ref `Zc_useskill_ack` branch and existing runtime actor fixture. Regular non-monsters pass `(skillId, sourceActor, 0)`, except 334/335 pass destination AID. Keep 57/62/monster exclusions and the reviewed local instant deferral/rejection. Verify actual bubble text/expiry as well as message arguments. Concrete source/test instructions are now in the work item; no generic stronger review is pending.
2. Continue [PK09.04](work-items/PK09.04.md) broader item field/variant/UI/lifecycle acceptance. The selected cart WearState/bind omission is already fixed (`67096e78`) with a guarded header probe and literal production decoder checks; do not repeat it or treat raw bind word as boolean ownership.
3. Resume bounded PK46.02 per-request source/profile/UI/send-failure work, then its remaining character paths. The 177 request rows remain open research. Independent source-ready NPC/status/mail leaves can proceed when a specific technical dependency blocks another leaf; use their concrete contracts, not old generic review/dirty-tree gates.

## Verified current checkpoint

All four targets build with developer deployment OFF: application, packet replay, native/QML render checks and standalone party-state checks. Integrated CTest passed **3/3 in 7.22 seconds**, preserving **159 PASS packet records** and verifying **18 real native/QML rendered scenes with zero QML warnings/errors**. Standalone state assertions are explicitly active for Release (`-UNDEBUG`). The three final contact sheets were visually inspected. Exact source/blob, binary, font/plugin and image hashes are retained in PK14.04 evidence.

Rendering exposed and repaired native packed-RGB/COLORREF channel reversal and native dot bleed beyond the map rectangle. Native OnProcess/DrawToHdc and the production Qt adapter/image builder/GameOverlay repeater are exercised, with synthetic map art and real world attribute bounds. This is not a token-only or invented-circle preview. Only AUDIT-0x0107 is newly classified APPLICABLE_AND_FULLY_IMPLEMENTED_VERIFIED; its contract status remains IMPLEMENTED_PENDING_REVIEW for the later independent pass. Native/Qt chrome/player glyph styles remain distinct existing designs, not a skin-parity claim.

Earlier commits remain authoritative scoped proofs: `fde621a1` restores the reproducible roster closure, `d3ccc48c` fixes terminal receive/update behavior, `803458e7` verifies actual map/warp/reconnect/character callers, and `67096e78` fixes selected cart tail fields. Older dirty-tree snapshots and missing-render statements are historical; use current evidence for current binaries.

## Working discipline

One coherent leaf and its production regression/evidence/commit at a time. Preserve existing history and unrelated changes. Keep searches on exact symbols and clip long/minified lines before returning tool output. Source disposition, implementation, headless replay, rendered UI and live/deployed acceptance are separate claims. Unknown deployed settings do not block testing the source-selected 20080910 profile.

The offline tests use localhost and synthetic assets; request tool escalation when sandbox access prevents required loopback/offscreen GUI checks. The render test derives the matching kit's qoffscreen path because app deployment contains qwindows only; do not change installed client deployment to fix the test. No installed-client launch/copy, live account transaction, external server patch/build/restart or hostile server replay belongs to these checks. Scoped external source security findings remain documented separately.

Maintain this handoff, STATUS and state.json active_step together. Append diaries to PROGRESS/evidence. Do not mark full PK48/PK49 or the active whole-plan goal complete from a package checkpoint.
