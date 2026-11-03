# Map weather rendering

Einbroch uses weather effect 515 (Cloud variant 3), in addition to its RSW's
18 chimney-smoke emitters (effect 44, interval 40). Its weather launches 80
cloud primitives with four independently fading GI bands each, as in Ref.

`Ref/dhxj/RagEffect2.cpp::RenderTeiRect` names the separate parameters `PW`
(blend mode) and `TN` (texture index). `RenderCloud` passes PW from its pattern
switch, TN from the current band's `flag1[0]`, and F1=1 for world coordinates.
The decompiled `Ref/RagEffect2.cpp::RenderCloud` confirms these arguments.

The port mistakenly used TN as the blend switch and always selected texture 0.
Einbroch's pattern 12 requires inverse-source-alpha blending, but TN=0 made it
additive, so overlapping fog patches accumulated light. `CloudRenderStyle`
now names PW `blendMode`; each band independently selects its TN texture with
bounds checking. This fixes the shared renderer for all 13 routed primitive
types, preserving additive styles where Ref calls for them.

The fade envelope matches Ref: Einbroch rises to alpha 169, holds until its
randomized 300–499 tick threshold, fades by one per tick, then respawns near
the player. The persistent weather controller does not accumulate new cloud
primitives each cycle. No map-specific brightness multiplier or lifetime cap
is needed.

Run `python tools/tests/check_cloud_blending.py` for production draw-path checks
of every pattern, nested pattern-9 variant, and independent texture indices;
it also checks repeated weather cycles for all eight variants, including
Einbroch's exact fade envelope. `python tools/check_cloud_render_parity.py`
compares shared routes and geometry against Ref.

The isolated native fixture accepts `RO_HEAL_PREVIEW_MAP=einbroch`,
`RO_HEAL_PREVIEW_GROUND=1`, `RO_HEAL_PREVIEW_WEATHER=515`, and optional
`RO_HEAL_PREVIEW_WEATHER_FRAMES` to render installed fog assets at a chosen age.
It uses a fixed camera and does not enter gameplay or log in.
