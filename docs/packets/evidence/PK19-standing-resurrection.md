# Standing resurrection action

The [selected contract](../contracts/PK19.STANDING-RESURRECTION.md) traces the client menu through the server's two-byte `0292` request. The port adds the option to its existing recovery and quit menu. A dead character needs a usable inventory row for item 7621, and PK, siege, and event PvP each hide the action. Selection checks the same rules immediately before sending. Native rendering uses the selected `esc_05a/b/c.bmp` assets. Qt projects the extra entry through the existing menu model.

The client keeps its death and inventory state after sending. The server can reject silently, so the menu may be opened again while the character remains eligible. A successful resurrection uses the existing `0148` route. Loaded same-map warps now clear stale death state through the reuse path, following the selected client's `Initialize`.

The production replay starts with a local death packet, checks the visible recovery choice and its Qt projection, exercises each map restriction and token loss, then captures the literal `92 02` frame from the keyboard menu action on a loopback connection. It routes `0148` and checks the choice disappears. The transition replay covers the loaded same-map death reset.

All 164 application checks and 473 standalone checks passed. The existing Firewall fixture missed its timing assertion in the first parallel run and passed in isolation and on the full rerun. The installed OldRO executable and player skill tree matched the prior checkpoint. Live recovery and rendered menu acceptance remain open.
