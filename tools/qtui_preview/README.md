# Qt GUI rendering checks

This standalone Windows tool loads the real `GameOverlay.qml` with the real `QtUiState` class and synthetic character/options/status/shortcut data. It does not log in, read account data, or change client settings.

Run from the repository root with the installed MinGW Qt kit:

```powershell
cmake -S tools/qtui_preview -B build-qt-preview/verified -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH=C:/Qt/6.11.0/mingw_64 -DCMAKE_CXX_COMPILER=C:/Qt/Tools/mingw1310_64/bin/g++.exe -DCMAKE_MAKE_PROGRAM=C:/Qt/Tools/mingw1310_64/bin/mingw32-make.exe
cmake --build build-qt-preview/verified --parallel 4
$env:PATH = 'C:\Qt\6.11.0\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;' + $env:PATH
foreach ($scale in @('1','1.25','1.5','2')) {
    foreach ($case in @('normal','extreme','mini')) {
        & build-qt-preview/verified/gui_preview.exe $scale $case
        if ($LASTEXITCODE -ne 0) { throw "Preview failed: $scale / $case" }
    }
}
```

Additional viewport checks:

```powershell
& build-qt-preview/verified/gui_preview.exe 2 extreme small
& build-qt-preview/verified/gui_preview.exe 1 normal large
```

The small case isolates Basic Info at 640 x 480 with 200% UI scale; the large case renders the full sample at 1920 x 1080.

Outputs are in `build-qt-preview/renders/`; diagnostics append to `build-qt-preview/preview.log`. The tool uses Qt's offscreen platform and software renderer, loading the installed Segoe UI fonts explicitly.

Checks fail on a QML load error, runtime binding error, unsuccessful image capture, overlapping/out-of-bounds Basic Info action rectangles, or differing C++/QML header heights. The action rectangles come from the same `BasicInfoLayout.h` used for production hit testing and render data.

The extreme case covers long names/job titles, near-empty HP, empty SP, large resource maxima, capped and empty experience, a weight warning, and a large Zeny value. The mini case exercises compact Basic Info. Screenshots also show shared styling in Status, Options, tooltips, and shortcut rows.

These checks complement live gameplay checks; they do not exercise mouse dispatch through the game window or packets from a server.

For the redesigned Status window, run `gui_preview.exe <scale> status`,
`status-extreme`, `status-disabled` and `status-mini` at each scale. These load
the production `RoStatus.qml`, including long range/composite values and negative
bonuses. Each expanded fixture checks all six rendered upgrade buttons against
the native rectangles in `StatusLayout.h`, and verifies that every combat value
fits without truncation. The collapsed fixture verifies QML loading and capture.
Outputs use names such as `gui-1-status.png` and `gui-2-status-extreme.png`.

For Equipment, run `gui_preview.exe <scale> equipment`, `equipment-extreme`,
`equipment-empty` and `equipment-mini`. These share production EquipmentLayout.h
and EquipmentLayoutForQt.h to size and wrap full item names. Expanded fixtures
check every name for clipping and every row against its native hit rectangle.
The image provider draws numbered icons and a synthetic character stand-in;
these are layout checks, not game-asset rendering tests. The production character
preview renderer is unchanged. Test all four cases at 1, 1.25, 1.5 and 2 scale.
Also run `equipment-compact`, `equipment-compact-empty` and
`equipment-compact-mini` at each scale. These verify hidden name fields and
native row geometry in the smaller icon-only layout.

For the skill tree, run `gui_preview.exe <scale> tree` at each of the four scales.
It loads the shipped Acolyte catalog and production planner/layout, stages two
Heal points by targeting Decrease Agility, and highlights its prerequisite chain.
The tree fixture also checks all 175 job graphs for horizontal overflow,
overlaps and prerequisite ordering at viewport widths 264, 400 and 764.
Small trees shrink the displayed window to their content width.
Use `gui_preview.exe <scale> tree-gated` to render a Priest with only Novice and
Acolyte access exposed. Second-job icons show red lock markers while ordinary
prerequisite locks retain their usual color.
The image provider uses synthetic numbered icons: this verifies layout and
states, not loading actual game artwork. Tree previews use an 828 x 628 logical
viewport and output `gui-<scale>-tree.png`.
