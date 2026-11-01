# Qt GUI modernization: implementation and validation

Implemented 2026-11-01 against `QT_GUI_MODERNIZATION_PLAN.md`.

## Design applied

- Shared warm grey surfaces, slate headers, restrained borders/shadows, blue focus/selection, semantic gauges, and dark grey tooltips.
- All `RoFrame` windows use the same `RoHeader`; title labels use consistent alignment and weight. Shared chrome buttons now use vector close/collapse/expand marks.
- Buttons, fields, scrollbars, and tabs share the revised theme. Options, Status, and chat tabs reuse `RoTabButton`; existing window variants inherit the shared palette.
- The header remains 17 logical pixels after layout exploration. This keeps compact windows usable while preserving their established title/content boundaries. Fifteen C++ header definitions now reference `UiMetrics.h`; the rendering check verifies that QML agrees.
- Basic Info is 320 x 196 expanded and 320 x 58 collapsed. Identity, HP/SP, Base/Job experience, Weight/Zeny, and the eight menu actions have distinct aligned areas.
- SVG icons identify status, options, inventory, equipment, skills, map, chat, and friends. They rasterize at the current UI scale and are reused by shared controls.
- Basic Info menu rectangles are defined once in `BasicInfoLayout.h` and reused by native hit testing, release handling, rendering data, and the preview check. Activations require a press and release on the same button. Resizing clamps the window back into the client area.
- Action tooltips include meaningful names and working shortcuts. Hovering character identity reveals the full name/job. Chat now opens the chat window; Alt+C and Alt+O match their advertised actions.
- Shared HP/progress presentation uses `RoGauge`, including party health bars. UI tooltip colors resolve through theme tokens.
- Nested control states in hidden windows are guarded against missing data; this removes the runtime binding errors found while rendering the complete overlay with empty hidden-window state.

## Checks completed

- MinGW Qt client build: `cmake --build --preset build-mingw-qt-x64 --parallel 6`.
- Standalone real-QML render check: normal, extreme-value, and compact fixtures at 100%, 125%, 150%, and 200% scale; all 12 cases passed without QML errors. Additional Basic Info rendering at 640 x 480 / 200% and the full sample at 1920 x 1080 also passed.
- Header metric agreement, button bounds, and button non-overlap checks passed.
- `tools/tests/check_ui_invalidate_source.py` passed.
- `git diff --check` passed.

Representative renders use synthetic data, not a live game session:

- [Normal state](images/qt-gui/normal.png)
- [Extreme values at 200%](images/qt-gui/extreme-200.png)
- [Compact state](images/qt-gui/compact.png)

The screenshot supplied with the request is the visual baseline. Full test reproduction instructions are in `tools/qtui_preview/README.md`.

## Live-session limits

Server-driven updates, actual window dragging/collapse/menu dispatch, old saved placements on different monitors, and simultaneous use of every window type have not been exercised in a live login session. The offscreen check validates rendering, state bindings, and shared geometry; the native client build validates compilation and resource packaging.

Large windows retain their existing sizing behavior at very high UI scale on small viewports. This change does not introduce a separate responsive layout for every legacy window.
