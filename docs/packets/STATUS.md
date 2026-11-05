# Packet implementation status

**Active step: PK09.04.** [Bag/field evidence](evidence/PK09.04-bag-projection.md), [provenance](evidence/PK09.04-bag-projection.json) and [HANDOFF](HANDOFF.md) define current work.

- Qt worn-bag rows/counts now match native/primary Ref without deleting central records. Production snapshot/equip/unequip success/failure and ammunition exceptions pass; actual QML delegates and four frames are verified.
- Complete literal normal/equipment inventory fields, all-four-route category/order/empty/malformed checks and historical 0x01EF isolation pass. A guarded selected-header probe asserts all opcodes/sizes/offsets. Mapping/route leaves have stale tracker gates to finalize separately.
- Full deploy-off build and CTest pass 107/107 in 34.28s, with 161 packet records, 30 existing item-colour and 18 minimap frames retained. No installed client/live server was used.
- PK09.04 stays open for broader cart status/delta/failure/counters and applicable quantity/name/card/refine/expiry UI variants. PK09.05/.06, PK23.05 and selected PK14 checkpoints retain scoped proofs. No row is newly closed by this repair.
- NPC/status/mail implementations, PK46.02's 177 request rows/character paths, historical PK47 and independent PK48/49/deployed provenance remain open. The whole-plan goal remains active.
