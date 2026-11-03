# GR2 guardians and lava

Guardians use the original GR2 meshes, embedded textures and skeletons. The
class-to-model mapping follows `Ref/Session.cpp`. Movement, attacks, damage and
death load their separate `model/3dmob_bone/{type}_{action}.gr2` animations;
idle uses the animation embedded in the model. Time advances at twice real time,
and orientation follows `Ref/GameActor3d.cpp`. Projected mesh bounds feed the
existing actor selection and overhead-marker paths.

## Independent reader

The client embeds the MIT-licensed independent reader from
[granny-ro-js](https://github.com/Stitchuuuu/granny-ro-js). Source, pinned revision,
license and regeneration instructions are in `third_party/gr2-reader/`.
It decodes GR2 compression, embedded textures and animation curves without the
Granny SDK. No proprietary library, external decoder process or x86 compiler
is required to build or run this implementation.

Asset decoding runs in background workers using Qt's JavaScript engine, already
included in the Qt client. Assets are preloaded during startup and shared between
guardian variants. Resource reads and GPU uploads stay on the main thread.
C++ evaluates animation curves, composes skeleton poses, skins and renders the
meshes each frame; JavaScript runs only while loading assets. Each actor has its
own pose and animation clock. Shutdown joins workers before destroying Qt.

GR2 decoding currently requires the Qt-enabled build. Builds without Qt still
compile and report unsupported GR2 decoding rather than loading an SDK.

## Build and deployment

Build with `cmake --build --preset build-mingw-qt-x64`. Deploy the executable with
the usual Qt runtime and `licenses/gr2-reader/LICENSE`. The generated reader is
checked in: Node and esbuild are development tools only.

The previous `granny2_x64.dll` and `granny-texture-decoder/` files are obsolete.
An existing original-client `granny2.dll` may remain in an installation; the new
client does not load it. The legacy `RO_ENABLE_GRANNY` option is forced off.

## Lava

Texture set 4 uses the same 32-frame liquid animation and terrain eligibility
rule as water. As in `Ref/3dGround.cpp`, its vertex alpha is 255 and its RGB is
the map's ambient tint. Ordinary water retains alpha 144.

The supplied `mag_dun01` and `mag_dun02` maps declare lava. The supplied Thor
floors instead declare water type 0 at levels below all their terrain, so the
reference's eligibility rule produces no liquid surfaces there. No guessed
map-height overrides are applied.

## Checks

`python tools/tests/check_gr2_monsters.py` compiles the independent reader and
C++ pose sampler without any SDK headers or libraries. It checks all three
models and 12 external animation banks, decoded texture hashes, reference pose
snapshots at four animation times, looped sampling, finite skinning, malformed
inputs, guardian mappings, lava colour and liquid animation. Golden data was
captured from the original decoder before retiring it. Full pose comparisons
against that decoder differed by less than 0.001 per matrix element.
The opt-in native render fixture accepts `RO_HEAL_PREVIEW_MONSTER`,
`RO_HEAL_PREVIEW_MONSTER_STATE`, and `RO_HEAL_PREVIEW_MONSTER_AGE` to render and
mouse-pick an isolated guardian. `RO_HEAL_PREVIEW_MAP`, `RO_HEAL_PREVIEW_POS`, and
`RO_HEAL_PREVIEW_GROUND` exercise the installed map assets without logging in.
