# Qt GUI modernization plan

## Objective

Modernize the Qt GUI as one coherent interface, prioritizing Basic Info while applying shared window chrome and control styling throughout the client. Retain the compact Ragnarok character of the interface and its usefulness during gameplay.

Implementation completed on 2026-11-01. See `QT_GUI_MODERNIZATION_VALIDATION.md` for the implementation decisions, checks, screenshots, and live-session limits.

The shared header remains 17 logical pixels after layout exploration, keeping existing compact windows and their input regions aligned. Basic Info uses 320 x 196 logical pixels expanded and 320 x 58 collapsed.

## Findings from the current implementation

- `RoTheme.qml` already supplies a shared warm cream/slate palette, typography, radii, and control colors.
- `RoFrame.qml` supplies shared frames, title backgrounds, borders, and shadows. Titles and system buttons are still positioned separately in individual windows.
- `RoChromeButton.qml`, `RoTextButton.qml`, `RoField.qml`, and `RoScrollBar.qml` provide reusable controls, but other controls and colors are implemented directly in `GameOverlay.qml`.
- Basic Info occupies 280 × 120 logical pixels in its expanded state. The name and job each have only 76 pixels of text width; HP/SP, progression, footer data, and eight menu buttons compete for space.
- Basic Info repeats gauge construction and uses outlined text over saturated fills. Experience bars are only six logical pixels high and have little explanatory information.
- Header heights and input regions are partly fixed in C++: Basic Info and the base frame implementation use 17-pixel title bars. Updating the QML theme alone would leave mismatched input regions and content offsets.
- Qt renders much of the interface while existing C++ windows supply control rectangles and handle input. Layout changes must update the rendering and input geometry together.

## Design direction

Use a restrained light interface: warm grey surfaces, slate headers, dark readable text, and muted gold for selection and focus. Preserve red for HP, blue for SP, and distinct progression colors. Use fewer gradients, lighter borders, consistent spacing, and subtle shadows.

The result should look like a refreshed version of the existing client. Keep gameplay windows compact; avoid large dashboard-style panels or excessive empty space.

### Shared design rules

- Define named tokens for window surfaces, header colors, primary/secondary text, borders, selection, disabled states, tooltips, HP/SP, progression, and warnings.
- Introduce a spacing scale based on 4 pixels, consistent padding, and a small set of corner radii.
- Use Segoe UI consistently, with clear roles for window titles, ordinary text, captions, and numerical values. Avoid seven-pixel binding labels and heavy text outlines.
- Use consistent header height, title alignment, system-button placement, drag area, and collapsed-state treatment for every titled window. Start layout exploration at a 24-pixel logical header height.
- Standardize button hover, pressed, selected, disabled, and keyboard-focus states. Selection must remain clear without relying on color alone.
- Keep tooltips dark grey with readable text, consistent padding and borders, and placement that stays inside the viewport.
- Use consistent icon stroke weight, visual size, and alignment. Keep labels/tooltips available for unfamiliar actions.
- Use semantic variants of shared components where a window has different requirements; avoid per-window copies of headers or button styles.

## Basic Info redesign

Use Basic Info as the first fully composed example of the shared design system. Its header and controls must use the same components as other windows.

1. **Character identity:** give the character name more width, use a clear primary weight, and put the job beneath it in secondary text. Elide long names gracefully and expose the full value in a tooltip.
2. **HP and SP:** make both gauges equal in width and height, align their labels and values, and use calmer fills. Keep values legible at full, partial, and empty states. Indicate low HP through a warning treatment with a textual cue where appropriate.
3. **Progression:** align Base and Job level rows, give both progress tracks the same dimensions, and show percentages alongside them or in tooltips when space is limited. Level-cap and zero-progress states should look deliberate.
4. **Weight and Zeny:** place these in a clearly aligned footer with labels distinguished from values. Accommodate long currency values and show the existing weight warning without truncating essential data.
5. **Action buttons:** keep the familiar eight actions in a tidy two-column group, with consistent icon sizes, larger click targets, clear hover/pressed feedback, and readable action tooltips. Separate this group from statistics with spacing or a subtle divider.
6. **Compact mode:** preserve a genuinely compact alternative with readable essential status. Its header and collapse/expand control use the same design language as the expanded state.

### Basic Info action icons

Redraw the eight menu icons so their silhouettes communicate their actions at the actual displayed size. Use a single vector icon family, consistent stroke weight, optical sizing, and padding. Reuse these icons elsewhere for the same actions.

- **Character status:** a character/profile card with stat lines, distinguishing it from the social/friends icon.
- **Options:** a recognizable gear.
- **Inventory:** a backpack with a clear flap/pocket silhouette.
- **Equipment:** a chest armor silhouette, avoiding the current shield-with-cross symbol that can suggest healing or protection.
- **Skills:** an open spellbook with a simple spark, replacing the abstract connected nodes.
- **World map:** a folded map with a location marker, replacing the document-like outline.
- **Chat:** overlapping speech bubbles, visually distinct from the profile and friends icons.
- **Friends/messenger:** two people with a small conversation indicator, matching the window the button opens.

Show the action name in each tooltip, followed by a shortcut only when it actually works. Provide consistent hover, pressed, and disabled icon states. Check the silhouettes at normal and enlarged UI scales; avoid tiny decorative details and platform-dependent emoji or font glyphs.

The current `comm` (chat) button has an icon and an Alt+C tooltip, but no explicit action in `UIBasicInfoWnd::SendMsg`. Resolve its intended action during implementation; give an unavailable action a clear disabled state instead of presenting an active-looking button. Verify all eight icon/action/tooltip mappings together.

Explore an expanded footprint around 320 × 176 logical pixels, then settle dimensions against actual content and gameplay space. This is a layout starting point, not a fixed requirement.

## Implementation sequence

### 1. Inventory and shared geometry

- Inventory titled windows, title-free HUD elements, dialogs, lists, forms, tabs, gauges, and tooltips across `GameOverlay.qml` and other Qt overlays.
- Record duplicated styling, hardcoded colors, title offsets, and control rectangles.
- Establish shared logical metrics for headers and common controls, with matching C++ input geometry and Qt rendering data.
- Plan migration of all titled windows as a coordinated change. Do not ship a new Basic Info title bar while other windows retain a conflicting treatment.

### 2. Build the shared component foundation

- Extend `RoTheme.qml` with semantic color, spacing, typography, and geometry tokens.
- Refine `RoFrame.qml` and consolidate header title/system-button presentation into a shared header component or consistent frame API.
- Align the existing button, field, scrollbar, tab, and tooltip components to the same rules.
- Add reusable status/progress gauge and icon-action components so Basic Info, party displays, and other applicable windows can share their presentation.
- Register new QML components in the resource collection.

### 3. Apply the foundation throughout the GUI

- Migrate titles, system controls, and frame styling across all applicable windows together.
- Cover Basic Info, Status, Inventory, Equipment, Skills, Storage, Options, NPC dialogs, shops, party/messaging windows, item/skill descriptions, and front-end screens where these components apply.
- Update hardcoded content offsets, dragging, double-click collapse, hit testing, and saved-placement clamping for the revised geometry.
- Keep title-free elements such as shortcut bars coherent through shared surfaces, buttons, tooltips, and spacing.

### 4. Recompose Basic Info

- Implement the identity, resources, progression, footer, and action-button layout above.
- Redraw the Basic Info menu icon set in the shared icon component and verify each symbol against its actual action.
- Update `UIBasicInfoWnd.cpp` geometry and `QtUiStateAdapter.cpp` rendering data alongside QML.
- Reuse the same rectangles for rendering and input wherever practical; avoid a second independent set of QML button positions.
- Preserve menu actions, dragging, tooltips, collapse/expand, and saved positioning.

### 5. Consistency and usability pass

- Replace remaining one-off control styling with shared components or tokens.
- Bring the Keybinding tab, shortcut controls, list selections, form fields, and action buttons into the same system.
- Review each window beside Basic Info to catch inconsistent header heights, text sizes, borders, spacing, icons, and state feedback.

## Validation and acceptance

- Build using the MinGW Qt preset, then deploy the completed client to `D:\Spel\OldRO`.
- Review representative windows together at 100%, 125%, 150%, and 200% UI scale and at small and large supported viewport sizes.
- Check that click targets match visible controls after layout changes, including dragging, collapse/expand, tabs, scrolling, and tooltip anchors.
- Exercise long character/job/item names, large HP/SP and currency values, low/zero/full resources, weight warnings, and capped/empty experience states.
- Check old saved window placements so resized windows remain reachable.
- Confirm existing shortcut assignments and their tooltips still show the correct keys.
- Use focused regression checks for changed layout/input behavior; avoid unrelated gameplay changes.
- Capture before/after screenshots of Basic Info and a representative set of other windows for the final consistency review.

The work is complete when Basic Info is easier to scan, text and controls are readable, the shared title-bar treatment is applied throughout the applicable GUI, and all affected input regions remain aligned with their visuals.
