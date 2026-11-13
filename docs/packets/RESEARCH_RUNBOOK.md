# Source research and bounded execution examples

Use from the repository root in PowerShell. These are source-navigation commands, not substitute runtime tests. Read the matching code and call sites, not only search excerpts.

```powershell
$packetServerRoot = 'C:/Users/sziad/source/repos/rathena-master'
git status --short
git rev-parse HEAD
git -C $packetServerRoot rev-parse HEAD
git -C $packetServerRoot status --short
rg -n 'PACKETVER|PACKETVER_SAK_NUM' "$packetServerRoot/src/custom/defines_pre.hpp" "$packetServerRoot/src/config/packets.hpp"
rg -n -i '0x01f2|guild_memberlogin_notice|ZC_UPDATE_CHARSTAT' "$packetServerRoot/src/map" src Ref/GameModePacket.cpp Ref/packet.h
rg -n 'HandleIgnorePacket|RegisterPacket|SetPacketSize' src/gamemode/GameModePacket.cpp src/network/GronPacket.cpp
```

The audit compiler probes demonstrate effective configuration and packed layouts. Inspect `tools/audit_packets.py` before copying its commands. Follow definitions/includes and final registrations; a grep hit from an inactive guard proves nothing. For a feature, find all server emitters, all parser registrations, their callers and associated module result branches. Find the Ref receiver and follow all Session/actor/UI calls. Record exact field/branch evidence in a contract; the plan's seed opcodes are navigation aids, not a replacement for that work.

## Running Windows MinGW tests

Prepend the runtime directory for the compiler recorded in the test build's `CMakeCache.txt` to `PATH` in the same PowerShell invocation as CTest. The Qt MinGW kit used by `build-mingw-preset` and `build-recovery-unit-review` keeps `libgcc_s_seh-1.dll`, `libstdc++-6.dll`, and `libwinpthread-1.dll` in `C:/Qt/Tools/mingw1310_64/bin`. The standalone suite also needs this directory even when the application build already has its DLLs beside the executable.

```powershell
$env:Path = "C:\Qt\Tools\mingw1310_64\bin;$env:Path"
ctest --test-dir build-recovery-unit-review --output-on-failure
```

## Example: PK07 pet framing (bounded M task)

1. Check the packed pet status structure under20080910, including the >=20081126 extra-field guard; verify active emitter size35, current client37 and router behavior. Derive a35-byte fixture independently from server bytes. Record fields individually, IDs/units and layout guards.
2. Implement only the active profile's length/decoder/state handoff and associated regression fixtures. Do not infer all pet actor/UI behavior in this leaf. Confirm allowed files and exact state ownership from the source-backed contract; independent review follows the implementation run.
3. Replay the35-byte pet packet followed immediately by a known packet through production framing/router. Assert both decoded states and no remaining stream displacement; repeat with fragmented header/body and invalid/truncated input. Check unsupported variants are not accepted under the wrong profile.
4. Commit verified leaf behavior with evidence and handoff. P04's framing subproblem is fixed only if tests pass; full pet parity remains PK27. Do not claim the entire pet feature is complete.

## Example: PK08 text-skill request (source-led implementation)

Inspect final active registrations for0190 and007E, not Ref's packet struct alone. The audit identifies0190 as a19-byte action request with parser offsets5,18;007E MoreInfo is102 bytes with offsets5,9,12,20,22. Revalidate those against effective server configuration. Annotate every byte, including unused/padding/text regions, from the actual server registration and parser. Replace incorrect90-byte expectations in old tests. Assert production sends exactly what the server parser consumes, including text bounds and destination fields. Replay unrelated action requests too, because changing a shared profile can break them. A test comparing serializer output with the same client struct proves nothing.

## Production-path regression rule from the 2026-11-04 review

Before marking a leaf implemented, identify the actual caller that owns the transition and test a meaningful sequence through it. PK14's position-preservation helper passed while `HandlePartyList` cleared its inputs before calling it; changing marker ownership also left old teardown code behind. When moving state, enumerate every writer, reader, invalidator and lifetime boundary, and update them together within bounded repair leaves. Test a roster refresh and a map/session change, not just an assignment/reset helper. For receive failures, prove the consuming mode's status/cleanup/transition in addition to the socket's error enum. Use a small production-used seam where full app initialization is unsafe, and describe the remaining UI boundary honestly.

For current executable work, follow [HANDOFF](HANDOFF.md) and [review repair cards](evidence/REVIEW-2026-11-04.md). Preserve existing dirty work, serialize shared-file edits, and do not defer a source-resolved repair merely because independent PK48 review is pending. Add precise blockers only for the affected path. Keep HANDOFF as one current summary and append session history to PROGRESS/evidence.

## Example: PK13 trade asymmetry (source investigation; external server fix)

Read trade_tradeack's acceptance-distance failure path and each clif result recipient. Make a two-client timeline: requester pending, acceptor invited, accept arrives after distance changes, acceptor receives TOOFAR, requester receives no result, both server trade states clear. Confirm with source-backed replay. Record exactly what a client can observe. Client cancellation/timeout must not invent peer notifications or report a definitive server reason that was never received. Prepare a minimal server fix specification that sends the correct result to both peers and tests both states; external source modification/build/restart needs separate authorization. Split the client and server outcomes in status so a mitigation never closes the actual server defect.

## Example: bounded UI leaf

Specify the Session field, server ack/reject meanings, Qt signal/window, accepted text and tests from authoritative sources. The implementer must not invent wire constants, encrypt storage passwords, reinterpret map flags, or infer missing feature state. On unresolved source evidence, capture expected/actual with source locations in BLOCKED and stop only that dependent work; otherwise implement and verify the bounded leaf. Independent review follows the implementation run.

## Expanding broad packages safely

Before implementing a family/review step (for example PK45.02), enumerate stable children such as PK45.02.001 etc., each tied to named canonical contracts/handlers. Add children to work-packages.json and state.json, dependencies if required, and regenerate cards. Keep the parent as an aggregation gate; it is complete only after all applicable children are verified and committed. A child must fit one coherent transition/decoder/transaction behavior; do not make a hundred-handler rewrite. Review coverage's provisional ownership, correct it with a classification/contract reference, and keep the discovery ID when splitting stage/profile identities.

Completion reporting must distinguish source classification, implemented behavior, offline replay, visible UI evidence and pending live acceptance. A row can be source-proven inactive without pretending a feature was implemented. No status may silently become ignored/unsupported to increase the completion percentage.


## Exact checkpoint and review discipline — 2026-11-05

Review R4 proves why a dirty-tree build is not a committed checkpoint: staged hunk placement left syntactically invalid functions, while unstaged corrections made the main build pass. Validate the exact candidate source/header/test closure in a workspace-local snapshot, then commit it without unrelated hunks. Inspect function boundaries and test invocation, not only hunk content. An omitted dependency cannot be excused by a current binary timestamp.

The primary-agent source/design decisions in evidence/REVIEW-2026-11-05.md are actionable. Do not repeatedly ask for a stronger reviewer after the exact question has been adjudicated. If blocked, cite the still-missing technical input; otherwise implement one bounded state transition and production regression. Source-default/explicit-profile tests can proceed while deployed settings remain unknown. A package-owner label is allocation, not an approval barrier. No source-only review certifies unimplemented UI/resource behavior or hundreds of unresolved historical identities.
