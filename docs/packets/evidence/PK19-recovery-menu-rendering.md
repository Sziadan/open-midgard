# Standing resurrection menu rendering

`packet_standing_resurrection_render_checks` uses the existing paired rendering harness. It routes a local `0080` death with one token 7621, then applies the real four-byte `0199` event PvP packet while the menu is open. The native pass draws the staged original `esc_01` through `esc_05` bitmaps into a DIB through the menu's own draw methods. The Qt pass loads the production `GameOverlay.qml` and captures the window after the gameplay adapter synchronizes the menu state.

The eligible menu has five entries and a height of 136 pixels. The original native fifth bitmap reads **Resurrection**. The Qt fifth row reads **Use Token of Siegfried** in the existing recovery and quit menu. After the restriction packet both surfaces show four entries and a height of 113 pixels. The native fifth-row region drops from 208 dark bitmap pixels to zero. Qt changes 4420 pixels in the fifth-row region and removes the recovery label. All four captures are written to the test output directory.

The application suite passed 165 checks and the standalone suite passed 473 checks. The [standing resurrection packet replay](PK19-standing-resurrection.md) covers the other individual map gates, the exact request, and the server-driven death-state transition. Live recovery remains open.
