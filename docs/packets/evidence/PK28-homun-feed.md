# Homunculus feeding result

The selected five-byte `022F` frame routes to `Zc_Feed_Pet` at `0x5A61C0`. A nonzero result byte at offset 2 sets session state 29. A zero result reads the 16-bit food ID at offset 3, formats message 591 with the item name, and sends the two selected notice routes with COLORREF `0x1E1EF5`. When the current homunculus actor exists, the handler sends actor message 21 with emotion 4.

The selected binary pushes `0x24F` at `0x5A63D1` and the notice color at `0x5A63FD` and `0x5A6414`. The replay checks the frame length, a nonzero success result, both notice routes and color, the actor message, and failure without an actor. All 164 application checks and 473 standalone checks passed.

The companion status window still needs to present the success state. Companion summon, removal, rename, and hunger notices remain open. The skill tree files match the baseline and the OldRO executable hash is unchanged.

[Companion status evidence](PK28-companion-status.md).
