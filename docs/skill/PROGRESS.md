# Skill system implementation progress

Plan: `docs/SKILL_SYSTEM_1TO1_IMPLEMENTATION_PLAN.md`
Rule: 1:1 with `Ref` and `HighPriest.exe`. No guessed asset names, no stubs.

Last updated: 2026-10-26
Current work: WP5A original Portal (effect 36) emitter/circle migration; ReadyPortal checkpoint `6c1dd1c` precedes this part.

Next action: port the reachable original Portal cylinder state, then remaining active circle emitters. Generic particle controls, map scheduling, resource/cache and full renderer/backend acceptance remain open.


## Original Portal emitter / circle — 2026-10-26

- Ref/RagEffect.cpp:6981-7039 (0x5FA2A0), original dispatcher id 36 -> ApplyPortal. Frame zero emits two PP_3DCYLINDER in order, with original duration, arc 36, radii 3.5/4, rotation speeds 4/5, heights 40/50, alpha 0/max 128/rise max*0.16666667 and duration-6 fade. No sound, RNG or live-actor position refresh.
- Every fourteenth frame emits the original circle, including frame zero after both cylinders: duration 30, y -1, inner width 8, X rotation 90, radius speed 1, original deceleration/fade and constructor alpha/max 254, pattern zero and one segment. Id 36 now uses the reference circle updater/renderer. All three texture requests match ring_blue bytes at 0x71943C.
- Synthetic map portal scheduling remains distinct. Cylinder updater/renderer parity is still open; this checkpoint certifies emitter fields/order and the circle binding only.

Validation: release build, six silent C++ suites / 515 checks (386 lifecycle/primitive, 71 rendering, 58 other), six Python tests and whitespace check pass. Eleven new checks cover ordering, distinct cylinder profiles, defaults/resource, stored origin, circle state, cadence and no RNG/wave. No media/live acceptance. Approved OldRO SHA-256: `BE2A03783A4B0E4627C8EA15C5C0C66C2471C314431F758E50F3274BAEBDC8DF`. WP5A and the overall plan remain incomplete.

## Original ReadyPortal circle migration — 2026-10-26

- Ported Ref/RagEffect.cpp:7066-7098 (0x5FA100), normal effect 35, independently of synthetic map effect 50002 and ReadyPortal2. Every fourteenth frame plays the exact wave at horizontal stored-origin/listener delta, then emits PP_3DCIRCLE with duration 30, y -1, inner size 8, X rotation 90 degrees, radius speed 1 and source deceleration/fade. No actor-position refresh or RNG draw is introduced.
- Restored constructor alpha/max 254, pattern/radius zero, render flag 1 and one initial rotated segment. Exact ring_blue texture and EF_ReadyPortal wave match executable bytes. Bound this caller to reference circle state/rendering and direct dispatcher handling.
- Validation: release build, six silent C++ suites / 504 checks (375 lifecycle/primitive, 71 rendering, 58 other), six Python tests, two-literal executable byte audit and whitespace check pass. Twelve new numeric checks cover cadence, stored origin, audio, defaults, segment initialization, expansion/width and fade. Corrected the test-body extraction boundary to keep the new helper from being included twice.
- Approved OldRO deployment SHA-256 matches `E82C68D3F5BC8C9E64C2D327CD1F89942186F2456AC7FAF39DB5ECBC38889C66`. No media/live acceptance. Synthetic map scheduling, other circle callers, resources and full renderer/backend parity remain open; WP5A and the overall plan remain incomplete.


## Obsolete heal / recovery emitter cleanup — 2026-10-25

- Deleted SpawnRecoveryHp/Sp and SpawnHealLight/Medium/Large, their Handler tags, declarations and fallback switch branches. Effects 312/313/325/331/332 are already handled by OnProcess2 before that switch through ApplyHeal/ApplyHeal2/ApplyHealTime. Removed the dead guessed texture alternatives, synthetic circle/cylinder/particle profiles and extra wave paths with those methods.
- Init retains specialized tier handling rather than selecting a generic STR handler. Existing cull radius 140 for those five ids is preserved explicitly after removal of the unused tags; no culling-profile change is intended.
- Validation: source comparison confirms OnProcess2 and all three active helper bodies byte-identical to the preceding commit; the tier-before-fallback guard remains present. Release build, six silent C++ suites / 492 checks, six Python tests and whitespace check pass. No new behavior tests were added for this deletion-only part. Approved OldRO hash matches `C14D6626C0537829877FAD5766A50EE7992AEE2E73E0F9A094CECC66F7CF67A3`.
- Active helper/primitive/resource/backend parity remains partial; this removes obsolete alternatives, not a full-healing acceptance claim. No media/live acceptance. WP5A and the overall plan remain incomplete.


## WarpZone2 circle / orbit migration — 2026-10-25

- Corrected ApplyWarpZone2's source timing: parent position copies only on frames 1/2, existing four-band casting profiles retain their exact constants, frame zero initializes the long-lived circle with a segment, and every tenth frame initializes an orbit sprite. Circle update/render now uses the shared reference path (UpdateReferenceCircleState, renamed from the Blessing-only name).
- Restored exact particle1 sprite/action resources, constructor alpha/max 254, looping divisor 4 and one segment. Preserved all five RNG draws, including the discarded integer-divided size draw. Radius uses integer division before scale; size retains rand()%10/10 + 0.69999999. The initial stored orbit offset remains separate from the current radial displacement.
- Added the reachable reference orbit update: radial/gravity/angular integration, integer-degree Y rotation, position before raw-radian Z rotation, alpha/segment store before animation. Source profile has no keyed radius schedule or change point; those other orbit branches remain open. Missing assets no longer use the legacy soft-disc substitute for effect 321.
- Validation: release build, six silent C++ suites / 492 checks (363 lifecycle/primitive + 71 particle/circle/queue/pass + 58 other), six Python tests, three-literal executable byte audit and whitespace check pass. Added 24 checks for branch/RNG order, constructor/casting/orbit/circle profiles, parent-copy timing, integrated motion, fade and reset, resource names and cleanup. No media or live acceptance was used.
- Approved OldRO deployment SHA-256 matches `ECE991B1D6B15FBBE6F34228EEA6BA3639B455AE1AB25C6B8FC7994D8AA9D7A6`. Other casting bodies/rendering, generic orbit controls, circle callers and full renderer/backend acceptance remain open. WP5A and the overall plan remain incomplete.


## Sight / Sight2 migration — 2026-10-25

- Replaced Sight/Sight2 gravity primitives, fireball/sight basename candidates and bitmap substitutions with original PP_3DPARTICLE profiles and exact case-sensitive Sight/Shadow resources. Both copy parent position and project x/y before the even-frame emission gate, preserving the stored z field. Sight emits a main sprite plus Shadow; Sight2 emits only its lower-alpha main sprite.
- Preserved the source's m_deltaPosAccel.y = 0.1 store as a separate vector field. Neither emitter sets pattern 0x400, so the value is inactive in their ordinary linear particle updates. Removed invented falling/gravity motion and orange tint; original alpha/size/divisor/repeat/fade/segment profiles now apply through shared state and per-clip rendering.
- Renamed the reconstructed SightState handler to Sight2 (original helper name), removing the invented actor-effect-state termination and 9998 frame reset. Effect 601 follows its original even-frame emission without either condition. Removed the unreachable invented SightAura emitter, handler and guessed assets.
- Validation: release build, six silent C++ suites / 468 checks (339 lifecycle/primitive + 71 particle/circle/queue/pass + 58 other), six Python tests, four-literal executable byte audit and whitespace check pass. Added 26 checks for type/profile, inactive acceleration, projection, integer-angle offsets, call order, motion/fade, no state gate/wrap, missing resources and resource cleanup. No media or live acceptance was used.
- Approved OldRO deployment SHA-256 matches `18CC1DE7B7769558A74D155FAF48DE822541AAC4B3FDA563BB65E287DD65C504`. Remaining primitive/circle callers, resource/cache and full renderer/backend acceptance remain open. WP5A and the overall plan remain incomplete.


## Circle renderer / triangle queues — 2026-10-25

- Added the reference Render3DCircle geometry path: primitive current matrix/width with segment position/radius/packed ARGB, two independent triangles per wedge, source vertex/UV order, quarter-step UV phase retained across segments, direct trig-table endpoint 360 and original integer next-angle stepping. All 50 original segment slots are now represented; invalid arcs and missing textures get no replacement geometry.
- Reference triangle projection uses half-screen offsets and device depth from original 0x4131B0, retaining negative/far depths with no synthetic 80-unit cutoff, clamp or half-pixel displacement. Blessing's circle dispatch reaches this path before legacy alpha rejection. Other emitters retain their old binding until their reference segment initialization/state is ported.
- Added the original RPTriFace queue selection: first-vertex ordinary alpha key, first-two midpoint for emissive/depth modes, original bit precedence, 0x100 depth-write and 0x200 depth-disabled passes. Depth-write lists were previously declared but never consumed; they now draw and clear. No-depth alpha/emissive lists sort independently, use original blend/depth states and clear, preventing stale emissive faces. Existing fog and other frame passes remain a separate parity audit.
- Validation: release build, six silent C++ suites / 442 checks (313 lifecycle/primitive + 71 particle/circle/queue/render-pass + 58 other), six Python tests and whitespace check pass. Added 36 numeric checks with production geometry/projection/queue/pass bodies and a silent render-device recorder. No media or live acceptance was used.
- Approved OldRO deployment SHA-256 matches `B90DAECA39DC589140FA8EC0418114431CE721C659EF8AD3913CDDD2BC055D41`. This closes Blessing's previous circle submission subset, not all circle bindings or full visual/backend acceptance. WP5A and the overall plan remain incomplete.


## Blessing emitter / state migration — 2026-10-25

- Replaced Blessing's guessed texture alternatives and sprite basename searches with all six exact sprite/action/wave/texture literals. Preserved zero-frame circle/sound, four bursts at 0/3/6/9 and recurring particles every fourth frame, including branch order and three angle/radius/speed RNG draws. Copies parent position on every call; sound uses horizontal listener delta and y zero.
- Bursts now store the original y -25 offset, constructor maximum alpha 254, repeat, one segment, divisor 2 and fade frame -1. Recurring particles use the original matrix-derived spawn offset and replacement X-minus-90 motion matrix, duration 80, size 0.5, divisor 4, fade 64 and twinkle mask 0x80. Their original action-before-sprite Get/Lock order is supported explicitly by SetParticleResources; existing callers retain sprite-first order.
- Extracted the reference ordinary alpha branch into ProcessReferenceParticleAlpha and added twinkle sign reversal at min/max before parent-shortened/fixed-frame fade overrides and final clamping. Blessing's PP_3DPARTICLE now uses the shared reference state and per-clip renderer.
- Ported Blessing's reachable PP_3DCIRCLE state separately: persistent parent height, source radius/size/matrix order, ordinary alpha and segment storage. The existing generic circle renderer still has differences, including blend/segment/submission behavior; this checkpoint does not claim full circle or Blessing visual parity.
- Validation: release build, six silent C++ suites / 406 checks (313 lifecycle/primitive + 35 particle-render + 58 other), six Python tests, six-literal executable byte audit and whitespace check pass. Added 26 checks for branch counts/order, resource order/profiles, motion, circle state, twinkle/fade precedence, audio and balanced cleanup. No media or live acceptance was used.
- Approved OldRO deployment SHA-256 matches `C723280A955DB9A28307FC49077948CE3AE36F5EAD9AE97A91A5C35F59C21FE3`. WP5A and the overall plan remain incomplete.


## Poison particle migration — 2026-10-25

- Effects 20/125 now emit the original particle3 sprite/action every fifth frame; effect 493 emits every third frame with its distinct speed/size profile. Restored angle/radius/speed/size RNG order, spawn Y rotation followed by replacement X-90 motion matrix, exact source arithmetic, maximum alpha 254, fade at 32, animation divisor 4, repeat and one segment. Removed particle2 fallback and poison-specific approximation diagnostics.
- Both helpers preserve the stored effect origin. Sounds use effect-minus-listener x/z with y zero and original volume/range/factor. Effects 20/125 play two ordered waves only on zero; effect 493 plays the assassin wave on 0/5/11/18/26/40. All four distinct sprite/action/wave literals match executable bytes.
- Ported effect 493's dispatcher tint: alternating magenta/even and white/odd through frame 80, low light bit set on even frames, no tint writes from 81 onward. Actor alpha and higher light bits are preserved; particle/sound emission still follows the helper's independent timing.
- Validation: release build, six silent C++ suites / 380 checks (287 lifecycle/primitive + 35 particle-render + 58 other), six Python tests, four-literal executable byte audit and whitespace check pass. Added 28 checks for profiles/RNG, matrix replacement, origin, sound ordering/coordinates/schedule, motion/fade, tint window and resource cleanup. No media or live acceptance was used.
- Approved OldRO deployment SHA-256 matches `390B43D6683A88A7B444E8BBA3C6D843F4E1760E19702F0360CF1A6E6C56B49A`. These callers use the complete reachable linear state; other generic particle patterns/emitters, original resource/cache and backend acceptance remain open. WP5A and the overall plan remain incomplete.


## Smoke / Torch particle migration — 2026-10-25

- Replaced Smoke's invented fade-in (alpha 0, maximum 100) with original constructor alpha/maximum 254 and zero alpha speed. Restored the exact X-angle/Y-angle/speed RNG order, source matrix composition, duration-based fade point, one segment, repeat and divisor 4. Copies parent position on every call, including non-emission frames.
- Torch now preserves constructor alpha/maximum 254, zero alpha speed and fade frame -1 while selecting sprite clip color through pattern 0x200. Invalid size/divisor values update the parent's parameters to 1; valid fractional size remains unchanged and the animation divisor truncates to an integer. Removed invented alpha rise and all name/color-based bitmap fallbacks.
- Both use exact executable sprite/action literals and sprite-then-action locks. Generalized the existing reachable linear particle updater as UpdateLinearReferenceParticle, bound to effects 33/44/47. Added normal effect-47 handler routing; retained map Smoke/Torch branches now preserve ids 44/47 so their primitives use reference dispatch. Existing map scheduling and name-based handler selection remain separate open work.
- Validation: release build, six silent C++ suites / 352 checks (259 lifecycle/primitive + 35 particle-render + 58 other), six Python tests, four-literal executable byte audit and whitespace check pass. Added 25 checks, including controlled RNG sequences and production matrix multiplication/rotation bodies. No media or live acceptance was used.
- Approved OldRO deployment SHA-256 matches `CF610FC3217FC2403C1CA21B6384274677DD15DF5E389740915F2A4D4DAAE7DE`. This completes the emitter/reachable particle state migration for these profiles; other particle patterns/emitters, resource/cache and backend acceptance remain open. WP5A and the overall plan remain incomplete.


## Ruwach particle migration — 2026-10-25

- Replaced effect 33's continuous-angle emitter and basename fallback searches with the original integer-degree trig calls, emission every third frame, two ordered PP_3DPARTICLE profiles, four exact resource literals and resource locking. All four names match bytes at the original executable VAs. Parent position is copied even on non-emission frames; no wave or RNG call is introduced.
- Ruwach's two primitives now use the reference particle state path: follow the persistent parent, apply original shrink/alpha rates, compute the frame-19 fade from current alpha, shorten fade when the parent ends, store the segment before animation, advance motion on primitive frame zero, and repeat at the action's actual motion count. Reference constructor defaults are restored locally (animation divisors 4/2, maximum alpha 254, repeat, one segment).
- Generalized the shared particle resource members/setter/render entry point so Ruwach and SuperAngel use the same per-clip path. Resource locking/release order remains sprite then action. Missing Ruwach resources retain reference routing without a soft-disc or alternate-name replacement.
- Validation: release build, six silent C++ suites / 327 checks (234 lifecycle/primitive + 35 particle-render + 58 other), six Python tests, four-literal executable byte audit and diff whitespace check pass. Approved OldRO deployment hash matches `B67EDD4979E4A17B2366AC3EBC82CA019CB1282425F9363F70D69A1A037643A5`. No media or live acceptance was used.
- The Ruwach updater ports the complete reachable pattern-zero state for these two profiles; it does not certify the other Prim3DParticle pattern/change-point branches, every emitter, the original indexed atlas/cache or the common rendering backend. WP5A and the overall plan remain incomplete.


## Per-clip particle renderer — 2026-10-25

- Replaced SuperAngel's baked-motion submission with the shared reference per-segment/per-clip particle path. Preserves explicit action/motion, segment position/size/packed color, logical half-resolution dimensions, zoom (including negative values), flip-specific bounds/culling and clip-plus-roll rotation. No fixed canvas, opaque-bounds crop or synthetic near-plane cutoff in this path.
- Ported sprite submission with half-pixel offsets, integer-degree rotation about each clip center, distinct slope-adjusted top/bottom reciprocal depth, source device-depth formula, padded texture UVs and the exact flag-5 additive blend rule. Indexed palettes and RGBA pixels upload separately from clip colors. Sprite resources own the resulting textures; physical upload extents adapt the current backend's optional texture upscaling to reference UV coverage.
- Corrected the previous checkpoint's clip-color interpretation: `m_pattern & 0x200` selects original clip ARGB. `m_renderFlag & 0x200` does not. Confirmed original renderer instructions at 0x645C33/0x645C45 (pattern offset 0x194), with render flags separately loaded at 0x645CA2 (offset 0x180). SuperAngel variant 10 now retains segment alpha with render flag 0x201. Segment initialization/shifting now stores packed wrapping ARGB.
- Validation: release build, six silent C++ suites / 300 checks (207 lifecycle/primitive + 35 particle-render + 58 other), six Python tests and diff whitespace check pass. New fixtures compile the production renderer/texture bodies, capture synthetic numeric submissions and uploads, and open no real media. Approved deployment hash matches `A48060118223A25FD18EAEC4F322E11C2D2E912861D2A5C277D52DA463EAEC53`.
- This completes the per-clip path for the current SuperAngel binding, not every original Render3DParticle caller or the original indexed atlas/cache. Existing resource decoding, GPU/backend acceptance and live skill verification remain open. WP5A and the overall plan remain incomplete.


## Primitive identifier reconciliation — 2026-10-25

- Replaced the shortened reconstructed EFFECTPRIMID with all 129 explicit entries from HighPriest.exe.h:2866-2997. Existing raw-id aliases now name the canonical enum entries. No invented selectors remain. This establishes the identifier contract; it does not establish all 129 handler bodies.
- Retained RSW/synthetic map emitters still use band payloads for PP_MAPPILLAR, PP_PORTAL and PP_WIND. Dispatch now distinguishes those world-effect payloads from skill/packet GI payloads so corrected ids preserve the existing map handlers. Their original behavior remains separate parity work.
- Standalone fixtures now extract the production enum. A Python regression compares every ordered name/value pair with the reference, checks the count and contiguous 0..128 values. Seven C++ checks cover the restricted world-effect payload distinction.
- Validation: release build, five silent C++ suites (263 checks, including 205 lifecycle/primitive checks), six Python tests and diff whitespace check pass. Approved OldRO deployment SHA-256 matches: `45366E7FC843C449394A93ADECB5FC4574EE8BBF759B8B62B4DA83F336E887D9`. No media or live acceptance was used. WP5A and the overall plan remain incomplete.

## SuperAngel emitter / update continuation — 2026-10-25

- Replaced guessed fake_angel/al_angelus resources and wing-texture fallbacks with seven exact sprite/action pairs, byte-verified at all 14 executable addresses. Resources load/lock in original sprite-then-action order and unlock on world deletion. Added reference CRes Lock/Unlock, including unlock timestamp behavior.
- Ported initial/trail emission and caller order for effects 338/582, listener-only wave for 338, slash bursts at 65/66, variant flags, head segment and process-driven alpha/drift/motion/fade rules. Removed PP_RADIALSLASH and PP_EFFECTSPRITE and their renderer branches; PP_SUPERANGEL is now the original 0x55.
- Uses the shared sprite compositor with explicit current motion and source render flags. That compositor's original per-clip geometry/color/blending semantics remain open; no complete SuperAngel visual-parity claim.
- Validation: 256 C++ checks (198 lifecycle/STR/primitive/resource), five Python tests, release build and approved deployment pass. Deployed SHA-256: `9F9190B7E4158FECA8A8DB8EA004A34C1FB6A384090608EF06824CF65A180C86`. No media was played/displayed and no live acceptance was performed.

## Job-level-up / slash continuation — 2026-10-25

- Effect 337 now calls the two original green JobLvUp50 emitters at angles 0/45, emitting only on frame 5. Only time zero processes STR, using offsets 0/0/0; the tier dispatcher ignores that helper's return as Ref does. Effect 590 calls the blue variant only at frame 15.
- PP_SLASH1 now has its reference id 0x4B and dedicated update/render geometry. Ported four-band growth/fade rules, variant flag behavior, three ribbons per band, center/side heights and alpha, world offsets, UVs and additive submission. Removed the JobLvUp50 radial-billboard substitute, guessed texture alternatives and obsolete handler tag. Other primitive selectors remain an incomplete migration.
- Validation: release build, five Python tests and all five silent C++ suites pass (209 checks, including 151 lifecycle/STR/slash checks). Approved OldRO deployment hashes match `4064EB704A40E41504CE6EE70DD4717642770C1E43F23848437AA4B3EBE1B9C1`. Common triangle submission/backend behavior is outside these geometry fixtures; no media or live acceptance was used.

## Primitive dispatch cleanup — 2026-10-25

- Removed unreachable duplicate Portal2/ReadyPortal2/WarpZone2 and begin-casting emitters, their Handler tags and obsolete Init overrides. Existing OnProcess2 cases 316/317/321/341 and the main casting/texture dispatch retain their existing ports.
- Removed invented PP_PORTALSTACK and PP_CASTINGRING4 selectors and their private renderer/update/setup paths. Preserved the remaining 25 selector values explicitly to avoid shifting other ids during this incremental migration. Two invented selectors still remain: PP_RADIALSLASH and PP_EFFECTSPRITE. The actual 129-entry enum/dispatch reconciliation is not complete.
- Validation: release build and all five silent C++ suites pass (192 checks); explicit OldRO deployment hashes match `E3743B5ED2B11AC9327D92E66F11EDF6987A59679D7B8AA0BD402772F74C7D3A`. This part removes unreachable code and adds no replacement behavior or media tests.

## STR projection/quad continuation — 2026-10-25

- Ported processing-time screen projection and independent render-time pixel-ratio projection, including their different screen offsets and depth outputs. Confirmed the overload difference in the executable because Ref/Renderer.cpp duplicates the wrong signature/body for the tlvertex overload.
- Ported both corner/UV branch pairs, texture cropping, reference angular conversion, byte-wrapping colors, unchanged blend values/material preset, depth and reciprocal-depth writes. Removed invented key-age suppression, frame upper clamp, blend remapping and handler-tag restriction. STR Render still excludes layer zero, as Ref does.
- Validation: five C++ suites pass with 192 checks (134 lifecycle/STR), five Python tests pass, release build and approved deployment pass. Deployed SHA-256: `A499855C162C73E408532EADA456B504E41023A24E0272A16F05692A6DFE43C4`. Tests use silent numeric fixtures; no media was opened or displayed. Source resource-loading/primitive gaps and live acceptance remain open.

## STR keyframe continuation — 2026-10-25

- Delta keys preserve blend modes and material preset, as the original processor does. Key transitions preserve the preceding key's age for animation mode 5.
- Modes 1-5 now retain Ref arithmetic, signed results and truncation. The executable confirms that reverse-loop mode reads the quotient left by a preceding layer and initially uses the z argument slot. Preserved this quirk instead of normalizing it to a repeated wrap. ProcessEZ2STR now accepts and retains the original x/y/z arguments for its renderer.
- Validation: five silent C++ suites pass with 178 checks (120 lifecycle/STR), five Python tests pass, release build and approved deployment pass. Deployed SHA-256: `644A159BA069B37396D3CB03F85DC9167C9C6A1B6243F222F6D7C8D704D0AE18`. No media/live acceptance was used. The renderer, primitive mapping and remaining work packages are still open.

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
