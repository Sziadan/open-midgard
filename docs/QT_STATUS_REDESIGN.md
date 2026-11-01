# Status window redesign

The Status window uses the shared RoFrame header, Segoe UI typography, palette,
buttons and corner styles. Its expanded layout is 432 × 252 logical pixels.

The six attributes retain the order: name, current value, plus/minus, bonus,
upgrade button, cost. Separate aligned fields make bonuses and costs readable;
positive bonuses use the shared accent color and negative bonuses use the warning
color. Upgrade buttons remain visible, with unavailable upgrades disabled. The
existing point-cost and maximum-stat checks still gate native clicks.

Combat values use a separate two-column grid, with labels above values. This
reserves the full cell width for Matk ranges and composite values. Numbers fit to
their field for extreme values without entering a label or neighboring cell.
Available status points appear in a footer.

Removed the page tabs, their native hit targets, unused second-page assets and
Qt page state. The single view retains move, collapse, restore and close controls.
Upgrade geometry comes from StatusLayout.h for native input and Qt render data.
The legacy drawing fallback also uses the single-view layout.

Validation: the MinGW Qt client preset builds. The standalone preview tool
renders 16 fixtures: normal, extreme, disabled and collapsed at scales 1, 1.25,
1.5 and 2. All load without QML errors; expanded fixtures verify native/rendered
upgrade rectangle agreement and that all combat values fit without truncation.
Normal and extreme screenshots were visually inspected. The preview uses
synthetic data, including six-digit composite values, large Matk ranges and a
negative bonus. Live input and server confirmation remain for user testing.

Images: `docs/images/qt-gui/status.png` and `status-extreme-200.png`.
