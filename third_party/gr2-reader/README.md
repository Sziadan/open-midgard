# Native GR2 reader attribution

Vendored from [granny-ro-js](https://github.com/Stitchuuuu/granny-ro-js),
commit `955189e060bf38270cbc3ea8cba610e98c7f62dc` (MIT; see LICENSE).
This is an independent implementation, not the Granny SDK.

The original reader and its adapters are archived in `Ref/JsGranny/` for
reference and optional development parity tests. This directory retains the
license notices required by the native C++ port under `src/res/gr2/`.
The client neither embeds nor executes the archived reader. Node, esbuild and
script engines are not required to build or run GR2 support.

Deploy `LICENSE` next to the executable (`licenses/gr2-reader/LICENSE`). No
external decoder process or proprietary runtime is used.
