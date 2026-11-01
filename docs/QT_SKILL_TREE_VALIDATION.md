# Qt skill tree implementation and validation

The Qt Skill Tree now displays the current job's complete connected graph,
including inherited, locked, passive and quest skills. Empty viewport space pans
the graph in both axes; the mouse wheel scrolls vertically. The shared Qt frame,
palette and controls keep it consistent with the other windows.

Cards use 104 x 98 logical pixels (about 33% less area than the initial
132 x 116 cards). Layout is bounded by the viewport width: branching skills
wrap downward, terminal skills stack in their prerequisite's column, and
standalone skills occupy a separate vertical column. Small trees shrink the
frame to remove empty columns. Long dependency links bypass intermediate cards
through column gutters. Native hit testing and QML consume shared card/control
geometry from `SkillTreeModel.h` and `SkillTreeGeometry.h`.

Icon clicks stage one point, recursively choosing the first unmet usable
prerequisite. Arrows select the cast level independently. Apply sends one
increment at a time and waits for both the expected level and point balance;
Reset discards only unapplied points. Closing keeps an idle draft and does not
interrupt an in-flight queue. Job/character and incompatible server-state
changes clear the remaining draft. A timeout never retries a request.

Special family/marriage (`IsWedding`) and soul-link (`IsSpirit`) skills are
marked `grantedOnly` from the server database. They remain in the shipped catalog
but are omitted from the visible tree until a positive learned level arrives
from the server. They cannot receive draft points. Gaining or losing a granted
skill rebuilds the visible graph and invalidates an outstanding draft. This
fixes Call Baby appearing on a new Novice simply because it is listed in that
job's database tree. Ordinary locked job skills and quest skills retain their
existing visibility.

## Data provenance and regeneration

- Source checkout: `C:\Users\sziad\source\repos\rathena-master`.
- Renewal is enabled in `src/config/renewal.hpp`; the inspected pre/post headers
  and project flags do not override it. Packet profile: 20080910.
- Inputs: `src/common/mmo.hpp`, `src/map/script_constants.hpp`,
  `db/re/skill_db.yml`, `db/re/skill_tree.yml`, and their `db/import` overrides.
- Export includes all 175 job trees (8,122 job/skill entries), including jobs
  beyond the client's current gameplay support. Their presence adds catalog
  coverage, not new protocol, sprite, or gameplay support.
- Inheritance matches the server's `SkillTreeDatabase::loadingFinished`, using
  each listed parent's declared entries and applying exclusions/own overrides.
- Embedded JSON records schema, ruleset, packet version, source license and a
  SHA-256 over the six copied input files. Git revision lookup was unavailable
  under the filesystem sandbox; the content hash identifies the source inputs.
- The catalog is derived from rAthena, licensed GPL-3.0-or-later. Keep its
  attribution and license obligations when redistributing it.

Regenerate from the repository root:

```powershell
.venv/Scripts/python.exe tools/export_skill_tree.py C:/Users/sziad/source/repos/rathena-master
.venv/Scripts/python.exe tools/tests/check_skill_tree_catalog.py
```

The exporter uses Python's standard library. Invalid prerequisite references,
caps, cycles, unknown jobs and unsupported tree fields fail export. No server
checkout or external JSON file is needed at runtime; the JSON is a Qt resource.

## Completed checks

- MinGW Qt client build passed.
- Copied `build-mingw-preset/open-midgard.exe` to `D:\Spel\OldRO\open-midgard.exe`
  and verified matching SHA-256:
  `044AD149894221B55F83278E0E75FF658CA87317C5E17FBECB6BC4A4BAF0225F`.
- Standalone production C++ model checks passed: exact five-click Acolyte
  sequence, one-point budget, caps, quest/job gates, invalid graphs, selected
  levels, Reset, stable layout, both acknowledgement orders, no duplicate sends,
  partial confirmation, missing balance, unexpected level change and timeout.
- Catalog/exporter checks passed: coverage including fourth jobs, Acolyte
  requirements, no Acolyte skills in Thief, imported caps/metadata/quest flags,
  and rejection of unknown schema fields.
- Grant visibility regression checks passed: Novice Call Baby is tagged from
  the server flags; zero/absent learned levels hide it, a confirmed learned
  level reveals it without permitting allocation, and removing the grant hides
  it again. Partial import overrides preserve special flags. Filtering ungranted
  nodes leaves valid ordinary prerequisite graphs across all 175 job trees.
- Actual QML render checks passed at 100%, 125%, 150% and 200%, with no QML
  binding/load errors. Fixture icons are synthetic; game artwork is loaded by
  the production image provider with grayscale variants and a missing-art
  fallback.
- All 175 catalog trees passed production layout checks at viewport widths
  264, 400 and 764: no horizontal overflow, no overlapping cards, and every
  prerequisite positioned above its dependent skill. Focused checks cover
  vertical terminal/standalone columns, wide roots wrapping downward, stable
  draft positions and icon/arrow hit regions.
- Shortcut use now honors the saved selected level, clamped to confirmed learned
  levels, through the existing self/actor/ground/delayed targeting paths.

Preview: `images/qt-gui/skill-tree.png`.

## Live gameplay checks still needed

No authenticated server session was used for these checks. Verify real mouse
dispatch, drag/drop to shortcuts, level-specific casts, actual icon assets,
Apply/Reset and server confirmations in game. Apply uses the existing one-point
protocol and cannot be atomic; already confirmed allocations cannot be undone.
The server remains authoritative for special skill restrictions. Unsupported
future jobs remain catalog-only. Layout currently uses the stable layered
fallback; authored job layouts and inherited-branch headings are future polish.
