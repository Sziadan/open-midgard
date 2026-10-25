# Skill system implementation progress

Plan: `docs/SKILL_SYSTEM_1TO1_IMPLEMENTATION_PLAN.md`
Rule: 1:1 with `Ref` and `HighPriest.exe`. No guessed asset names, no stubs.

Last updated: 2026-10-25
Current work: WP5A position propagation, primitive counter ownership, STR target anchoring and Entry2, partial. Incoming changes are saved in commit `aa6ea58`; subsequent continuation changes remain in the working tree.

Next action: reconcile the full reference primitive enum/dispatch, replace the remaining legacy emitters/renderers, and finish per-handler return/position semantics. Primitive counters now advance only in the outer processor. STR keeps the processing-time world anchor; full reference projection/render math and animation modes remain open. Then continue outstanding WP2/WP3/WP7 and acceptance work. The overall plan is not complete.

## Position/primitive continuation — 2026-10-25

- Corrected a missed ownership migration: Super Angel's asset-setup failure still directly deleted a registered primitive, leaving its pointer in the world list. It now detaches through messages 25/24 and the world deletes it. A production-body regression check covers rejection, replacement and exactly-once reclamation. Super Angel's existing guessed asset/fallback behavior is still an unfinished emitter port.
- Propagated persistent parent-position stores in 21 existing effect handlers (including both heal handlers), retaining frame-zero-only behavior for Teleportation2 and Chemical2's launch-time y subtraction. The complete dispatcher-position audit remains open.
- Ported effect 185's temporary master-position substitution/restoration around STR processing. STR now retains the master's world anchor from the accepted processing call, so rendering after restoration uses the target anchor. This does not complete reference projection/view/vertex math.
- Consolidated Entry2 under effect 344 and corrected effect 158 to the STR dispatcher. The Entry2 PP_HEAL emitter uses: four distinct GI ring profiles, four RNG draws, exact ring-blue texture and EF_Portal wave with relative y -150. Parent position follows the master on every call; emission occurs only at frame zero.
- Primitive handlers no longer increment the shared frame counter. `CEffectPrim::OnProcess` dispatches at the current frame, exits on failure, then increments exactly once and checks inclusive duration. Explicit handler counter changes are preserved.
- Validation: release build succeeds, five Python tests pass and all five silent C++ suites pass (162 C++ checks, including 104 lifecycle checks). Approved explicit OldRO deployment succeeds; source/destination SHA-256 both `E03683504DD9E6F6DED3B7E0B8ADC0948047151EFA3F89863D56BC328A8D5FA1`. No media or live session was used. WP5A remains partial.

## Position/control and STR continuation — 2026-10-25

- OldRO deployment is now authorized and succeeds through the approved explicit copy command. The latest release executable and `D:/Spel/OldRO/open-midgard.exe` have identical SHA-256: `4219F5D4CAC7574977672EA233A8D80765C8D42DAD967D69B69A2812FC4EB87E`. The sandboxed post-build step still lacks external-directory write access; the explicit approved copy completed after the build.
- Added persistent effect position/control storage, initialized from the master (or map source), independently of the live actor cache. Message 44 now represents all 65535/65534 control options and ordinary signed-count behavior, including option 243's third-parameter duration. Removed the invented unused level assignment.
- Primitive Init now adds the supplied delta and copies the parent position into both current and origin fields. The 15 reference position-copy primitive ids update before the skip-frame gate. These stores are ported; mapping every existing handler's movement into the persistent parent field remains open.
- STR restart now resets counters and active key cursors without clearing accumulated transforms. Processing checks resource availability and completed-layer count before touching draw/transform state. The terminal key draws on its transition call; the following call reports completion.
- Main-dispatch completion routing now honors the 106 direct STR-result consumers, leaves the 10 ignored-result cases running to their duration check, and restarts case 145 before the outer frame increment. Helper functions that internally call STR and have their own return/side-effect behavior still require individual ports.
- Validation: x64 release build and deployment succeed; five Python tests and five silent C++ suites pass (145 C++ checks, including 87 lifecycle/control/STR checks). No media or live server session was used. WP5A and the overall plan remain incomplete.

## Lifecycle continuation — 2026-10-25

- Replaced the skill effect's wall-clock catch-up loop and primitive/alpha/STR lifetime extensions with one handler dispatch, one counter increment and `stateCnt < duration`. Handler failure returns before the outer increment. Removed synthetic infinite-loop flags on real ids 321 and 601. RSW map emitters retain their existing scheduler separately.
- Primitives now join the world's object list, process/render there, and unregister from their parent when deleted. Parent destruction/message 25 detaches rather than deletes primitives; the world reclaims them. Actor exit/message 24 clears the master instead of allocating a permanent world anchor. Primitive outer lifetime is inclusive (`stateCnt <= duration`), with the reference life-99 gate exception for numeric ids 0 and 0x10.
- Ported `CAbleToMakeEffect::SendMsg` cases 23, 84, 85, 86, 87, 109 and 120. Actor messages now delegate to this base. Begin-cast message 85 preserves Ref's launch/remove/relaunch order, including the first positive-id double initialization; message 86 reuses an existing target effect. Duration zero is passed through. Removal handles id zero and only the first matching attachment.
- Effect message 14 copies both vectors. Removed invented message-109 effect behavior and message-14 Grim Tooth detachment/flight/duration rewriting. Grim Tooth now retains its Init origin, subtracts its two-unit direction step in Ref order, emits every third frame, consumes the two discarded RNG draws, and requests only `effect\stone.bmp`. Its downstream primitive update/render still needs the complete WP5 port.
- Validation: x64 release build passes; five Python tests and five silent C++ suites pass (98 C++ checks, including 40 lifecycle/casting/Grim Tooth checks). Production lifecycle/message/emitter bodies compile against controlled, silent doubles; no media was opened, played or rendered. Optional OldRO copy is still skipped with permission denied.
- WP5A remains **partial**: `EFFECTPRIMID` in src still has reconstructed values/collisions; Ref's pre-gate primitive position-copy list is not fully represented; handler failure/counter semantics and `ProcessEZ2STR` end-layer handling remain open. Effect messages 44's special control vectors also remain open. No RunningServer acceptance or skill-matrix verification was performed.

## Continuation review — 2026-10-25

- Preserved the incoming worktree changes. The initial release build failed at `ResolveTexturePath`: a public declaration conflicted with an anonymous-namespace definition. Added a public wrapper; the release executable now links.
- Corrected `cpp_string_literal` hex termination, effect-index schema, absolute source lines, shared goto targets and nested-switch handling. Reference dispatch inventory is **693** case labels (416/73/204), not the original plan's 733. `skill_behaviour_index.tsv` now includes enum/reserved-range inventory rows and excludes level/action/unit-job cases from skill-id cases. These are reference indexes, not implementation-coverage claims.
- `PlayWave` now centralizes skill sounds, with the reference integer distance/volume calculation and mode-2 no-provider fallback. Both current backends lack a Miles 3D provider and therefore use that reference fallback; provider-backed 3D parity remains open. Removed guessed WAV prefix retries, STR-derived startup sounds, synthetic bolt ids 50009–50011 and the Fire Ball-to-bolt-shower retargeting.
- Added packet framing/handlers for `0x01D3` sound, `0x01D2` combo delay and `0x01D1` Blade Stop. Server repeat sounds have their own list, separate from map ambience. Added session force-tick storage, used it for skill 272, and ported the three forced-motion branches from `CRenderObject::ProcessMotion`.
- Hit sounds now obtain the attacking actor's weapon, retain the low-word rule for jobs 12/4013/4035, and convert item ids through the session weapon table. Full attacked-handler parity is still open.
- `tools/` and `*.py` were globally ignored. Added narrow `.gitignore` exceptions for the required generators and parity checks so the generated sources are reproducible from a saved change.
- `generated/PARITY_AUDIT.txt` reports **30/41** required packet opcodes registered with a non-ignore handler. Still ignored: `0x01D0`, `0x01E1`, `0x01CF`. Still missing: `0x01CD`, `0x0215`, `0x0284`, `0x0191`, `0x043D`, `0x0162`, `0x0145`, `0x01B4`. Existing handlers also need body/receiver parity review.
- Five Python regression tests and four silent C++ suites pass (58 C++ checks: 13 volume + 24 sound/combo/Blade Stop + 8 playback dispatch + 13 forced motion). The C++ suites compile production function bodies against silent test doubles. They establish those branches' behavior, not a RunningServer skill-matrix result. No media was played, rendered or opened for inspection.
- Full generation was repeated with zero byte differences. `WP0_VALIDATION.txt` reports zero generated-table validation errors, 391 wave calls and 19 runtime-name calls. Six runtime-name sites now have source port anchors; thirteen still need producer ports. No runtime-name entry is evidence of an unrecoverable filename.
- Build succeeds with the plan's x64 release commands. The optional post-build copy to `D:/Spel/OldRO/open-midgard.exe` was skipped with `Permission denied`; the built executable remains in `build64/Release/`. No live skill session or full per-skill replay acceptance was run. `SKILL_MATRIX.md` is still missing.

## Status

| Package | Status | Notes |
|---|---|---|
| WP0.1 exe resolver | done | `tools/ro_exe_resolve.py`. Korean names come from exe bytes. Truncated `_hit_` symbols are paired with pushes inside `InitWeaponHitWaveName`. |
| WP0.2 generated tables | done | Run ` .venv/Scripts/python.exe tools/generate_skill_tables.py `. Outputs listed in `docs/skill/PARITY_LOG.md`. |
| WP0.3 acceptance | partial | Validation in `docs/skill/generated/WP0_VALIDATION.txt` reports 0 errors. 19 `PlayWave` sites build the name at runtime. Six have src port anchors in `generated/runtime_wave_callsites.tsv`; thirteen producers still need ports and remain in `BLOCKED.md`. Full asset/behavior acceptance remains open. |
| WP1 session tables | implemented; full parity review pending | Wave tables, effect-id tables, `IsSkillNotDelayed`, `GetMonsterSkillInfo`, `LaunchSkillSpecialEffect`, and skill 47 `CArrowEffect` are in. Action-info case labels match Ref. |
| WP2 actor messages | in progress | Hit queue, `Am_Action`, `Am_Skill`, `Am_Groundskill`, `SetAttrState`, and the status/multi-hit messages are in. The full `SendMsg` case list and `SetState` per actor class are still open. Forced-motion branches are now ported; the remaining actor-state/message audit is still open. |
| WP3 packet handlers | in progress | Damage, ground, state, cast-ack, `0x011A`, `0x01B9`, the unit packets, and `Zc_Notify_Act` follow the 2008 handlers. `0x0110` unfreezes the player and talks only when the result byte is 0; the chat text is not `MsgStr`. `Zc_Notify_Act_Position` is not on the packet_ver 23 stream. |
| WP4 ground units | in progress | `CSkill`, `CGraffiSkill`, `GetSkillByAID`, `MakeSkill`, `RemoveSkill`, and `RemoveSkillByJob` are in. |
| WP5 effect handlers | in progress | `Init` uses the duration table and the 167 `.str` names. `OnProcess` has the frame gate. `OnProcess2` covers ids 300, 301, 304, 305, 306, 308, 309, 310, 311, 312, 313, 314, 315, 316, 317, 318, 319, 320, 321, 325, 331, 334, 335, 341, 344, 368, 369, 371, and 372. Id 300 launches two `PP_CHEMICAL` prims. Id 309 launches `PP_FIRSTAID` and draws `RenderCloud` case 1. `Heal`, `Heal2`, `Exit2`, and `GlassWall2` use `PP_HEAL`. `Heal`, `Heal2`, id 310, and id 331 launch `PP_PARTICLE_UP`. Ids 318 and 319 use `PP_RECT_UP`. Id 334 uses `PP_BLIND`. Id 335 uses `PP_POISON`. Id 344 uses `PP_HEAL`. Ids 317 and 341 launch `PP_HEAL`, `PP_PORTAL`, and `PP_WIND`. Id 316 launches `PP_PORTAL`. Id 321 launches `PP_3DCASTING_4`, `PP_3DCIRCLE`, and `PP_3DPARTICLEORBIT`. The other 300–373 ids and all of `OnProcess3` are still the handler enum. Batches 5A–5H |
| WP6 sound | in progress | Shared PlayWave, integer attenuation, exact-name helpers, sound packets and six runtime-name anchors are in. Remaining handler waves, ProcessSound/ProcessDamageSituation and provider-backed 3D parity remain open. |
| WP7 adjacent visuals | in progress | Combo delay, stored force ticks, forced-motion processing and Blade Stop are in. Spirit/Devotion and other listed packets/visuals remain open. |
| WP8 skill matrix | not started | |
| WP9 acceptance | not started | |

## Resume

1. Read this file, then the plan section for the current package.
2. Do not re-guess Korean `.wav` / `.str` names. Use `tools/ro_exe_resolve.py` and `docs/skill/generated/playwave_callsites.tsv`.
3. Unresolved names go to `docs/skill/BLOCKED.md`. Source disagreements go to `docs/skill/MISMATCHES.md`.
4. Port notes go to `docs/skill/PARITY_LOG.md`.
5. Build, when code changes need it: `cmake --preset vs2022-x64 -DRO_ENABLE_DEV_DEPLOY=ON` then `cmake --build --preset build-release-x64`.
