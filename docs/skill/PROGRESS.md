# Skill system implementation progress

Plan: `docs/SKILL_SYSTEM_1TO1_IMPLEMENTATION_PLAN.md`
Rule: 1:1 with `Ref` and `HighPriest.exe`. No guessed asset names, no stubs.

Last updated: 2026-10-25
Current work: WP0 index/resolver corrections; WP2 forced animation and hit-wave selection; WP3/WP6 server sound; WP7 combo/Blade Stop. All remain parts of an unfinished plan.

Next action: finish WP5A's effect lifecycle and primitive parity, then the remaining WP2 actor states/messages and WP3/WP7 packet handlers. `CRagEffect::OnProcess` still uses elapsed-time accumulation, loop extensions and prim/STR liveness beyond the reference duration. Do not call the effect core complete because its duration table and skip-frame gate are present.

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
