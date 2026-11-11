# Companion identity packets

`0230` is a 12-byte status frame. The homunculus identity comes from the unsigned GID at offset 4 when byte 3 is zero. The source receiver is `Zc_Change_Mer_Status` at `0x5AEE40`. The bundled server writes the subtype and ID in those positions. Other status subtypes remain open.

`029B` uses the selected 80-byte layout and the unsigned mercenary GID at offset 2. The source receiver `Zc_Mer_Init` at `0x5AF1E0` also clears the previous mercenary skills. The other initialization fields remain open. [Server registry ledger](PK01.02-server-registry-assignments.txt).

The selected executable's `CMercenaryAI::Open` at `0x5D58D0` and `Proc` at `0x5D5900` return zero. `Close`, `SetEnemyID`, and `RegisterFunc` return immediately. Source modifier branches require an opened Lua VM. The scripts in `Ref/dhxj` document an alternate implementation. The checkpoint preserves the selected executable's inactive VM while restoring the companion identity used by the packet and skill paths.

The replay frames adjacent `0230` and `029B` packets and dispatches both through the registered receiver. It checks high-bit IDs, subtype isolation, rejection of the short mercenary layout, and skill-list reset. All 164 application checks and 473 standalone checks passed. The skill tree source and catalog match the baseline and the OldRO executable hash is unchanged.
