# Actor-targeted skill self-selection

The latest inspected gameplay log, `D:\Spel\OldRO\debug_hp_43620.log`, shows
Heal (skill 28) entering ActorTarget mode at levels 10 and 5, but no subsequent
skill-chase or cast requests. The saved shortcut level was reaching the targeting
state correctly.

`CWorld::FindHoveredActorScreen` excludes the local player for normal world
picking. Both the pending spell cursor and spell click handler also used that
picker, so hovering the caster produced the Forbidden cursor and clicking could
not arm a cast.

The picker now has an explicit `includeLocalPlayer` option, defaulting to false.
Actor-targeted spell cursor/click paths opt in. The local hit region comes from
the same `BuildBillboardRenderEntry` projection as the displayed sprite. Normal
movement, NPC interaction, ground-targeted skills and ordinary hover retain the
default. Existing self target-ID resolution and selected-level casting remain
in use.

The regression test compiles the production picker body with deterministic
projection inputs. It checks self selection, unchanged normal movement picking,
sprite boundaries, hidden/dead players, projection failure, missing player and
other actor selection. Build/test target: `skill_self_target_checks` in
`tools/tests`. These tests do not replace a live Heal cast against the server.

The MinGW Qt preset build passed and was copied to
`D:\Spel\OldRO\open-midgard.exe`, with matching SHA-256:
`1644531FEE692D45EB5DC0064840AB0A3BD1F049D2BF0AF1159B573E7A5F609A`.
