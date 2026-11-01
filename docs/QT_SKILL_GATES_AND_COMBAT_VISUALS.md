# Server job access and combat visuals

## Job skill access

rAthena's `pc_calc_skilltree_normalize_job_sub` counts spent normal skill points,
subtracts Novice points, and checks the character's saved job-change levels
before enabling later job trees. `pc_calc_skilltree` and `clif_skillinfoblock`
then expose the normalized tree in the normal skill-list packets.

The 20080910 client protocol does not provide the saved `change_level_2nd` value
or an exact remaining first-job point count. One absent skill cannot prove that
the point restriction is active: it may have unmet prerequisites or level gates.

The exported catalog now retains each skill's originating job group. Inherited
overrides keep the earlier origin rather than misclassifying a first-job skill
as belonging to an advanced job. The Qt controller recognizes a group when the
server advertises an ordinary upgradeable skill or a confirmed capped skill
from it. Quest and grant-only skills do not provide evidence. Learned skills
below their cap with the server upgrade flag disabled do not provide evidence.

Unexposed groups remain visible but cannot reserve draft points. The UI explains
that earlier-job points must be applied first. Draft allocations alone never
manufacture a server group unlock. After Apply and the server skill-list update,
later-job allocations become available. Within an exposed group, recursive
prerequisite drafting continues to work. Apply still checks each actual server
upgrade flag before submitting the next request.

This conservatively follows server availability rather than guessing a fixed
49-point requirement; characters can have different saved job-change levels.
It cannot identify every reason for a hidden group or display an exact count.

## Combat text and Heal

`debug_hp_33416.log` confirms successful Heal responses for 1,680 and 880 HP and
the Heal effect's wave launch. The actor-target packet path is working.

The modern frame path submitted message effects before `CView::OnRender` had
produced them. The following frame cleared that queue. Submission now runs after
world rendering and before `DrawScene`, restoring the same-frame path for healing
digits, damage digits and Miss sprites. Qt event processing remains ahead of
world geometry submission to preserve the existing texture-lifetime fix.

Heal/HealLow rings now use the existing scoped ground-effect depth clearance,
preserving their scene depth tests while clearing biased model floors. Other
primitives retain their previous depth policy. A bounded `[HealRender]` diagnostic
records ring alpha/height, texture presence and submitted face count at state 8.
This helps distinguish missing animation geometry from scene occlusion in the
next live check; no authenticated gameplay session was available during this fix.

The installed `data.grf` contains `data\texture\effect\alpha_down.tga`, a valid
64 x 64 image with nonzero color and alpha. No game assets were changed.

## Validation

- Production model tests: a visible Magnificat group cannot reserve points while
  only Acolyte access is exposed; drafting prerequisite points does not enable
  it; a server group unlock enables it.
- Catalog checks: Novice Basic, Heal and Magnificat retain origins 0, 4 and 8
  respectively; all 175 server job trees remain included.
- Frame-order regression: clear, world production, message submission and scene
  draw occur in that order; both digit and sprite queues are consumed.
- Native particle-render checks: 731 pass, including two green Heal bands with
  nonzero alpha, vertical extent, texture and blend mode. These test geometry,
  not GPU visibility in a live scene.
- The legacy shared CTest configure encountered an unrelated existing extraction
  boundary error for `ResolveSkillPacketActor`; focused checks were compiled and
  run directly with the relevant production bodies instead.

Live verification is still needed for server tier unlocks, the restored combat
text and Heal's ring animation.

The MinGW Qt build passed and was copied to `D:\Spel\OldRO\open-midgard.exe`.
Build and deployed SHA-256 match:
`13C1B05B891BB7F74948FAB1036B599D602FAC8311C4979DB06A153F409619E3`.

## Follow-up: Heal animation and job lock marker

The user confirmed that combat text and job gating work. The subsequent
`debug_hp_53624.log` confirms two active Heal bands, alpha 40, nonzero height,
a valid texture and 76 submitted triangles at animation state 8. Therefore the
missing animation is beyond the emitter/geometry stage; the previous floor
clearance change did not resolve its visibility.

The renderer had a concrete blend-cache mismatch: after the blended scene pass
it restored the device to SRCALPHA/INVSRCALPHA, but retained the previous cached
values. If that pass ended in ONE/ONE, the first Heal AlphaOP face requested
SRCALPHA/ONE yet skipped setting destination ONE, because the stale cache already
claimed it was active. The cache now tracks the restored state. A focused test
compiles the production pass, fails before the fix, and passes afterward for
both the first additive face and mixed normal/additive faces. The existing 731
particle geometry checks also pass. Actual live animation visibility remains a
user verification step; this test proves the state mismatch and its correction,
not a completed live GPU capture.

A bounded `[HealDraw]` trace records the actual Vulkan texture/shader/blend/depth
state alongside projected vertex data for the first four qualifying Heal faces.
This complements `[HealRender]` if further live investigation is needed.

The icon's small x now uses the shared red warning color when `jobBlocked` is
true. Ordinary prerequisite locks keep their existing color. The `tree-gated`
QML fixture renders the Priest/first-job restriction for visual verification.

Follow-up MinGW Qt build passed and was deployed to `D:\Spel\OldRO\open-midgard.exe`.
Matching build/deployed SHA-256:
`C62995B1F93BE424EB58372A1B91FB4E3A744BC158EF9F91DC148849CCEE0A13`.
