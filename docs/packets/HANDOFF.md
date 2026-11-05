# Packet continuation handoff

**Active step: PK09.04.** Latest checkpoint: [PK37.06 commerce item details](evidence/PK37.06-item-detail-contexts.md), [contract](contracts/PK37.ITEM-DETAIL-CONTEXTS.md), [provenance](evidence/PK37.06-item-detail-contexts.json). Dedicated commit: `packet(PK37.06): isolate commerce item detail contexts`. Prior rental, special/card/ordinary names, CID lookup and cart ownership checkpoints remain passing.

## Continue here

1. Continue [PK09.04](work-items/PK09.04.md) with visible inventory type/tab/radio behavior, using [selected source/DB evidence](evidence/PK09.04-selected-item-types.json). RoCommerce is the visible inventory; its ad hoc type groups differ from primary/native Sort. Stock Cash18 is emitted unchanged and currently lands in Other instead of Usable. DelayConsume11 becomes2 before emission, pet eggs7 become4 and shadowgear12 become4/5. Unify broad classification only after preserving primary groups; source radio branches still need actual UI and lifecycle acceptance. Do not infer raw7/11/12 selected emission from DB labels. The stock DB audit is hash-recorded source evidence, not deployed provenance.
   PK37.06 fixes vending/cart fallback and restores source-backed merchant/trade/viewed-equipment detail entry points. Preserve explicit source selection, remote row ordinals, required expected IDs, snapshot versus live ownership, tooltip-only overlay stacking and125% rendering. PK33.05 rental/0298/0299/00AF behavior remains passing. Full detail placement needs its own review: MakeWindow currently shows a new detail before SetItemInfo checks hidden-window preferred placement.
2. The strong-agent cart ownership decision is complete in PK37.05. `012B` clears rows/window while preserving entitlement/counters; local legacy actor options own removal. Same-index adds retain metadata; only `0121` updates count/weight; unknown `012C` results are ignored. Preserve these regressions. PK37.01–.04 remain open for broad transfer/request, inventory rollback, vending and trade review; do not close their rows from this leaf.
3. Resume bounded PK46.02 request/profile/send-failure and character paths; its 177 request rows remain research. NPC/status/mail have concrete source-ready contracts. PK09.02/.03 mapping/route checkpoints are complete; historical dirty-tree gates are superseded.

## Verified current checkpoint

All targets build deploy-off. **110/110 CTest in 89.32s**, **167 PASS packet records**, 20 commerce detail frames and zero commerce-detail QML errors. Prior rental (26 frames), special-name (20 variants/77 frames), card, ordinary-name, quantity, colour, cart, bag and minimap checks remain passing.

No discovery row is newly closed. PK09.04, broad PK37, independent PK48/49 and deployed provenance remain open. The whole packet plan remains unfinished.

## Working discipline

One coherent leaf and its production regression/evidence/commit at a time. Preserve unrelated work. Use exact-symbol searches and clip long/minified lines. Source disposition, implementation, headless replay, rendered UI and live/deployed acceptance are separate claims. Unknown deployed settings do not block source-selected 20080910 tests.

Offline tests use localhost, offscreen Qt and declared synthetic assets. The matching kit provides qoffscreen; do not modify installed deployment to fix a test. No installed-client launch/copy, live account transaction or external/Ref source modification belongs to these checks. Observe the active tool permission policy; do not request sandbox escalation when approval is disabled.

Maintain HANDOFF, STATUS and state.json active_step together; append diaries to PROGRESS/evidence. Never complete full PK48/PK49 or the whole-plan goal from a package checkpoint.
