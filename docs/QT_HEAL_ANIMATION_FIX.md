# Heal animation: render-rate lifetime regression

The latest user log, `debug_hp_27452.log`, confirms that Heal's bands reach Vulkan
with a valid texture, nonzero alpha, the expected blend state and visible screen
coordinates. The earlier combat-text and blend-cache fixes were valid but did
not resolve the missing animation.

## Reproduced cause and fix

Heal effects 312, 313 and 325 were in the parent effect's render-only tick bypass
list. Their parent state counter therefore advanced on every rendered frame.
The PP_HEAL primitives advance only on logic ticks (`g_isSkipFrame`). At high
render rates, the 100-frame parent expired and destroyed the primitives before
their rising bands had grown visibly above the caster's feet.

These three rising-Heal parents now follow the same logic clock as their
primitives. Master-position bookkeeping still runs normally. Other effect tick
policies are unchanged. This is a targeted modern frame-rate adaptation; the
canonical render-only bypass inventory itself remains unchanged.

An earlier iteration also changed UV orientation and ring sampling in the
shared Heal/casting renderer based on a reading of Ref. The user reported that
both appearances regressed and selected the earlier `heal-both.bmp` capture as
the correct visual baseline. Those two visual changes have been reverted.
The high-FPS lifetime correction remains. A zero-span guard is retained to
prevent a stalled loop without changing valid rings.

The restored shared renderer retains its original circumference sampling and
U/V orientation for both Heal and white casting rings. A fresh native Heal
capture at the same animation phase differs from the accepted earlier capture
in only 52 of 1,228,800 pixel bytes. The experimental sprite-depth-plane
adjustment is absent; scene occlusion and billboard depth geometry are unchanged.

## Verification

- Production timing checks cover effects 312/313/325 at 1, 2, 3 and 8 rendered
  frames per logic tick. Each remains alive at logic state 60 and expires at
  state 100, independently of render rate. An unrelated render-ticked effect
  retains its previous behavior.
- 732 native particle-render checks pass, including the restored Heal strip
  orientation, nonzero green bands, original band sampling and white casting
  ring texture/blend behavior.
- The previous blend-state and combat-overlay-order checks still pass.
- A hidden native Vulkan fixture uses the installed `D:\Spel\OldRO\data.grf`,
  `new_zone01` models/ground and the male Priest sprite. It never enters a game
  mode or logs into an account.
- Before the timing correction, 180 rendered updates with one logic tick every
  three frames left the parent dead and the animation absent in the GPU capture.
  After correction it remains at state 60 and the green bands extend above the
  Priest. The same result holds at eight rendered frames per logic tick.
- GPU readback confirms visible bands in high-refresh captures. The current
  visual restoration was also checked with native Heal and white-casting
  captures on the same map and Priest sprite.

## Reproducing the GPU fixture

Build with the MinGW Qt preset, then run the built executable with these
process-local environment variables. Use an absolute writable output BMP path.
The window is hidden in this mode. Remove the variables for ordinary gameplay.

```powershell
$env:RO_HEAL_PREVIEW_OUT = '<absolute workspace path>/heal-preview.bmp'
$env:RO_HEAL_PREVIEW_ASSETS = 'D:/Spel/OldRO'
$env:RO_HEAL_PREVIEW_ROOM = '1'
$env:RO_HEAL_PREVIEW_GROUND = '1'
$env:RO_HEAL_PREVIEW_PLAYER = '1'
$env:RO_HEAL_PREVIEW_REFRESH = '3'
Start-Process -FilePath '<absolute workspace path>/build-mingw-preset/open-midgard.exe' -WindowStyle Hidden -Wait
```

Refresh is clamped to 1..16. Three and eight exercise high-refresh scheduling;
the default one renders an isolated early animation frame. The Vulkan capture
path is opt-in and only requests transfer/readback resources for this fixture.
Normal gameplay does not allocate capture buffers.

For the white casting-ring fixture, set `RO_HEAL_PREVIEW_CAST=1` and leave
`RO_HEAL_PREVIEW_REFRESH` unset. This renders effect 58 at the default early
animation phase instead of Heal. Clear all preview variables after testing.

The final MinGW Qt build passed and was copied to `D:\Spel\OldRO\open-midgard.exe`.
Build and deployed SHA-256 match:
`A341B89ACE107DF69C028A8A45C2FF511A0B3A70C4D25CB2D967D06151CEDE78`.
