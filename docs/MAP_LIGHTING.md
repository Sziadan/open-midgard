# Map lighting parity

Lighting comes from each map's RSW sunlight angles, diffuse RGB and ambient RGB,
plus its GND lightmaps and surface colors. The supplied Comodo RSW specifies
diffuse `(0.8, 0.4, 0.5)`, ambient `(0.3, 0.4, 0.8)`, longitude 45 and latitude
65. These values load correctly and produce its blue/purple nighttime palette.
The reference does not force Comodo into the server-controlled night mode.
Its map-specific weather effect is fireworks.

## Model shading

`Ref/3dActor.cpp` (`C3dNode::UpdateVertexColor`) calculates shaded RSM model
colors from `ambient + max(dot(normal, sunlight), 0) * diffuse`, capped at 1.
Flat and smooth normals are transformed through the node matrix and normalized.
The mesh normal generator follows the reference's RSM winding:
`cross(v2 - v1, v2 - v0)`. Reversing these edges makes sun-facing surfaces use
ambient-only lighting even during daytime. The regression test invokes the
production generator for both flat and smoothed normals and checks that an
up-facing Prontera surface reaches full daytime brightness.
Recomputing a flat normal from transformed triangle edges reverses its direction
on mirrored model instances and differs from the reference.

`C3dNode::Render` then multiplies cached model colors by the current environment
tint. `Ref/RagEffectPrim.cpp` (`CWeather::Process`) defines that tint per channel
as `1 - (1 - ambient) * (1 - diffuse)`. Unlit RSM materials also receive this
environment tint. Previously the client discarded the directional term and
rendered cached colors without the environment tint, making buildings and
foliage too bright, especially in Comodo and other dark maps.

## Ground and colored lights

`Ref/3dGround.cpp` applies environment tint while drawing the authored ground
corner colors. The client now retains those original colors and applies the
current tint at draw time, so night/day transitions affect the terrain without
rebuilding it.

`Ref/Renderer.cpp` uses texture operation 18 (`MODULATEALPHA_ADDCOLOR`) for its
combined lightmap stage. The final RGB is `base * lightmap.alpha + lightmap.rgb`,
saturated to the output range. Alpha carries baked shadows; RGB carries authored
colored lights. The former shader used only alpha and discarded those lights.
The corrected operation is supported by the legacy render state and the D3D11,
D3D12 and Vulkan shaders. The shadow-only modulation mode is retained separately.

The Vulkan fragment shader is checked in. After changing its source, regenerate
with `python tools/generate_vulkan_world_shader.py` using the Vulkan SDK's DXC;
the tool also validates the resulting SPIR-V when `spirv-val` is available.

## Validation

`python tools/tests/check_map_lighting.py` checks Comodo lighting math, reflected
normals, environment tint, night changes and shader state selection. Add `--gpu`
to run isolated native captures on D3D11, D3D12 and Vulkan and verify actual
shadow-only and colored-light pixels against the reference equation. This uses
the local MinGW toolchain, Pillow and assets in `D:/Spel/OldRO`, without login.

The map fixture supports `RO_HEAL_PREVIEW_MAP`, `RO_HEAL_PREVIEW_GROUND` and
`RO_HEAL_PREVIEW_NIGHT`. Add `RO_HEAL_PREVIEW_DAY_AFTER_NIGHT` to settle a full
night transition and return to daytime before capturing, checking restoration
of both model and terrain colors. Comodo, Prontera, Niflheim and iz_dun03 are checked to
cover dark towns, daytime maps and underwater colors; the night fixture exercises
the same weather transition as normal gameplay.
