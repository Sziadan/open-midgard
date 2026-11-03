# GR2 guardians and lava

Guardians, emperium, guild flag and treasure box use the original GR2 meshes,
embedded textures and skeletons. The class-to-model mapping follows
`HighPriest.exe.c` `C3dGrannyBoneRes` job/type tables (`1285â€“1287` guardians,
`1288` emperium, `722` guild flag, `1324` treasure box, plus Thor/battlefield
guardian variants). Movement, attacks, damage and death load separate
`model/3dmob_bone/{type}_{action}.gr2` banks when present; idle uses the
animation embedded in the model. Time advances at twice real time, and
orientation follows `Ref/GameActor3d.cpp`. Projected mesh bounds feed the
existing actor selection and overhead-marker paths.

## Independent reader

The client uses a C++ port of the MIT-licensed independent reader from
[granny-ro-js](https://github.com/Stitchuuuu/granny-ro-js). Sources and pinned revision
are archived in `Ref/JsGranny/`; native-port license notices remain in
`third_party/gr2-reader/`. The runtime implementation is under
`src/res/gr2/` (Oodle0, type-tree walk, IGC/BinkTC textures, mesh/skeleton/curve
extract). It decodes GR2 compression, embedded textures and animation curves
without a script engine, the Granny SDK or any proprietary library.

Asset decoding runs in background workers. Assets are preloaded during startup and
shared between guardian / emperium / flag / treasure variants. Resource reads and
GPU uploads stay on the main thread. C++ evaluates animation curves, composes
skeleton poses, skins and renders the meshes each frame. Each actor has its own
pose and animation clock.

## Build and deployment

Build with `cmake --build --preset build-mingw-qt-x64` (or the default x64 Release
dev-deploy preset). Deploy the executable with the usual Qt runtime and
`licenses/gr2-reader/LICENSE`.

The previous `granny2_x64.dll`, embedded JS reader and `granny-texture-decoder/`
files are obsolete. An existing original-client `granny2.dll` may remain in an
installation; the new client does not load it. The legacy `RO_ENABLE_GRANNY`
option is forced off.

## Lava

Texture set 4 uses the same 32-frame liquid animation and terrain eligibility
rule as water. As in `Ref/3dGround.cpp`, its vertex alpha is 255 and its RGB is
the map's ambient tint. Ordinary water retains alpha 144.

The supplied `mag_dun01` and `mag_dun02` maps declare lava. The supplied Thor
floors instead declare water type 0 at levels below all their terrain, so the
reference's eligibility rule produces no liquid surfaces there. No guessed
map-height overrides are applied.

## Checks

`python tools/tests/check_gr2_monsters.py` compiles the native C++ reader and
pose sampler without any SDK headers, libraries or Qt. It checks all six 3dmob
models (guardians + emperium/flag/treasure) and 12 external animation banks,
decoded texture hashes, reference pose snapshots at four animation times,
looped sampling, finite skinning, malformed inputs, job mappings, lava colour
and liquid animation. It also starts 12 workers together and compares 96
concurrent model decodes against serial texture bytes, matching the client's
parallel startup loading. Texture goldens match `third_party/gr2-reader` (JS) after
RGBA8888â†’A8R8G8B8 packing. Optional `python tools/tests/check_gr2_js_parity.py`
runs the archived reference directly in Node, without a generated bundle.
This optional development check is separate from the client build. Pose comparisons against the old
decoder differed by less than 0.001 per matrix element.
Pass `--sanitize` to the native check to trap undefined C++ behavior on the
same assets and concurrency workload.

The C++ port uses call-local wavelet scratch arrays; the JavaScript reader's
module-scoped scratch pools were safe only within an isolated sequential JS
engine. Sharing those pools between native workers corrupted textures. The
column-ring recenter explicitly writes zero where JavaScript read past a typed
array; native code must not reproduce that as an out-of-bounds memory read.
The arithmetic bit-reversal table is immutable and initialized at compile time.

GPU upload for GR2 textures skips magenta color-keying and default upscaling;
literal `0xFF00FF` texels from IGC must stay opaque.
Decoded RGBA is uploaded unchanged: near-magenta guesses and low-alpha wipes
remove legitimate texture colors and transparency and are not applied.
Fully visible models use the reference's alpha-tested, depth-writing geometry
pass even when textures contain alpha. The translucent effect pass cannot
correctly resolve the flag's overlapping cloth strips and rear frame.

The flag's second mesh is the guild-emblem surface. Its embedded green image
is an authoring placeholder; Ref replaces it with the guild emblem and uses
transparent pixels if none is available. The client currently omits this
surface because guild-emblem resource retrieval is not implemented. The banner
itself still renders normally. Flag idle/attack poses are checked against all
41 reference bones at four times; no mesh welding or animation changes are used
to hide rendering seams.
The opt-in native render fixture accepts `RO_HEAL_PREVIEW_MONSTER`,
`RO_HEAL_PREVIEW_MONSTER_STATE`, and `RO_HEAL_PREVIEW_MONSTER_AGE` to render and
mouse-pick an isolated guardian. `RO_HEAL_PREVIEW_MAP`, `RO_HEAL_PREVIEW_POS`, and
`RO_HEAL_PREVIEW_GROUND` exercise the installed map assets without logging in.
