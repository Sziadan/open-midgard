# Scoped server security source review — 2026-11-05

User explicitly requested review of stronger-agent deferrals. Scope: the local selected 20080910 source snapshot; hashes in `REVIEW-2026-11-05.json`. No external source edit, server build/restart, live packet, credential/account operation or exploit test was performed. This is source review and a bounded remediation specification, not deployed-server vulnerability verification.

## Slot move: confirmed unchecked read in failure reply

`src/char/char_clif.cpp::chclif_parse_moveCharSlot` rejects client-controlled `slot_before >= MAX_CHARS` but passes that same value as `index` to `chclif_moveCharSlotReply`. The helper reads `sd.char_moves[index]` before `socket_send`. `CharPacketDatabase` registers the request unconditionally in this selected source. The feature-disabled branch does not prevent the out-of-range branch that comes first.

Finding: out-of-bounds read and a value-dependent `moves` field in the failure reply; at least a potential process fault or disclosure of data outside the intended array. This source review does not prove exact accessible memory, reliable exfiltration, code execution, or exposure of the running server. The current client has no sender; that does not fix the server path.

Bounded external fix specification: guard the read in the reply helper itself, returning reason=failure and moves=0 for an invalid index; retain parser validation of source/destination and account slot limits. Do not clamp an invalid slot to another real character and expose that character's move count. An isolated copied/minimal harness can test indices 0, last-valid, MAX_CHARS and uint16 max under sanitizers, plus disabled feature, missing source and valid swap/empty destination. No hostile live replay is needed or authorized. Before any supported client slot-move feature, require server remediation evidence or explicitly disable that feature against the affected peer.

## SSO: confirmed missing dynamic-length lower bound

`src/common/packets.hpp::PacketDatabase::handle` converts remaining bytes to signed int16, requires remaining >= registered minimum, and checks remaining >= declared signed `packetLength`, but does not check declared length >= registered minimum. `PACKET_CA_SSO_LOGIN_REQ` is a 92-byte minimum prefix with signed int16 length. `loginclif.cpp::logclif_parse_reqauth_sso` computes `size_t token_length = p->packetLength - sizeof(*p)`, then passes `min(sizeof(sd.passwd), token_length+1)` to `safestrncpy` from the flexible token payload.

With a complete minimum prefix buffered but a declared length below 92, the existing checks permit handler entry. Underflow can make the token-copy bound depend on bytes outside the declared/received token; the 91-byte case can also wrap token_length+1 to zero and skip writing the password destination. The FIFO cursor subsequently advances by the invalid declared length via `RFIFOSKIP`; negative-to-size_t conversion and FIFO clamping are not substitute validation. This establishes invalid length arithmetic and logical read-boundary/stream-consumption defects. It does not establish a reliable remote exploit or that a particular deployment exposes SSO.

Bounded external fix specification: maintain receive/declared lengths in a checked nonnegative width; reject declared length < registered minimum or > protocol maximum before calling the handler; wait for a complete declared frame according to the existing socket parser contract. Validate token span before subtraction/copy, explicitly initialize the password destination for a zero token, and copy at most token bytes/destination-capacity-minus-one before terminating. Do not read a terminator beyond the token span. Isolated tests should cover 0, 91, 92, 93, maximum, negative declared values; short header/body; coalesced suffix; empty/nonterminated/long token; and exact consumption/no handler calls on malformed input. Do not change generic fragmentation behavior incidentally in the patch. No current-client SSO support should be added as a workaround.

## Ref 0x027C MAC bytes and current client

The Ref Login4 branch's non-value-initialized struct and unassigned MAC field cannot be treated as a required wire value. Current `CLoginMode::SendLogin4` value-initializes its packet and fills the MAC string; production captures verify its initialized output. Keep that behavior; do not reproduce uninitialized data for binary parity. This review finds no equivalent current-client uninitialized-MAC defect. Ref executable exact bytes remain unknown, a historical parity question rather than a blocker on safe initialized serialization.

## Rename and other scope

Legacy rename state pairing and current deletion-layout decisions are in the main review. Guild-emblem bounded inflate and pre-cache validation need malformed image/cache tests; no Qt decoder memory exploit was demonstrated. Auction escrow, complete mail transactions, every historical packet and all authentication algorithms were not security-certified by this bounded pass. PK48 as a whole remains open. External fixes above are prepared specifications only; ask for external patch authorization when that separate work is requested, not for another review of the already-confirmed source paths.
