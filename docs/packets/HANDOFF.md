# Packet continuation handoff

**PK14.06 caller checkpoint verified — 2026-11-05:** [actual map/character lifetime repair](evidence/PK14.06-transition-caller-replay.md), deploy-off app/replay build PASS and 158 PASS records. Same-map reuse, active-loading guard, same-map server move, reconnect outcomes and actual character notification/exit now have production caller proof. Earlier caller-seam deferrals below are historical. Active PK14.04 for remaining integration/rendered acceptance, then PK23.05 and bounded PK09/PK46 work. The separate PK09 cart wear/bind omission is now [fixed and independently verified](evidence/PK09.04-cart-wire-fields.md), with 159 PASS records; broader PK09 acceptance remains open.

Review/cleanup checkpoint — 2026-11-05. **Active step: PK14.04.** Read [current review and exact candidate results](evidence/REVIEW-CLEAN-2026-11-05.md), [provenance](evidence/REVIEW-CLEAN-2026-11-05.json) and the assigned work item. Runtime commit `fde621a1006593c894c273ff0151930985e45c1c` restores the buildable roster checkpoint, preserves the accumulated reviewed runtime/build/replay changes and fixes incremental offline/online marker resurrection. PK14.07 and the scoped PK14.05 checkpoint are complete. Full PK14, PK09 and PK48 are not certified.

PK05.05 is now implemented and verified: [terminal map receive repair](evidence/PK05.05-map-disconnect-repair.md), [input/binary provenance](evidence/PK05.05-map-disconnect-repair.json), 157 PASS records. The exact production preamble gates world/audio work, stops dispatch on explicit Switch/Quit and handles unexpected closure once. Earlier receive-seam deferrals are superseded.

## Continue here

1. [PK14.04](work-items/PK14.04.md): integrated regression and native/Qt rendered evidence where safely available. PK05.05 and PK14.06 headless repairs/callers are now verified. Keep missing visual acceptance explicit; headless token tests are not rendered frames.
2. [PK23.05](work-items/PK23.05.md): implement reviewed cast announcements with exact message parameters and production actor fixtures.
3. Continue bounded PK09 field/lifecycle acceptance, preserving the independently verified cart WearState/bind repair; then PK46.02's per-request crosswalk. The 177 request rows remain open research.


Independent NPC/status/mail leaves have concrete [stronger-review contracts](evidence/STRONG-UNBLOCK-2026-11-05.md) and can proceed if a specific repair dependency is technically blocked. PK22.03's timed-status inactivity and PK24.05's preserve-331/332 disposition are source/applicability checkpoints, not missing selected runtime features. Their leaf commits record those bounded dispositions separately; do not repeat the completed source search.

## Verified checkpoint and preservation

The staged runtime tree `b06e5f025b53f6a9b92cfc849bc06e949dc6e806` was exported into `build-review-clean-snapshot` and built freshly in `build-review-clean-build`. Both app/replay targets passed with deployment OFF. Authorized local-loopback CTest passed 1/1 in 4.65 seconds, 156 PASS records, including the new incremental marker sequence and selected four-route inventory/cart isolation. All 1,471 exported files remained unchanged and 19 runtime/build/test inputs match staged blobs after normal Git CRLF conversion. Evidence-only commits do not change these compiler inputs. That exported clean baseline is historical. Current `build-mingw-preset` app/replay binaries were rebuilt for the cart field repair with deployment OFF; focused authorized CTest passed 1/1 in 4.88 seconds, 159 PASS records. Use PK09.04-cart-wire-fields provenance for their current hashes/source inputs.

The user's clean-slate request authorized this one broader accumulated checkpoint. Preserve its history. Future implementation returns to one coherent leaf and its tests/commit at a time; do not mix unfinished leaves merely to make a build pass. A new dirty tree is work to inspect and isolate, not a generic blocker. Historical deferral notes are superseded where the current review supplies a result.

Reproduction commands, cached dependency hashes, binary hashes and JSON records are in the current review. Qt/MinGW bin directories must be on PATH for the isolated replay. The sandbox may require local-loopback permission. Do not copy to or run the installed client, initialize live accounts, patch/restart the external server or send hostile server requests as part of these offline checks. Rendered/native evidence, historical identity work, deployed-server provenance and full PK48/PK49 remain open.

Maintain this handoff, STATUS and state.json active_step together. Append diaries to PROGRESS/evidence. Do not infer production/UI parity from a source scan, opcode mention, test name or package ownership.
