# Companion status snapshots

The selected 71-byte `022E` snapshot stores homunculus name, combat stats, HP, SP, experience, and other status fields using the widths in `Zc_Property_Homun` at `0x5AF000`. The fixed 24-byte name is copied as data. The `0230` update stores the selected subtype value, including full 32-bit hunger and intimacy and the actor gate for accessory state.

The 80-byte `029B` snapshot stores mercenary combat stats, name, level, 32-bit HP and SP, lifetime, faith, calls, kills, and range. The 8-byte `02A2` parameter update uses the alive gate and selectors in `Zc_Mer_Par_Change` at `0x5AF340`. The registered replay verifies the selected frame widths, signed fields, 24-byte name bounds, unknown selector handling, and the alive gate. All 164 application checks and 473 standalone checks passed.

The actor lifecycle and sprite reset, homunculus `022F` feed result, hunger notice, and companion UI refresh remain open. The selected companion Lua VM entry points remain inactive. The skill tree source and catalog match the baseline and the OldRO executable hash is unchanged.

[Selected identity evidence](PK27-companion-identity.md), [server registry ledger](PK01.02-server-registry-assignments.txt).
