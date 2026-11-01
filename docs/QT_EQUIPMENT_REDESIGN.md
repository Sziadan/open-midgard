# Equipment window redesign

Equipment uses the shared header, palette, font, borders and controls, with a
centered character preview and labeled equipment rows on either side.

The left order remains upper headgear, lower headgear, weapon, garment,
accessory 1. The right order remains middle headgear, armor, weapon/shield,
shoes, accessory 2; ammunition moves below accessory 2. The existing slot
assignment priorities and treatment of two-handed weapons remain intact.

Names come from `shopui::BuildItemHoverNameText`, the same name formatter as
the item hover bubble. This preserves refinement levels, card prefixes and
suffixes, repeated-card labels and slot counts. No name is deliberately elided:
the 624-pixel-wide window measures wrapped names using QTextLayout and expands
each paired row as needed. Labels and names have separate text areas. Empty
slots show their equipment category and Empty.

EquipmentLayout.h supplies the icon, name, row, preview and removal rectangles.
Native hover, drag, right-click and double-click use the whole equipment row,
including its name. Existing server requests and drag/drop destinations remain
unchanged. The shared falcon/cart/unmount button sits below the preview with
its current action label. Its position and native hit target share the layout.
Collapse/restore/close controls move with the wider window. Resizing for longer
names re-clamps the window to the client bounds.

Validation: MinGW Qt client build, focused ammunition/acknowledgement/relogin
regressions, and 16 Equipment previews (normal, long names, empty, collapsed at
100/125/150/200 percent). Expanded preview fixtures validate full text fit and
agreement between rendered row geometry and native hit rectangles. Normal and
extreme screenshots were inspected. Preview artwork is synthetic; the actual
game's character and icon renderers remain in use. Live gameplay verification
remains for the user.

Layout screenshots: `docs/images/qt-gui/equipment.png` and
`equipment-extreme.png` (placeholder artwork).

The title-bar grid button toggles compact mode (256 × 280). It hides names,
slot labels and the section caption, while preserving the preview, item icons,
slot order and removal control. Its icon changes to a list to restore full
names. Hover names, drag/drop, right-click and double-click remain available.
Collapsing and restoring the window retains the selected layout. Full and
compact rectangles come from the same shared layout builder, including native
hit targets. The preview suite now covers 28 cases: full/long/empty/collapsed and
compact/compact-empty/compact-collapsed at all four UI scales. The compact image
is `docs/images/qt-gui/equipment-compact.png` (placeholder artwork).
