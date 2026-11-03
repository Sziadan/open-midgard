# Contract: <permanent ID>

Status: DRAFT / STRONG_REVIEW_APPROVED. Discovery parents: AUDIT-0x.... Owner step: PKxx.nn. Reviewer and approval commit: required before M/L implementation.

## Identity and applicability

- Stage, direction, packet date/profile, opcode, layout variant; raw handshake if applicable.
- Server repository revision, dirty status, effective defines and source hashes. Probe command/output artifact. Explain whether deployed-server binary provenance is verified.
- Final packetdb registration with exact symbol/line/guard and overridden earlier assignments.
- Emitter/parser/caller and feature-module symbols/lines. Prove active/inactive reachability; declaration alone is insufficient.
- Ref symbols/lines, all relevant branches/constants and explicit wire conflicts resolved in favor of server.

## Exact bytes

For **every field** list name, byte offset, width, signedness, endianness, units, valid range, sentinel, ID namespace and meaning. Use a numbered list, not an unexplained struct dump. Include headers, declared versus actual length, minimum/maximum length, optional guards, padding, record size/count formula, string encoding/termination/truncation and overflow handling. Document each unknown value as unresolved; do not guess.

Provide an independently server-derived hex fixture and annotated expected values. Cite the emitter/packed layout and final parser offsets used. Specify exactly how the client serializes every outbound byte; accepted server offsets must match. Unsupported server variants must not be sent.

## Required behavior

Enumerate preconditions and state transitions: before, event, after, emitted reply, visible result. Include success and each server rejection code, cancel, retry, duplicate, missing actor, map change, disconnect/reconnect, partial/delayed/out-of-order updates where protocol allows them. Explain ownership, pending action IDs, modal/input lock lifetime and stale-response policy. Specify authoritative versus transient display state.

List exact Session/actor fields and Qt model signals/windows/actions to change. UI must fit existing modern style. Specify anchor/layer/input behavior where relevant. List resource creation/replacement/destruction. No placeholder effects or generic feature approximations.

## Implementation boundary and tests

Allowed files and forbidden adjacent changes. Approved implementer tier. Exact independent fixtures and production entry points. Expected field/state/reply/UI assertions for each transition. List source branch-to-test mappings and acceptance thresholds. Separate offline and live gates. Unresolved questions block implementation of the affected leaf.
