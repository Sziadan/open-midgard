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

## Example: PK07 pet framing (bounded M task after strong approval)

1. Strong researcher checks the packed pet status structure under20080910, including the >=20081126 extra-field guard; verifies active emitter size35, current client37 and router behavior. Approve a35-byte fixture independently from server bytes. Record fields individually, IDs/units and layout guards.
2. Assign the implementer only the active profile's length/decoder/state handoff and associated regression fixtures. Do not ask them to infer all pet actor/UI behavior in this leaf. Confirm allowed files and exact state ownership from the contract.
3. Replay the35-byte pet packet followed immediately by a known packet through production framing/router. Assert both decoded states and no remaining stream displacement; repeat with fragmented header/body and invalid/truncated input. Check unsupported variants are not accepted under the wrong profile.
4. Commit verified leaf behavior with evidence and handoff. P04's framing subproblem is fixed only if tests pass; full pet parity remains PK27. Do not claim the entire pet feature is complete.

## Example: PK08 text-skill request (strong agent)

Inspect final active registrations for0190 and007E, not Ref's packet struct alone. The audit identifies0190 as a19-byte action request with parser offsets5,18;007E MoreInfo is102 bytes with offsets5,9,12,20,22. Revalidate those against effective server configuration. Annotate every byte, including unused/padding/text regions, from the actual server registration and parser. Replace incorrect90-byte expectations in old tests. Assert production sends exactly what the server parser consumes, including text bounds and destination fields. Replay unrelated action requests too, because changing a shared profile can break them. A test comparing serializer output with the same client struct proves nothing.

## Example: PK13 trade asymmetry (strong agent; external gate)

Read trade_tradeack's acceptance-distance failure path and each clif result recipient. Make a two-client timeline: requester pending, acceptor invited, accept arrives after distance changes, acceptor receives TOOFAR, requester receives no result, both server trade states clear. Confirm with source-backed replay. Record exactly what a client can observe. Client cancellation/timeout must not invent peer notifications or report a definitive server reason that was never received. Prepare a minimal server fix specification that sends the correct result to both peers and tests both states; external source modification/build/restart needs separate authorization. Split the client and server outcomes in status so a mitigation never closes the actual server defect.

## Example: weak-agent UI leaf

A strong agent first specifies exact Session field, server ack/reject meanings, Qt signal/window, accepted text and tests. Delegate only binding the approved field to the approved widget behavior with those tests. The weak agent cannot decide wire constants, encrypt storage passwords, reinterpret map flags, or infer missing feature state. On disagreement, capture expected/actual with source locations in BLOCKED and hand back to a strong agent.

## Expanding broad packages safely

Before implementing a family/review step (for example PK45.02), enumerate stable children such as PK45.02.001 etc., each tied to named canonical contracts/handlers. Add children to work-packages.json and state.json, dependencies if required, and regenerate cards. Keep the parent as an aggregation gate; it is complete only after all applicable children are verified and committed. A child must fit one coherent transition/decoder/transaction behavior; do not make a hundred-handler rewrite. Review coverage's provisional ownership, correct it with a classification/contract reference, and keep the discovery ID when splitting stage/profile identities.

Completion reporting must distinguish source classification, implemented behavior, offline replay, visible UI evidence and pending live acceptance. A row can be source-proven inactive without pretending a feature was implemented. No status may silently become ignored/unsupported to increase the completion percentage.
