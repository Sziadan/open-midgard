# Packet continuation handoff

**Active step: PK09.04.** Latest checkpoint: [PK09.11 detail selection](evidence/PK09.11-detail-selection.md), [contract](contracts/PK09.ITEM-DETAIL-SELECTION.md), [provenance](evidence/PK09.11-detail-selection.json). Dedicated commit: `packet(PK09.11): restore item detail toggle and forced updates`. Prior category, commerce-detail, rental, special/card/ordinary names, CID lookup and cart ownership checkpoints remain passing.

## Continue here

1. Continue [PK09.04](work-items/PK09.04.md) with missing NPC shop detail contexts. Current UIItemShopWnd is the source/catalog pane using m_shopRows; UIItemPurchaseWnd/UIItemSellWnd are selected buy/sell deal panes using m_shopDealRows. Primary right-clicks snapshot the correct row with Select. Their legacy val2=1 fallback reads the **post-hit** current item and also sends mode36/44/45 list actions; it is not a detached refresh gesture or necessarily the old selected item. Parent must trace that coupling before implementation. Never substitute a same-index inventory/cart record.
   PK09.11 now implements default Select equality/toggle and explicit ForceUpdate at the production C++ boundary. Select compares raw m_itemName/refinement/all four words, ignoring other fields and owner; visibility must be checked before MakeWindow. Native close/toggle store logical position and delete the owned window; reacquire pointers afterward. Hidden retained objects reopen. Preserve live versus snapshot binding and do not turn maintenance assignments into selection toggles. No current NPC force gesture is claimed implemented.
   Source details retain default/stored location and receive no cursor coordinates; full legacy rectangle/default geometry remains unaccepted. Category/type/radio review passes PK09.10: three normal UITab choices with initialized All groups, no connected subtype radio controls, selected11->2,7->4,12->4/5 and Cash18->Usable. Preserve wire-category versus visual-group boundaries. Saved-tab restart and remaining full item/detail/status/delta contexts remain open.
2. The strong-agent cart ownership decision is complete in PK37.05. `012B` clears rows/window while preserving entitlement/counters; local legacy actor options own removal. Same-index adds retain metadata; only `0121` updates count/weight; unknown `012C` results are ignored. Preserve these regressions. PK37.01–.04 remain open for broad transfer/request, inventory rollback, vending and trade review; do not close their rows from this leaf.
3. Resume bounded PK46.02 request/profile/send-failure and character paths; its 177 request rows remain research. NPC/status/mail have concrete source-ready contracts. PK09.02/.03 mapping/route checkpoints are complete; historical dirty-tree gates are superseded.

## Verified current checkpoint

All targets build deploy-off. **112/112 CTest in 99.44s**, **167 PASS packet records**, 23 detail-selection cases, eight detail frames and zero detail-toggle QML errors. Prior categories (24 probes/10 frames), commerce details (20 frames), rental (26 frames), special names (20 variants/77 frames), card, ordinary-name, quantity, colour, cart, bag and minimap checks remain passing.

No discovery row is newly closed. PK09.04, broad PK37, independent PK48/49 and deployed provenance remain open. The whole packet plan remains unfinished.

## Working discipline

One coherent leaf and its production regression/evidence/commit at a time. Preserve unrelated work. Use exact-symbol searches and clip long/minified lines. Source disposition, implementation, headless replay, rendered UI and live/deployed acceptance are separate claims. Unknown deployed settings do not block source-selected 20080910 tests.

Offline tests use localhost, offscreen Qt and declared synthetic assets. The matching kit provides qoffscreen; do not modify installed deployment to fix a test. No installed-client launch/copy, live account transaction or external/Ref source modification belongs to these checks. Observe the active tool permission policy; do not request sandbox escalation when approval is disabled.

Maintain HANDOFF, STATUS and state.json active_step together; append diaries to PROGRESS/evidence. Never complete full PK48/PK49 or the whole-plan goal from a package checkpoint.
